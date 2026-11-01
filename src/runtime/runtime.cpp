#include "runtime.hpp"

#include <algorithm>
#include <unordered_set>

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <magic_enum/magic_enum.hpp>
#include <string_view>

#include "config/config_manager.hpp"
#include "core/event_bus.hpp"
#include "core/logger.hpp"
#include "core/service_locator.hpp"
#include "gui/gui_manager.hpp"
#include "hooks/hooks.hpp"
#include "input/input_manager.hpp"
#include "vk/image.hpp"
#include "vk/initializers.hpp"
#include "vk/macros.hpp"
#include "reshade_uniforms.hpp"

vkShade::Runtime::Runtime(VulkanDevice& device, VkSwapchainKHR swapchain, VkSwapchainCreateInfoKHR swapchainInfo)
    : VulkanObject(device)
{
    // Store the swapchain info
    m_swapchain = swapchain;
    m_format = swapchainInfo.imageFormat;
    m_extent = swapchainInfo.imageExtent;
    m_colorSpace = swapchainInfo.imageColorSpace;

    // Get swapchain images
    uint32_t imageCount = 0;
    VK_CHECK(m_device.dispatch.GetSwapchainImagesKHR(m_device.handle, swapchain, &imageCount, nullptr));
    std::vector<VkImage> images(imageCount);
    VK_CHECK(m_device.dispatch.GetSwapchainImagesKHR(m_device.handle, swapchain, &imageCount, images.data()));

    // Create image views
    for (auto& image : images)
    {
        m_images.push_back(std::make_unique<VulkanImage>(device, image, m_extent, m_format));
    }

	VkImageUsageFlags drawImageUsages {};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    m_pingPongA = std::shared_ptr<VulkanImage>(new VulkanImage(m_device, m_extent, m_format, drawImageUsages));
    m_pingPongB = std::shared_ptr<VulkanImage>(new VulkanImage(m_device, m_extent, m_format, drawImageUsages));

    // Create a dummy depth image.
	VkImageUsageFlags depthImageUsages {};
	depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    m_depthDummy = std::make_unique<VulkanImage>(m_device, m_extent, VK_FORMAT_R32_SFLOAT, depthImageUsages);

    // Create command pool
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_device.queueFamilyIndex,
    };

    VK_CHECK(m_device.dispatch.CreateCommandPool(m_device.handle, &poolInfo, nullptr, &m_commandPool));

    // Create the fence
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VK_CHECK(m_device.dispatch.CreateFence(m_device.handle, &fenceInfo, nullptr, &m_fence));

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VK_CHECK(m_device.dispatch.CreateSemaphore(m_device.handle, &semaphoreInfo, nullptr, &m_presentSemaphore));

    m_depthImage = m_depthDummy.get();

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    // NOTE: Command buffers are dispatchable types. Their creation must go through our
    //  own hook, not the dispatch table, to ensure the dispatch pointer fixup runs.
    VK_CHECK(vkShade_AllocateCommandBuffers(m_device.handle, &allocInfo, &m_commandBuffer));

    auto& preset = vkShade::Locator<vkShade::ConfigManager>::get().preset();

    // Subscribe to config changes
    preset.on_changed("", "Effects").connect<&Runtime::on_effects_changed>(this);
    preset.on_changed("", "Techniques").connect<&Runtime::on_techniques_changed>(this);
    preset.on_changed("", "TechniqueSorting").connect<&Runtime::on_techniques_changed>(this);

    // Load the current effects list
    if (auto effects = preset.get<std::vector<std::string>>("", "Effects"))
    {
        this->on_effects_changed("Effects", effects.value());
    }

    // Subscribe to events
    auto& eventBus = Locator<EventBus>::get();
    eventBus.sink<Events::ReloadEffects>().connect<&Runtime::on_reload_effects>(this);
}

