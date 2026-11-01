#include "depth_tracker.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <magic_enum/magic_enum.hpp>
#include <vulkan/vulkan_core.h>

#include "config/config_manager.hpp"
#include "core/logger.hpp"
#include "core/service_locator.hpp"
#include "hooks/hooks.hpp"
#include "vk/image.hpp"
#include "vk/initializers.hpp"
#include "vk/macros.hpp"

namespace
{
    // Max difference between the aspect ratio of a depth buffer and of the swapchain
    constexpr double ASPECT_RATIO_TOLERANCE = 0.1;

    bool is_depth_format(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return true;
            default:
                return false;
        }
    }

    bool has_stencil(VkFormat format)
    {
        return format == VK_FORMAT_D16_UNORM_S8_UINT
            || format == VK_FORMAT_D24_UNORM_S8_UINT
            || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    // Every aspect the format has (what whole-image barriers have to cover)
    VkImageAspectFlags full_aspect(VkFormat format)
    {
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (has_stencil(format))
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        return aspect;
    }

    VkImageMemoryBarrier2 image_barrier(VkImage              image,
                                        VkImageAspectFlags   aspect,
                                        VkImageLayout        oldLayout,
                                        VkImageLayout        newLayout,
                                        VkPipelineStageFlags2 srcStage,
                                        VkAccessFlags2       srcAccess,
                                        VkPipelineStageFlags2 dstStage,
                                        VkAccessFlags2       dstAccess)
    {
        VkImageMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = srcStage,
            .srcAccessMask = srcAccess,
            .dstStageMask = dstStage,
            .dstAccessMask = dstAccess,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        return barrier;
    }
}  // namespace

vkShade::DepthTracker::DepthTracker(VulkanDevice& device)
    : m_device(device)
{
    Logger::trace("Creating depth tracker");

    m_getFormatProperties = get_instance_from_handle(m_device.instance).dispatch.GetPhysicalDeviceFormatProperties;
    this->refresh_config();
}

vkShade::DepthTracker::~DepthTracker()
{
    Logger::trace("Destroying depth tracker");
}

void vkShade::DepthTracker::refresh_config()
{
    bool enabled = true;
    bool useAspectRatio = true;
    bool useDrawStats = true;

    if (Locator<ConfigManager>::has())
    {
        auto& config = Locator<ConfigManager>::get().app();
        enabled = config.get<bool>("Depth", "Enabled").value_or(true);
        useAspectRatio = config.get<bool>("Depth", "UseAspectRatioHeuristics").value_or(true);
        useDrawStats = config.get<bool>("Depth", "DrawStatsHeuristic").value_or(true);
    }

    std::lock_guard lock(m_mutex);
    m_enabled = enabled;
    m_useAspectRatio = useAspectRatio;
    m_useDrawStats = useDrawStats;
}

bool vkShade::DepthTracker::has_depth() const
{
    std::lock_guard lock(m_mutex);
    return m_hasDepth && m_sampled != nullptr;
}

vkShade::VulkanImage* vkShade::DepthTracker::sampled_depth() const
{
    std::lock_guard lock(m_mutex);
    return m_sampled.get();
}

// ---------------------------------------------------------------------------------------
// Per-frame selection
// ---------------------------------------------------------------------------------------

