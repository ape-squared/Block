#include "image.hpp"

#include <algorithm>
#include <effect_module.hpp>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <vulkan/vulkan_core.h>

#include "core/service_locator.hpp"
#include "core/logger.hpp"
#include "config/config_manager.hpp"
#include "buffer.hpp"
#include "initializers.hpp"
#include "macros.hpp"

namespace
{
    // Aspects an image *view* uses. Views that are sampled must select a single aspect,
    // so combined depth/stencil formats are viewed as depth only.
    VkImageAspectFlags view_aspect_flags(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    // Aspects a whole-image barrier has to cover: combined formats need both.
    VkImageAspectFlags barrier_aspect_flags(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return view_aspect_flags(format);
        }
    }
}  // namespace

std::filesystem::path vkShade::VulkanImage::find_texture_file(const std::string& fileName)
{
    auto& config = Locator<ConfigManager>::get().app();
    auto searchPaths = config.get<std::vector<std::string>>("ReShade", "TextureSearchPaths");

    auto matches = [&](const std::filesystem::path& path)
    {
        auto a = path.filename().string();
        auto b = fileName;
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        std::transform(b.begin(), b.end(), b.begin(), ::tolower);
        return a == b;
    };

    for (const auto& path : searchPaths.value_or(std::vector<std::string>{}))
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec))
            continue;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 path, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file(ec))
                continue;
            if (matches(entry.path()))
                return entry.path();
        }
    }

    return {};
}

vkShade::VulkanImage::VulkanImage(VulkanDevice& device, const reshadefx::texture& info)
    : VulkanObject(device)
{
    Logger::trace("Creating image from ReShade FX info");

    m_extent.width  = info.width;
    m_extent.height = info.height;
    m_extent.depth  = info.depth;
    m_mipLevels = std::max<uint16_t>(1, info.levels);

    m_format = convert_format(info.format);

    // Format undefined is UB, so fallback to RGBA8 UNORM.
    if (m_format == VK_FORMAT_UNDEFINED)
        m_format = VK_FORMAT_R8G8B8A8_UNORM;

    m_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (info.render_target)
        m_usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (info.storage_access)
        m_usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

    // If image has a source (file) annotation, load it.
    auto it = std::find_if(info.annotations.begin(), info.annotations.end(), [](const auto& a)
    {
        return a.name == "source";
    });

    std::filesystem::path sourcePath;
    if (it != info.annotations.end())
    {
        sourcePath = find_texture_file(it->value.string_data);
        if (sourcePath.empty())
            throw std::runtime_error("Unable to find texture: " + it->value.string_data);

        if (m_extent.width == 1 && m_extent.height == 1)
        {
            int w = 0, h = 0, c = 0;
            if (stbi_info(sourcePath.string().c_str(), &w, &h, &c))
            {
                m_extent.width = w;
                m_extent.height = h;
            }
        }
    }

    this->create_image();
    this->create_image_view();

    if (!sourcePath.empty())
        this->load_from_file(sourcePath.string());
}

void vkShade::VulkanImage::load_from_file(const std::string& filePath)
{
    Logger::trace("Creating image from file: {}", filePath);

    int32_t channels;
    stbir_pixel_layout stbirLayout;
    switch (m_format)
    {
        case VK_FORMAT_R8_UNORM:
            channels = 1;
            stbirLayout = STBIR_1CHANNEL;
            break;
        case VK_FORMAT_R8G8_UNORM:
            channels = 2;
            stbirLayout = STBIR_2CHANNEL;
            break;
        case VK_FORMAT_R8G8B8A8_UNORM:
            channels = 4;
            stbirLayout = STBIR_RGBA;
            break;
        default:
            throw std::runtime_error("Unsupported format for file-sourced texture: " +
                std::string(magic_enum::enum_name(m_format)));
    }

    int32_t srcWidth, srcHeight, srcChannels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &srcWidth, &srcHeight, &srcChannels, channels);

    if (!pixels)
    {
        Logger::error("Failed to load image: {}", filePath);
        throw std::runtime_error("Failed to load image: " + filePath);
    }

    // Resize the image if it doesn't match the requested size
    const void* uploadData = pixels;
    std::vector<uint8_t> resized;

    if (srcWidth != (int32_t)m_extent.width || srcHeight != (int32_t)m_extent.height)
    {
        resized.resize(m_extent.width * m_extent.height * channels);
        stbir_resize_uint8_linear(pixels, srcWidth, srcHeight, 0,
                                  resized.data(), m_extent.width, m_extent.height, 0,
                                  stbirLayout);
        uploadData = resized.data();
    }

    size_t imageSize = m_extent.width * m_extent.height * channels;
    this->upload(uploadData, imageSize);

    stbi_image_free(pixels);
}