vkShade::Runtime::~Runtime()
{
    // Wait for the command buffer to finish executing
    VK_CHECK(m_device.dispatch.WaitForFences(m_device.handle, 1, &m_fence, VK_TRUE, FENCE_TIMEOUT_NS));

    // Clean up
    m_device.dispatch.DestroyCommandPool(m_device.handle, m_commandPool, nullptr);
    m_device.dispatch.DestroyFence(m_device.handle, m_fence, nullptr);
    if (m_presentSemaphore != VK_NULL_HANDLE)
        m_device.dispatch.DestroySemaphore(m_device.handle, m_presentSemaphore, nullptr);

    // Unsubscribe from events
    auto& eventBus = Locator<EventBus>::get();
    eventBus.sink<Events::ReloadEffects>().disconnect<&Runtime::on_reload_effects>(this);

    // Unsubscribe from config changes
    auto& preset = vkShade::Locator<vkShade::ConfigManager>::get().preset();
    preset.on_changed("", "Effects").disconnect<&Runtime::on_effects_changed>(this);
    preset.on_changed("", "Techniques").disconnect<&Runtime::on_techniques_changed>(this);
    preset.on_changed("", "TechniqueSorting").disconnect<&Runtime::on_techniques_changed>(this);
}

vkShade::ReshadeEffect* vkShade::Runtime::get_effect_mutable(std::string_view name)
{
    for (auto& effect : m_effects)
    {
        if (effect && effect->name() == name)
            return effect.get();
    }
    return nullptr;
}

void vkShade::Runtime::rebuild_schedule()
{
    m_schedule.clear();
    auto& preset = Locator<ConfigManager>::get().preset();

    auto techniques = preset.get<std::vector<std::string>>("", "Techniques");
    auto sorting = preset.get<std::vector<std::string>>("", "TechniqueSorting");

    auto parse_ref = [](const std::string& token) -> std::pair<std::string, std::string>
    {
        // Name@File.fx or just File.fx
        const auto at = token.find('@');
        if (at == std::string::npos)
            return { "", token };
        return { token.substr(0, at), token.substr(at + 1) };
    };

    if (techniques && !techniques->empty())
    {
        std::vector<std::string> order = sorting.value_or(*techniques);
        // Ensure all enabled techniques appear somewhere
        for (const auto& t : *techniques)
        {
            if (std::find(order.begin(), order.end(), t) == order.end())
                order.push_back(t);
        }

        for (const auto& token : order)
        {
            if (std::find(techniques->begin(), techniques->end(), token) == techniques->end())
                continue; // disabled / not in Techniques list

            auto [techName, fileName] = parse_ref(token);
            for (auto& effect : m_effects)
            {
                if (!effect || effect->name() != fileName)
                    continue;
                for (size_t i = 0; i < effect->technique_count(); ++i)
                {
                    const auto& info = effect->techniques()[i];
                    if (!techName.empty() && info.name != techName)
                        continue;
                    effect->set_technique_enabled(i, true);
                    m_schedule.push_back({ effect, i });
                }
            }
        }

        // Disable techniques not in the Techniques list
        for (auto& effect : m_effects)
        {
            if (!effect)
                continue;
            for (size_t i = 0; i < effect->technique_count(); ++i)
            {
                const auto& info = effect->techniques()[i];
                const std::string key = info.name + "@" + effect->name();
                const bool enabled = std::find(techniques->begin(), techniques->end(), key) != techniques->end()
                    || std::find(techniques->begin(), techniques->end(), effect->name()) != techniques->end();
                effect->set_technique_enabled(i, enabled);
            }
        }
        return;
    }

    // Legacy Effects= fallback: enable all techniques of listed files
    for (auto& effect : m_effects)
    {
        if (!effect)
            continue;
        for (size_t i = 0; i < effect->technique_count(); ++i)
        {
            effect->set_technique_enabled(i, true);
            m_schedule.push_back({ effect, i });
        }
    }
}