void vkShade::DepthTracker::on_present(VkExtent2D swapchainExtent)
{
    this->refresh_config();

    std::lock_guard lock(m_mutex);

    m_swapchainExtent = swapchainExtent;

    // Destroy replaced sampled images once no recent command buffer can still reference them
    for (auto& retired : m_retired)
    {
        if (retired.framesLeft > 0)
            retired.framesLeft--;
    }
    std::erase_if(m_retired, [](const RetiredImage& retired) { return retired.framesLeft == 0; });

    VkImage selected = m_enabled ? this->select_best_locked() : VK_NULL_HANDLE;

    if (selected != m_selectedImage)
    {
        if (selected != VK_NULL_HANDLE)
        {
            const auto& info = m_images.at(selected);
            Logger::info("Selected depth buffer: {}x{} {} ({} draws)",
                info.extent.width, info.extent.height, magic_enum::enum_name(info.format), info.draws);
        }
        else if (m_selectedImage != VK_NULL_HANDLE)
        {
            Logger::info("No depth buffer candidate available");
        }
        m_selectedImage = selected;
    }

    if (selected != VK_NULL_HANDLE)
    {
        // Make sure the copy target matches the selection. The first copy lands next frame.
        const auto& info = m_images.at(selected);
        const VkFormat copyFormat = this->choose_copy_format_locked(info.format);

        if (copyFormat != VK_FORMAT_UNDEFINED &&
            (!m_sampled || m_sampledFormat != copyFormat ||
             m_sampledExtent.width != info.extent.width || m_sampledExtent.height != info.extent.height))
        {
            this->create_sampled_image_locked(copyFormat, info.extent);
        }
    }

    // Depth is only usable if that buffer's copy was submitted into the current sampled image.
    // A recreate above targets the next frame; copies already recorded still write the old one.
    if (selected != VK_NULL_HANDLE && !this->copy_submitted_locked(selected))
        selected = VK_NULL_HANDLE;

    m_hasDepth = selected != VK_NULL_HANDLE && m_sampled != nullptr;
}

void vkShade::DepthTracker::begin_frame()
{
    std::lock_guard lock(m_mutex);

    for (auto& [image, info] : m_images)
    {
        info.draws = 0;
        info.vertices = 0;
        info.rendered = false;
    }

    if (m_midPassClears > 0)
    {
        Logger::debug("{} depth clears recorded inside a render pass last frame (not copied)", m_midPassClears);
        m_midPassClears = 0;
    }

    m_bestImage = VK_NULL_HANDLE;
    m_copiedThisFrame.clear();
    m_hasDepth = false;
}

VkFormatFeatureFlags vkShade::DepthTracker::optimal_features_locked(VkFormat format)
{
    auto it = m_formatFeatures.find(format);
    if (it != m_formatFeatures.end())
        return it->second;

    if (!m_getFormatProperties)
        return 0;

    VkFormatProperties properties {};
    m_getFormatProperties(m_device.physicalDevice, format, &properties);

    m_formatFeatures.emplace(format, properties.optimalTilingFeatures);
    return properties.optimalTilingFeatures;
}