vkShade::VulkanImage::VulkanImage(VulkanDevice& device, VkExtent2D size, VkFormat format, VkImageUsageFlags usageFlags)
    : VulkanObject(device)
{
    Logger::trace("Creating image (Size: {}x{}, Format: {})",
        size.width, size.height, magic_enum::enum_name(format));

    // Set internal properties
    m_extent.width  = size.width;
    m_extent.height = size.height;
    m_extent.depth  = 1;
    m_format = format;
    m_usageFlags = usageFlags;

    this->create_image();
    this->create_image_view();
}

vkShade::VulkanImage::VulkanImage(VulkanDevice& device, VkImage image, VkExtent2D size, VkFormat format)
    : VulkanObject(device)
{
    Logger::trace("Wrapping existing image (Size: {}x{}, Format: {})",
        size.width, size.height, magic_enum::enum_name(format));

    // Set internal properties
    m_image = image;
    m_extent.width  = size.width;
    m_extent.height = size.height;
    m_extent.depth  = 1;
    m_format = format;
    m_owning = false;  // IMPORTANT! We don't own the image here.

    // Only create the view (no image since we don't own it)
    this->create_image_view();
}

vkShade::VulkanImage::~VulkanImage()
{
    Logger::trace("Destroying VulkanImage");

    if (m_srgbImageView != VK_NULL_HANDLE)
        m_device.dispatch.DestroyImageView(m_device.handle, m_srgbImageView, nullptr);

    if (m_imageView != VK_NULL_HANDLE)
        m_device.dispatch.DestroyImageView(m_device.handle, m_imageView, nullptr);

    if (m_owning && m_image != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE)
        vmaDestroyImage(m_device.allocator, m_image, m_allocation);
}

VkFormat vkShade::VulkanImage::srgb_variant(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
        default: return VK_FORMAT_UNDEFINED;
    }
}

void vkShade::VulkanImage::create_image()
{
    VkImageCreateInfo imgInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = (srgb_variant(m_format) != VK_FORMAT_UNDEFINED) ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0u,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_format,
        .extent = VkExtent3D {m_extent.width, m_extent.height, 1},
        .mipLevels = m_mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = m_usageFlags,
    };

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(m_device.allocator, &imgInfo, &allocInfo, &m_image, &m_allocation, nullptr));
}

void vkShade::VulkanImage::create_image_view()
{
	const VkImageAspectFlags aspectFlags = view_aspect_flags(m_format);

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_format,
        .subresourceRange = {
            .aspectMask = aspectFlags,
            .baseMipLevel = 0,
            .levelCount = m_mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

	VK_CHECK(m_device.dispatch.CreateImageView(m_device.handle, &viewInfo, nullptr, &m_imageView));

    const VkFormat srgbFormat = srgb_variant(m_format);
    if (srgbFormat != VK_FORMAT_UNDEFINED && m_owning)
    {
        viewInfo.format = srgbFormat;
        VK_CHECK(m_device.dispatch.CreateImageView(m_device.handle, &viewInfo, nullptr, &m_srgbImageView));
    }
}

void vkShade::VulkanImage::generate_mipmaps(VkCommandBuffer cmd)
{
    if (m_mipLevels <= 1)
        return;

    const VkImageAspectFlags aspect = view_aspect_flags(m_format);
    int32_t mipWidth = static_cast<int32_t>(m_extent.width);
    int32_t mipHeight = static_cast<int32_t>(m_extent.height);

    for (uint32_t i = 1; i < m_mipLevels; ++i)
    {
        // Transition source mip to TRANSFER_SRC
        VkImageMemoryBarrier2 srcBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = (i == 1) ? m_currentLayout : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = m_image,
            .subresourceRange = { aspect, i - 1, 1, 0, 1 },
        };
        VkImageMemoryBarrier2 dstBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = m_image,
            .subresourceRange = { aspect, i, 1, 0, 1 },
        };
        std::array barriers = { srcBarrier, dstBarrier };
        VkDependencyInfo dep {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
            .pImageMemoryBarriers = barriers.data(),
        };
        m_device.dispatch.CmdPipelineBarrier2(cmd, &dep);

        const int32_t nextW = std::max(1, mipWidth / 2);
        const int32_t nextH = std::max(1, mipHeight / 2);

        VkImageBlit2 blitRegion { .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
        blitRegion.srcOffsets[0] = { 0, 0, 0 };
        blitRegion.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blitRegion.dstOffsets[0] = { 0, 0, 0 };
        blitRegion.dstOffsets[1] = { nextW, nextH, 1 };
        blitRegion.srcSubresource = { aspect, i - 1, 0, 1 };
        blitRegion.dstSubresource = { aspect, i, 0, 1 };

        VkBlitImageInfo2 blitInfo {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = m_image,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = m_image,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blitRegion,
            .filter = VK_FILTER_LINEAR,
        };
        m_device.dispatch.CmdBlitImage2(cmd, &blitInfo);

        mipWidth = nextW;
        mipHeight = nextH;
    }

    // Transition all mips to SHADER_READ_ONLY
    VkImageMemoryBarrier2 finalBarrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = m_image,
        .subresourceRange = { aspect, 0, m_mipLevels, 0, 1 },
    };
    VkDependencyInfo finalDep {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &finalBarrier,
    };
    m_device.dispatch.CmdPipelineBarrier2(cmd, &finalDep);
    m_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void vkShade::VulkanImage::blit_from(VkCommandBuffer cmd, VkImage source)
{
    blit(cmd, source, m_image);
}