void vkShade::Runtime::watch_effect_sources()
{
    m_effectWatchers.clear();
    for (const auto& effect : m_effects)
    {
        if (!effect)
            continue;
        for (const auto& path : effect->source_files())
        {
            auto watcher = Platform::FileWatcher::create();
            if (watcher && watcher->watch(path))
                m_effectWatchers.push_back(std::move(watcher));
        }
    }
}

void vkShade::Runtime::poll_effect_changes()
{
    bool any = false;
    for (auto& watcher : m_effectWatchers)
    {
        if (watcher && watcher->changed())
            any = true;
    }
    if (!any)
        return;

    // Reload all effects safely (one at a time with fence wait inside)
    auto names = std::vector<std::string>{};
    for (const auto& effect : m_effects)
    {
        if (effect)
            names.push_back(effect->name());
    }
    for (const auto& name : names)
        reload_single_effect(name);
}

bool vkShade::Runtime::reload_single_effect(const std::string& effectName)
{
    auto& config = Locator<ConfigManager>::get().app();
    auto searchPaths = config.get<std::vector<std::string>>("ReShade", "EffectSearchPaths");

    std::filesystem::path foundPath;
    for (const auto& path : searchPaths.value_or(std::vector<std::string>{}))
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec))
            continue;
        for (const auto& entry : std::filesystem::directory_iterator(path, ec))
        {
            if (entry.path().filename() == effectName)
            {
                foundPath = entry.path();
                break;
            }
        }
        if (!foundPath.empty())
            break;
    }

    if (foundPath.empty())
        return false;

    VK_CHECK(m_device.dispatch.WaitForFences(m_device.handle, 1, &m_fence, VK_TRUE, FENCE_TIMEOUT_NS));

    try
    {
        SwapchainInfo swapchainInfo = {
            .extent = m_extent,
            .format = m_format,
            .colorSpace = m_colorSpace,
        };
        auto replacement = std::make_shared<ReshadeEffect>(m_device, swapchainInfo, foundPath);

        for (auto& effect : m_effects)
        {
            if (effect && effect->name() == effectName)
            {
                effect = std::move(replacement);
                rebuild_schedule();
                watch_effect_sources();
                Logger::info("Hot-reloaded effect: {}", effectName);
                return true;
            }
        }
    }
    catch (const std::runtime_error& exception)
    {
        Logger::error("Hot-reload failed for {}: {}", effectName, exception.what());
    }
    return false;
}

void vkShade::Runtime::on_techniques_changed(const std::string&, std::vector<std::string>)
{
    rebuild_schedule();
}

void vkShade::Runtime::on_effects_changed(const std::string& key, std::vector<std::string> effects)
{
    auto& config = vkShade::Locator<vkShade::ConfigManager>::get().app();
    auto searchPaths = config.get<std::vector<std::string>>("ReShade", "EffectSearchPaths");

    // Wait for the command buffer to finish executing
    VK_CHECK(m_device.dispatch.WaitForFences(m_device.handle, 1, &m_fence, VK_TRUE, FENCE_TIMEOUT_NS));

    m_effects.clear();
    std::vector<std::string> loadedEffects;
    for (const auto& effect : effects)
    {
        bool found = false;
        bool error = false;
        for (const auto& path : searchPaths.value_or(std::vector<std::string>{}))
        {
            if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
            {
                for (const auto& entry : std::filesystem::directory_iterator(path))
                {
                    if (entry.path().filename() == effect)
                    {
                        try {
                            SwapchainInfo swapchainInfo = {
                                .extent = m_extent,
                                .format = m_format,
                                .colorSpace = m_colorSpace,
                            };
                            m_effects.push_back(std::make_shared<ReshadeEffect>(m_device, swapchainInfo, entry.path()));
                            loadedEffects.push_back(effect);
                            found = true;
                        } catch (const std::runtime_error& exception) {
                            Logger::error(exception.what());
                            error = true;
                        }
                        break;
                    }
                }
            }
        }

        if (!found && !error)
            Logger::warn("Unable to find effect: {}", effect);
    }

    auto& internalCfg = vkShade::Locator<vkShade::ConfigManager>::get().internal();
    internalCfg.set("__INTERNAL__", "LoadedEffects", loadedEffects);

    rebuild_schedule();
    watch_effect_sources();
}