VkFormat vkShade::DepthTracker::choose_copy_format_locked(VkFormat sourceFormat)
{
    auto it = m_copyFormats.find(sourceFormat);
    if (it != m_copyFormats.end())
        return it->second;

    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    auto supported = [&](VkFormat format) { return (this->optimal_features_locked(format) & required) == required; };

    // Prefer D32_SFLOAT when the source depth aspect is copy-compatible with it.
    // Otherwise keep the matching format, or its depth-only twin. Soft-fail (UNDEFINED)
    // when nothing sampleable can be a transfer destination.
    VkFormat result = VK_FORMAT_UNDEFINED;
    auto consider = [&](VkFormat format)
    {
        if (result == VK_FORMAT_UNDEFINED && supported(format))
            result = format;
    };

    const bool d32Compatible = sourceFormat == VK_FORMAT_D32_SFLOAT
                            || sourceFormat == VK_FORMAT_D32_SFLOAT_S8_UINT;
    if (d32Compatible)
        consider(VK_FORMAT_D32_SFLOAT);

    consider(sourceFormat);
    if (sourceFormat == VK_FORMAT_D24_UNORM_S8_UINT)
        consider(VK_FORMAT_X8_D24_UNORM_PACK32);
    else if (sourceFormat == VK_FORMAT_D16_UNORM_S8_UINT)
        consider(VK_FORMAT_D16_UNORM);
    else if (sourceFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
        consider(VK_FORMAT_D32_SFLOAT);

    if (result == VK_FORMAT_UNDEFINED)
        Logger::warn("Depth format {} can't be copied to a sampled image", magic_enum::enum_name(sourceFormat));

    m_copyFormats.emplace(sourceFormat, result);
    return result;
}

bool vkShade::DepthTracker::passes_filters_locked(const ImageInfo& info) const
{
    if (!info.copyable || !info.rendered)
        return false;

    if (m_useDrawStats && info.draws == 0)
        return false;

    if (m_useAspectRatio && m_swapchainExtent.width > 0 && m_swapchainExtent.height > 0 && info.extent.height > 0)
    {
        const double imageAspect = static_cast<double>(info.extent.width) / info.extent.height;
        const double swapchainAspect = static_cast<double>(m_swapchainExtent.width) / m_swapchainExtent.height;
        if (std::abs(imageAspect - swapchainAspect) > ASPECT_RATIO_TOLERANCE)
            return false;
    }

    return true;
}

bool vkShade::DepthTracker::is_better_locked(const ImageInfo& a, const ImageInfo& b) const
{
    if (m_useDrawStats)
    {
        if (a.draws != b.draws)
            return a.draws > b.draws;
        if (a.vertices != b.vertices)
            return a.vertices > b.vertices;
    }

    // Tie-break (or no draw statistics): prefer the larger image
    return static_cast<uint64_t>(a.extent.width) * a.extent.height >
           static_cast<uint64_t>(b.extent.width) * b.extent.height;
}

VkImage vkShade::DepthTracker::select_best_locked() const
{
    VkImage best = VK_NULL_HANDLE;
    const ImageInfo* bestInfo = nullptr;

    for (const auto& [image, info] : m_images)
    {
        if (!this->passes_filters_locked(info))
            continue;

        if (!bestInfo || this->is_better_locked(info, *bestInfo))
        {
            best = image;
            bestInfo = &info;
        }
    }

    return best;
}

// Returns true if `image` is at least as good as every candidate seen so far this frame
bool vkShade::DepthTracker::consider_best_locked(VkImage image)
{
    auto it = m_images.find(image);
    if (it == m_images.end() || !this->passes_filters_locked(it->second))
        return false;

    auto bestIt = m_images.find(m_bestImage);
    if (m_bestImage == VK_NULL_HANDLE || bestIt == m_images.end() || image == m_bestImage ||
        !this->is_better_locked(bestIt->second, it->second))
    {
        m_bestImage = image;
        return true;
    }

    return false;
}

vkShade::DepthTracker::CopyPlan vkShade::DepthTracker::plan_copy_locked(VkImage image)
{
    CopyPlan plan;

    auto it = m_images.find(image);
    if (!m_sampled || it == m_images.end())
        return plan;

    const ImageInfo& info = it->second;

    // The copy target is sized for the image selected at the last present. Until it matches
    // (first frame, resolution change) nothing is copied.
    if (this->choose_copy_format_locked(info.format) != m_sampledFormat ||
        info.extent.width != m_sampledExtent.width || info.extent.height != m_sampledExtent.height)
    {
        return plan;
    }

    plan.valid = true;
    plan.src = image;
    plan.srcFormat = info.format;
    plan.dst = m_sampled->image();
    plan.dstFormat = m_sampledFormat;
    plan.extent = info.extent;
    return plan;
}

void vkShade::DepthTracker::create_sampled_image_locked(VkFormat format, VkExtent2D extent)
{
    Logger::debug("Creating sampled depth image (Size: {}x{}, Format: {})",
        extent.width, extent.height, magic_enum::enum_name(format));

    if (m_sampled)
        m_retired.push_back({std::move(m_sampled), RETIRE_FRAMES});

    m_sampled = std::make_unique<VulkanImage>(
        m_device, extent, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    m_sampledFormat = format;
    m_sampledExtent = extent;

    // Copies already recorded target the retired image. Drop them so has_depth stays false
    // until a later submit writes this one. Pending records keep the old destination handle
    // and fail the destination check on their own.
    m_copiedThisFrame.clear();

    // Bring the image into SHADER_READ_ONLY_OPTIMAL so that it is always valid to sample,
    // even if no copy ends up executing before the runtime reads it.
    // Failure here is a soft miss: effects keep the dummy depth for this frame.
    auto fail = [&](const char* reason)
    {
        Logger::warn("Skipping sampled depth image ({}x{} {}): {}",
            extent.width, extent.height, magic_enum::enum_name(format), reason);
        m_sampled.reset();
        m_sampledFormat = VK_FORMAT_UNDEFINED;
        m_sampledExtent = { 0, 0 };
    };

    if (!m_device.dispatch.BeginCommandBuffer || !m_device.dispatch.EndCommandBuffer ||
        !m_device.dispatch.FreeCommandBuffers || !m_device.dispatch.CmdClearDepthStencilImage ||
        !m_device.dispatch.QueueSubmit || !m_device.dispatch.QueueWaitIdle)
    {
        fail("device is missing transfer commands");
        return;
    }

    VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(m_device.commandPool);

    // NOTE: Command buffers are dispatchable types. Their creation must go through our
    //  own hook, not the dispatch table, to ensure the dispatch pointer fixup runs.
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkShade_AllocateCommandBuffers(m_device.handle, &allocInfo, &cmd) != VK_SUCCESS || cmd == VK_NULL_HANDLE)
    {
        fail("command buffer allocation failed");
        return;
    }

    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (m_device.dispatch.BeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
    {
        m_device.dispatch.FreeCommandBuffers(m_device.handle, m_device.commandPool, 1, &cmd);
        fail("command buffer begin failed");
        return;
    }

    m_sampled->transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkClearDepthStencilValue clearValue = { .depth = 1.0f, .stencil = 0 };
    VkImageSubresourceRange range = vkinit::image_subresource_range(full_aspect(format));
    m_device.dispatch.CmdClearDepthStencilImage(
        cmd, m_sampled->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);

    m_sampled->transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (m_device.dispatch.EndCommandBuffer(cmd) != VK_SUCCESS)
    {
        m_device.dispatch.FreeCommandBuffers(m_device.handle, m_device.commandPool, 1, &cmd);
        fail("command buffer end failed");
        return;
    }

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    // Dispatch table, not the layer hook: this clear is not an application depth copy.
    const VkResult submitted = m_device.dispatch.QueueSubmit(m_device.queue, 1, &submitInfo, VK_NULL_HANDLE);
    const VkResult waited = submitted == VK_SUCCESS ? m_device.dispatch.QueueWaitIdle(m_device.queue) : submitted;
    m_device.dispatch.FreeCommandBuffers(m_device.handle, m_device.commandPool, 1, &cmd);

    if (waited != VK_SUCCESS)
        fail("depth clear submit failed");
}

// ---------------------------------------------------------------------------------------
// Copy recording
// ---------------------------------------------------------------------------------------

bool vkShade::DepthTracker::record_copy(VkCommandBuffer cmd, const CopyPlan& plan, VkImageLayout srcLayout)
{
    if (!plan.valid || srcLayout == VK_IMAGE_LAYOUT_UNDEFINED || !m_device.dispatch.CmdPipelineBarrier2 ||
        !m_device.dispatch.CmdCopyImage)
        return false;

    // Images in a layout that is already readable by transfers keep it for the copy
    const bool srcLayoutKept = srcLayout == VK_IMAGE_LAYOUT_GENERAL ||
                               srcLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkImageLayout copyLayout = srcLayoutKept ? srcLayout : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    // Layouts which only cover the depth aspect can only be transitioned for depth
    VkImageAspectFlags srcAspect = full_aspect(plan.srcFormat);
    if (has_stencil(plan.srcFormat) &&
        (srcLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || srcLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL))
    {
        srcAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    const VkImageAspectFlags dstAspect = full_aspect(plan.dstFormat);

    // Source -> TRANSFER_SRC and sampled image -> TRANSFER_DST (its old contents are discarded)
    std::array<VkImageMemoryBarrier2, 2> before = {
        image_barrier(plan.src, srcAspect, srcLayout, copyLayout,
                      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT),
        image_barrier(plan.dst, dstAspect, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT),
    };

    VkDependencyInfo beforeDep = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<uint32_t>(before.size()),
        .pImageMemoryBarriers = before.data(),
    };
    m_device.dispatch.CmdPipelineBarrier2(cmd, &beforeDep);

    VkImageCopy region = {
        .srcSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 },
        .srcOffset = { 0, 0, 0 },
        .dstSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 },
        .dstOffset = { 0, 0, 0 },
        .extent = { plan.extent.width, plan.extent.height, 1 },
    };
    m_device.dispatch.CmdCopyImage(
        cmd, plan.src, copyLayout, plan.dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Hand the source back in the layout the application expects, and publish the copy
    std::array<VkImageMemoryBarrier2, 2> after = {
        image_barrier(plan.src, srcAspect, copyLayout, srcLayout,
                      VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT),
        image_barrier(plan.dst, dstAspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT),
    };

    VkDependencyInfo afterDep = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<uint32_t>(after.size()),
        .pImageMemoryBarriers = after.data(),
    };
    m_device.dispatch.CmdPipelineBarrier2(cmd, &afterDep);
    return true;
}