void vkShade::VulkanImage::blit_to(VkCommandBuffer cmd, VkImage destination)
{
    blit(cmd, m_image, destination);
}

void vkShade::VulkanImage::blit(VkCommandBuffer cmd, VkImage source, VkImage destination)
{
	VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

    blitRegion.srcOffsets[0] = { 0, 0, 0 };
    blitRegion.srcOffsets[1] = { (int32_t)m_extent.width, (int32_t)m_extent.height, 1 };

    blitRegion.dstOffsets[0] = { 0, 0, 0 };
    blitRegion.dstOffsets[1] = { (int32_t)m_extent.width, (int32_t)m_extent.height, 1 };

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.srcImage = m_image;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	m_device.dispatch.CmdBlitImage2(cmd, &blitInfo);
}

VkFormat vkShade::VulkanImage::convert_format(reshadefx::texture_format format)
{
    switch (format)
    {
        case reshadefx::texture_format::unknown:    return VK_FORMAT_UNDEFINED;

        case reshadefx::texture_format::r8:         return VK_FORMAT_R8_UNORM;
        case reshadefx::texture_format::r16f:       return VK_FORMAT_R16_SFLOAT;
        case reshadefx::texture_format::r16:        return VK_FORMAT_R16_UNORM;
        case reshadefx::texture_format::r32f:       return VK_FORMAT_R32_SFLOAT;
        case reshadefx::texture_format::r32u:       return VK_FORMAT_R32_UINT;
        case reshadefx::texture_format::r32i:       return VK_FORMAT_R32_SINT;

        case reshadefx::texture_format::rg8:        return VK_FORMAT_R8G8_UNORM;
        case reshadefx::texture_format::rg16f:      return VK_FORMAT_R16G16_SFLOAT;
        case reshadefx::texture_format::rg16:       return VK_FORMAT_R16G16_UNORM;
        case reshadefx::texture_format::rg32f:      return VK_FORMAT_R32G32_SFLOAT;

        case reshadefx::texture_format::rgba8:      return VK_FORMAT_R8G8B8A8_UNORM;
        case reshadefx::texture_format::rgba16f:    return VK_FORMAT_R16G16B16A16_SFLOAT;
        case reshadefx::texture_format::rgba16:     return VK_FORMAT_R16G16B16A16_UNORM;
        case reshadefx::texture_format::rgba32f:    return VK_FORMAT_R32G32B32A32_SFLOAT;
        case reshadefx::texture_format::rgba32u:    return VK_FORMAT_R32G32B32A32_UINT;
        case reshadefx::texture_format::rgba32i:    return VK_FORMAT_R32G32B32A32_SINT;

        case reshadefx::texture_format::rgb10a2:    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case reshadefx::texture_format::rg11b10f:   return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    }

    std::unreachable();
}

void vkShade::VulkanImage::transition_layout(VkCommandBuffer cmd, VkImageLayout newLayout)
{
    const VkImageAspectFlags aspectMask = barrier_aspect_flags(m_format);

    VkImageMemoryBarrier2 imageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = m_currentLayout,  // Track current layout
        .newLayout = newLayout,
        .image = m_image,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkDependencyInfo depInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier,
    };

    m_device.dispatch.CmdPipelineBarrier2(cmd, &depInfo);

    m_currentLayout = newLayout;  // Update tracked layout
}

void vkShade::VulkanImage::upload(const void* data, size_t size)
{
    // Create staging buffer
    VulkanBuffer staging(m_device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    staging.write(data, size);

    // Record and submit a one-time command buffer
    VkCommandBufferAllocateInfo cmdBufAllocInfo = vkinit::command_buffer_allocate_info(m_device.commandPool);

    VkCommandBuffer cmd;

    // NOTE: Command buffers are dispatchable types. Their creation must go through our
    //  own hook, not the dispatch table, to ensure the dispatch pointer fixup runs.
    VK_CHECK(vkShade_AllocateCommandBuffers(m_device.handle, &cmdBufAllocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(m_device.dispatch.BeginCommandBuffer(cmd, &beginInfo));

    // Transition UNDEFINED -> TRANSFER_DST_OPTIMAL
    this->transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent = {
            .width = m_extent.width,
            .height = m_extent.height,
            .depth = 1
        }
    };

    m_device.dispatch.CmdCopyBufferToImage(cmd, staging.buffer(), m_image,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // Transition TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    this->transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VK_CHECK(m_device.dispatch.EndCommandBuffer(cmd));

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    VK_CHECK(m_device.dispatch.QueueSubmit(m_device.queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(m_device.dispatch.QueueWaitIdle(m_device.queue));

    m_device.dispatch.FreeCommandBuffers(m_device.handle, m_device.commandPool, 1, &cmd);
}