void vkShade::Runtime::on_reload_effects(const Events::ReloadEffects& event)
{
    if (!event.effect.empty())
    {
        reload_single_effect(event.effect);
        return;
    }

    auto& preset = vkShade::Locator<vkShade::ConfigManager>::get().preset();
    auto effects = preset.get<std::vector<std::string>>("", "Effects");
    this->on_effects_changed("Effects", effects.value_or(std::vector<std::string>{}));
}

void vkShade::Runtime::set_depth_image(VulkanImage* depthImage, bool hasDepth)
{
    m_hasDepth = hasDepth;
    m_depthImage = (hasDepth && depthImage) ? depthImage : m_depthDummy.get();
}

void vkShade::Runtime::set_overlay_focus(uint32_t active, uint32_t hovered)
{
    m_overlayActive = active;
    m_overlayHovered = hovered;
}

void vkShade::Runtime::set_display_scale(float scaleX, float scaleY)
{
    m_displayScaleX = scaleX > 0.0f ? scaleX : 1.0f;
    m_displayScaleY = scaleY > 0.0f ? scaleY : 1.0f;
}

VkSemaphore vkShade::Runtime::render(uint32_t imageIndex,
                                     uint32_t waitSemaphoreCount,
                                     const VkSemaphore* waitSemaphores)
{
    poll_effect_changes();

    VulkanImage* swapchainImage = m_images.at(imageIndex).get();
    const auto reshadeFrameState = this->update_time();

    // Wait until the previous command buffer has finished executing.
	VK_CHECK(m_device.dispatch.WaitForFences(m_device.handle, 1, &m_fence, true, FENCE_TIMEOUT_NS));
	VK_CHECK(m_device.dispatch.ResetFences(m_device.handle, 1, &m_fence));

    // Reset the command buffer
	VK_CHECK(m_device.dispatch.ResetCommandBuffer(m_commandBuffer, 0));

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(m_device.dispatch.BeginCommandBuffer(m_commandBuffer, &beginInfo));

    // Blit swapchain image to ping-pong and transition to COLOR_ATTACHMENT
    swapchainImage->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    m_pingPongA->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    swapchainImage->blit_to(m_commandBuffer, m_pingPongA->image());
    m_pingPongA->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Keep dummy depth filled when no real depth is available
    if (!m_hasDepth)
    {
        m_depthDummy->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue clearValue = { .float32 = { 1.0f, 0.0f, 0.0f, 0.0f } };
        VkImageSubresourceRange range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
        m_device.dispatch.CmdClearColorImage(m_commandBuffer, m_depthDummy->image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
        m_depthDummy->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_depthImage = m_depthDummy.get();
    }

    // Render effects if enabled
    auto& input = vkShade::Locator<vkShade::InputManager>::get();
    auto& internalCfg = vkShade::Locator<vkShade::ConfigManager>::get().internal();
    const bool effectsEnabled = internalCfg.get<bool>("__INTERNAL__", "EffectsEnabled").value_or(true);
    if (input.is_action_just_pressed("ToggleEffects") && !input.want_text_input())
        internalCfg.set<bool>("__INTERNAL__", "EffectsEnabled", !effectsEnabled);

    VulkanImage* readImage = m_pingPongA.get();
    VulkanImage* writeImage = m_pingPongB.get();
    VulkanImage& depthImage = *(m_depthImage ? m_depthImage : m_depthDummy.get());

    if (effectsEnabled)
    {
        // Update each effect once per frame before its first technique
        std::unordered_set<ReshadeEffect*> updated;
        for (const auto& ref : m_schedule)
        {
            if (!ref.effect)
                continue;
            if (updated.insert(ref.effect.get()).second)
                ref.effect->update(reshadeFrameState);
        }

        for (const auto& ref : m_schedule)
        {
            if (!ref.effect || !ref.effect->technique_enabled(ref.techniqueIndex))
                continue;

            readImage->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            ref.effect->bind_input_technique(*readImage, depthImage, ref.techniqueIndex);

            if (ref.effect->apply_technique(m_commandBuffer, *writeImage, ref.techniqueIndex))
                std::swap(readImage, writeImage);
        }
    }

    VulkanImage* finalImage = readImage;

    // Barrier: Ensure final image is ready to sample
    finalImage->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Begin dynamic rendering using our final image
    std::array<VkRenderingAttachmentInfo, 1> colorAttachments = {
        vkinit::rendering_attachment_info(finalImage->image_view(), nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
    };

    VkRenderingInfo renderingInfo = vkinit::rendering_info(m_extent, colorAttachments, nullptr);
    m_device.dispatch.CmdBeginRendering(m_commandBuffer, &renderingInfo);

    // Render ImGui on top of everything
    ImDrawData* imguiDrawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(imguiDrawData, m_commandBuffer);

    // End dynamic rendering for ImGui
    m_device.dispatch.CmdEndRendering(m_commandBuffer);

    // Blit the final image back to swapchain and transition to PRESENT_SRC
    readImage->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    swapchainImage->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    readImage->blit_to(m_commandBuffer, swapchainImage->image());
    swapchainImage->transition_layout(m_commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // End and submit
    VK_CHECK(m_device.dispatch.EndCommandBuffer(m_commandBuffer));

    std::vector<VkPipelineStageFlags> waitStages(
        waitSemaphoreCount, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = waitSemaphoreCount,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &m_commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_presentSemaphore,
    };

    VK_CHECK(m_device.dispatch.QueueSubmit(m_device.queue, 1, &submitInfo, m_fence));
    m_frameCount++;
    return m_presentSemaphore;
}

vkShade::VulkanImage& vkShade::Runtime::image(size_t index) const
{
    assert(index < m_images.size());
    return *m_images[index];
}

vkShade::ReshadeFrameState vkShade::Runtime::update_time()
{
    ReshadeFrameState state {};

    Clock::time_point now = Clock::now();

    state.frameTime = reshade_frame_time(now - m_lastFrame);
    state.frameCount = static_cast<uint32_t>(m_frameCount % std::numeric_limits<uint32_t>::max());
    state.timer = reshade_frame_time(now - m_start);
    state.hasDepth = m_hasDepth;
    state.screenshot = false;

    if (Locator<InputManager>::has())
    {
        auto& input = Locator<InputManager>::get();
        const auto mouse = input.mouse_position();
        state.mousePoint = {
            mouse.x * m_displayScaleX,
            mouse.y * m_displayScaleY,
        };
        {
            const auto delta = input.mouse_delta();
            state.mouseDelta = { delta.x, delta.y };
        }
        state.mouseWheel = input.mouse_wheel_delta();
        for (int i = 0; i < 5; ++i)
        {
            state.mouseButtonsDown[i] = input.is_mouse_down(i);
            state.mouseButtonsPressed[i] = input.is_mouse_just_pressed(i);
        }
        for (uint32_t vk = 0; vk < 256; ++vk)
        {
            state.keysDown[vk] = input.is_virtual_key_down(vk);
            state.keysPressed[vk] = input.is_virtual_key_just_pressed(vk);
        }
    }

    if (Locator<GuiManager>::has())
    {
        auto& gui = Locator<GuiManager>::get();
        state.overlayOpen = gui.visible();
        state.overlayActive = state.overlayOpen ? m_overlayActive : 0;
        state.overlayHovered = state.overlayOpen ? m_overlayHovered : 0;
    }

    m_lastFrame = now;

    return state;
}