void vkShade::DepthTracker::remember_copy(VkCommandBuffer cmd, const CopyPlan& plan)
{
    if (!plan.valid || cmd == VK_NULL_HANDLE)
        return;

    std::lock_guard lock(m_mutex);
    m_pendingCopies[cmd].push_back({ cmd, plan.src, plan.dst });
}

void vkShade::DepthTracker::note_submitted_locked(VkCommandBuffer cmd)
{
    if (cmd == VK_NULL_HANDLE)
        return;

    auto it = m_pendingCopies.find(cmd);
    if (it == m_pendingCopies.end())
        return;

    m_copiedThisFrame.insert(m_copiedThisFrame.end(), it->second.begin(), it->second.end());
}

bool vkShade::DepthTracker::copy_submitted_locked(VkImage src) const
{
    if (!m_sampled || src == VK_NULL_HANDLE)
        return false;

    const VkImage dst = m_sampled->image();
    for (const auto& copy : m_copiedThisFrame)
    {
        if (copy.src == src && copy.dst == dst)
            return true;
    }
    return false;
}

void vkShade::DepthTracker::on_begin_command_buffer(VkCommandBuffer cmd)
{
    std::lock_guard lock(m_mutex);
    // vkBeginCommandBuffer drops every command previously recorded in this buffer.
    m_pendingCopies.erase(cmd);
    m_cmdStates.erase(cmd);
}

void vkShade::DepthTracker::on_queue_submit(uint32_t submitCount, const VkSubmitInfo* pSubmits)
{
    if (!pSubmits || submitCount == 0)
        return;

    std::lock_guard lock(m_mutex);
    for (uint32_t i = 0; i < submitCount; ++i)
    {
        const VkSubmitInfo& submit = pSubmits[i];
        if (!submit.pCommandBuffers)
            continue;

        for (uint32_t c = 0; c < submit.commandBufferCount; ++c)
            this->note_submitted_locked(submit.pCommandBuffers[c]);
    }
}

void vkShade::DepthTracker::on_queue_submit2(uint32_t submitCount, const VkSubmitInfo2* pSubmits)
{
    if (!pSubmits || submitCount == 0)
        return;

    std::lock_guard lock(m_mutex);
    for (uint32_t i = 0; i < submitCount; ++i)
    {
        const VkSubmitInfo2& submit = pSubmits[i];
        if (!submit.pCommandBufferInfos)
            continue;

        for (uint32_t c = 0; c < submit.commandBufferInfoCount; ++c)
            this->note_submitted_locked(submit.pCommandBufferInfos[c].commandBuffer);
    }
}

// ---------------------------------------------------------------------------------------
// Image / view hooks
// ---------------------------------------------------------------------------------------

bool vkShade::DepthTracker::adjust_image_create_info(const VkImageCreateInfo& in, VkImageCreateInfo& out)
{
    if (!is_depth_format(in.format))
        return false;

    // Only plain 2D depth attachments can be copied. Transient attachments can't take extra usages.
    constexpr VkImageCreateFlags sparseFlags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                                               VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT |
                                               VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
    if (in.imageType != VK_IMAGE_TYPE_2D ||
        in.tiling != VK_IMAGE_TILING_OPTIMAL ||
        in.samples != VK_SAMPLE_COUNT_1_BIT ||
        in.arrayLayers != 1 ||
        in.mipLevels != 1 ||
        (in.flags & sparseFlags) ||
        !(in.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ||
        (in.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT))
    {
        return false;
    }

    std::lock_guard lock(m_mutex);

    if (!m_enabled)
        return false;

    const VkFormatFeatureFlags features = this->optimal_features_locked(in.format);
    if (!(features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
        return false;

    out = in;
    out.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        out.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    return true;
}

void vkShade::DepthTracker::on_image_created(VkImage image, const VkImageCreateInfo& createInfo)
{
    if (!is_depth_format(createInfo.format) ||
        createInfo.imageType != VK_IMAGE_TYPE_2D ||
        !(createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        return;
    }

    ImageInfo info;
    info.format = createInfo.format;
    info.extent = { createInfo.extent.width, createInfo.extent.height };
    info.copyable = (createInfo.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &&
                    createInfo.tiling == VK_IMAGE_TILING_OPTIMAL &&
                    createInfo.samples == VK_SAMPLE_COUNT_1_BIT &&
                    createInfo.arrayLayers == 1 &&
                    createInfo.mipLevels == 1;

    Logger::debug("Tracking depth image (Size: {}x{}, Format: {}, Copyable: {})",
        info.extent.width, info.extent.height, magic_enum::enum_name(info.format), info.copyable);

    std::lock_guard lock(m_mutex);
    m_images[image] = info;
}

void vkShade::DepthTracker::on_image_destroyed(VkImage image)
{
    std::lock_guard lock(m_mutex);

    if (m_images.erase(image) == 0)
        return;

    if (m_bestImage == image)
        m_bestImage = VK_NULL_HANDLE;
    if (m_selectedImage == image)
        m_selectedImage = VK_NULL_HANDLE;

    std::erase_if(m_views, [&](const auto& entry) { return entry.second == image; });

    for (auto& [cmd, copies] : m_pendingCopies)
    {
        std::erase_if(copies, [&](const RecordedCopy& copy) {
            return copy.src == image || copy.dst == image;
        });
    }
    std::erase_if(m_copiedThisFrame, [&](const RecordedCopy& copy) {
        return copy.src == image || copy.dst == image;
    });
}

void vkShade::DepthTracker::on_image_view_created(const VkImageViewCreateInfo& createInfo, VkImageView view)
{
    if (!(createInfo.subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT))
        return;

    std::lock_guard lock(m_mutex);
    if (m_images.contains(createInfo.image))
        m_views[view] = createInfo.image;
}

void vkShade::DepthTracker::on_image_view_destroyed(VkImageView view)
{
    std::lock_guard lock(m_mutex);
    m_views.erase(view);
}

// ---------------------------------------------------------------------------------------
// Command hooks
// ---------------------------------------------------------------------------------------

void vkShade::DepthTracker::on_begin_rendering(VkCommandBuffer cmd, const VkRenderingInfo* renderingInfo)
{
    CopyPlan plan;
    VkImageLayout planLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    {
        std::lock_guard lock(m_mutex);

        const VkRenderingAttachmentInfo* depth = renderingInfo ? renderingInfo->pDepthAttachment : nullptr;
        if (!m_enabled || !depth || depth->imageView == VK_NULL_HANDLE)
        {
            m_cmdStates.erase(cmd);
            return;
        }

        auto viewIt = m_views.find(depth->imageView);
        auto imageIt = viewIt != m_views.end() ? m_images.find(viewIt->second) : m_images.end();
        if (imageIt == m_images.end())
        {
            m_cmdStates.erase(cmd);
            return;
        }

        CmdState state;
        state.image = viewIt->second;
        state.layout = depth->imageLayout;
        state.storeDepth = depth->storeOp != VK_ATTACHMENT_STORE_OP_DONT_CARE;
        state.suspending = (renderingInfo->flags & VK_RENDERING_SUSPENDING_BIT) != 0;

        ImageInfo& info = imageIt->second;
        if (state.storeDepth)
            info.rendered = true;

        // The depth is about to be cleared. If the buffer already holds this frame's
        // content, copy it now: this is the last chance to get it.
        const bool resuming = (renderingInfo->flags & VK_RENDERING_RESUMING_BIT) != 0;
        if (depth->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR && !resuming &&
            info.draws > 0 && this->consider_best_locked(state.image))
        {
            plan = this->plan_copy_locked(state.image);
            planLayout = state.layout;
        }

        m_cmdStates[cmd] = state;
    }

    if (this->record_copy(cmd, plan, planLayout))
        this->remember_copy(cmd, plan);
}

void vkShade::DepthTracker::on_end_rendering(VkCommandBuffer cmd)
{
    CopyPlan plan;
    VkImageLayout planLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    {
        std::lock_guard lock(m_mutex);

        auto stateIt = m_cmdStates.find(cmd);
        if (stateIt == m_cmdStates.end())
            return;

        const CmdState state = stateIt->second;
        m_cmdStates.erase(stateIt);

        auto imageIt = m_images.find(state.image);
        if (imageIt == m_images.end())
            return;

        imageIt->second.draws += state.draws;
        imageIt->second.vertices += state.vertices;

        // A suspended pass is finished (and copied) by the pass that resumes it
        if (state.suspending || !state.storeDepth)
            return;

        if (this->consider_best_locked(state.image))
        {
            plan = this->plan_copy_locked(state.image);
            planLayout = state.layout;
        }
    }

    if (this->record_copy(cmd, plan, planLayout))
        this->remember_copy(cmd, plan);
}

void vkShade::DepthTracker::on_draw(VkCommandBuffer cmd, uint32_t drawCount, uint64_t vertexCount)
{
    std::lock_guard lock(m_mutex);

    auto it = m_cmdStates.find(cmd);
    if (it == m_cmdStates.end())
        return;

    it->second.draws += drawCount;
    it->second.vertices += vertexCount;
}

void vkShade::DepthTracker::on_clear_depth_stencil_image(VkCommandBuffer cmd, VkImage image, VkImageLayout layout)
{
    // Only layouts that can be transitioned to TRANSFER_SRC and back
    if (layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && layout != VK_IMAGE_LAYOUT_GENERAL)
        return;

    CopyPlan plan;

    {
        std::lock_guard lock(m_mutex);

        if (!m_enabled || !this->consider_best_locked(image))
            return;

        plan = this->plan_copy_locked(image);
    }

    if (this->record_copy(cmd, plan, layout))
        this->remember_copy(cmd, plan);
}

void vkShade::DepthTracker::on_clear_attachments(VkCommandBuffer cmd, uint32_t attachmentCount, const VkClearAttachment* attachments)
{
    bool clearsDepth = false;
    for (uint32_t i = 0; i < attachmentCount; i++)
        clearsDepth |= (attachments[i].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;

    if (!clearsDepth)
        return;

    std::lock_guard lock(m_mutex);

    auto it = m_cmdStates.find(cmd);
    if (it == m_cmdStates.end())
        return;

    // Transfers aren't allowed inside a render scope, so the depth written before this
    // clear can't be saved. Only remember that it happened.
    it->second.clearedMidPass = true;
    m_midPassClears++;
}
