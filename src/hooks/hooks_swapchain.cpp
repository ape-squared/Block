#include "gui/gui_manager.hpp"
#include "runtime/runtime.hpp"
#include "hooks.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "config/config_manager.hpp"
#include "core/event_bus.hpp"
#include "core/logger.hpp"
#include "core/service_locator.hpp"
#include "input/input_manager.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

std::unordered_map<VkSwapchainKHR, vkShade::Runtime> g_runtimes;
std::mutex g_runtimeMutex;

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateSwapchainKHR(VkDevice                        device,
                                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                               const VkAllocationCallbacks*    pAllocator,
                                                               VkSwapchainKHR*                 pSwapchain)
{
    vkShade::Logger::trace("Intercepted VkCreateSwapchainKHR");

    auto& thisDevice = g_vulkanDevices[dispatch_key_from_handle(device)];

    // Add TRANSFER_SRC and TRANSFER_DST usages since we need to blit the image
    VkSwapchainCreateInfoKHR modifiedCreateInfo = *pCreateInfo;
    modifiedCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Forced HDR presentation: rewrite format/color space when configured and supported.
    // Surface queries are instance functions; the device dispatch table does not carry them.
    if (vkShade::Locator<vkShade::ConfigManager>::has())
    {
        auto& config = vkShade::Locator<vkShade::ConfigManager>::get().app();
        const bool forceHdr = config.get<bool>("HDR", "ForceHDR").value_or(false);
        if (forceHdr && pCreateInfo->surface != VK_NULL_HANDLE)
        {
            const std::string mode = config.get<std::string>("HDR", "Mode").value_or("scrgb");
            VkFormat desiredFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
            VkColorSpaceKHR desiredSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            if (mode == "hdr10")
            {
                desiredFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
                desiredSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT;
            }

            auto& instance = get_instance_from_handle(thisDevice.instance);
            if (instance.dispatch.GetPhysicalDeviceSurfaceFormatsKHR)
            {
                uint32_t formatCount = 0;
                instance.dispatch.GetPhysicalDeviceSurfaceFormatsKHR(
                    thisDevice.physicalDevice, pCreateInfo->surface, &formatCount, nullptr);
                std::vector<VkSurfaceFormatKHR> formats(formatCount);
                if (formatCount > 0)
                {
                    instance.dispatch.GetPhysicalDeviceSurfaceFormatsKHR(
                        thisDevice.physicalDevice, pCreateInfo->surface, &formatCount, formats.data());

                    bool supported = false;
                    for (const auto& fmt : formats)
                    {
                        if (fmt.format == desiredFormat && fmt.colorSpace == desiredSpace)
                        {
                            supported = true;
                            break;
                        }
                    }

                    if (supported)
                    {
                        modifiedCreateInfo.imageFormat = desiredFormat;
                        modifiedCreateInfo.imageColorSpace = desiredSpace;
                        vkShade::Logger::info("Forced HDR swapchain: {} / {}",
                            static_cast<int>(desiredFormat), static_cast<int>(desiredSpace));
                    }
                    else
                    {
                        vkShade::Logger::warn("Forced HDR requested but surface does not support format pair; using app format");
                    }
                }
            }
        }
    }

    // Call through to create the actual swapchain
    VkResult result = thisDevice.dispatch.CreateSwapchainKHR(device, &modifiedCreateInfo, pAllocator, pSwapchain);
    if (result != VK_SUCCESS)
        return result;

    // Create and store swapchain object using the actual create info (may be HDR)
    {
        std::lock_guard<std::mutex> lock(g_runtimeMutex);
        g_runtimes.try_emplace(*pSwapchain, thisDevice, *pSwapchain, modifiedCreateInfo);
    }

    vkShade::Logger::debug("Swapchain created");
    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_DestroySwapchainKHR(VkDevice                     device,
                                                            VkSwapchainKHR               swapchain,
                                                            const VkAllocationCallbacks* pAllocator)
{
    vkShade::Logger::trace("Intercepted VkDestroySwapchainKHR");

    // Clean up our bookkeeping data
    {
        std::lock_guard<std::mutex> lock(g_runtimeMutex);
        g_runtimes.erase(swapchain);
    }

    // Call through
    auto& thisDevice = g_vulkanDevices[dispatch_key_from_handle(device)];
    thisDevice.dispatch.DestroySwapchainKHR(device, swapchain, pAllocator);

    vkShade::Logger::debug("Swapchain destroyed");
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    // Get device
    auto& thisDevice = g_vulkanDevices[dispatch_key_from_handle(queue)];

    if (!pPresentInfo || pPresentInfo->swapchainCount == 0 || !pPresentInfo->pSwapchains)
        return thisDevice.dispatch.QueuePresentKHR(queue, pPresentInfo);

    VkSemaphore signalSemaphore = VK_NULL_HANDLE;
    bool layerRendered = false;

    {
        // Held across runtime lookup and the layer's own render. Released before
        // QueuePresentKHR so present cannot deadlock a thread that needs this mutex.
        std::lock_guard<std::mutex> lock(g_runtimeMutex);

        auto it = g_runtimes.find(pPresentInfo->pSwapchains[0]);
        if (it == g_runtimes.end())
        {
            vkShade::Logger::error("Swapchain not found in present");
        }
        else
        {
            auto& runtime = it->second;

            // Create the GUI Manager if it doesn't exist yet
            if (!vkShade::Locator<vkShade::GuiManager>::has())
                vkShade::Locator<vkShade::GuiManager>::emplace(thisDevice, runtime.format());

            if (vkShade::Locator<vkShade::ConfigManager>::has())
                vkShade::Locator<vkShade::ConfigManager>::get().update();

            auto& input = vkShade::Locator<vkShade::InputManager>::get();
            auto& gui = vkShade::Locator<vkShade::GuiManager>::get();
            auto& eventBus = vkShade::Locator<vkShade::EventBus>::get();

            // Update managers — input before event bus so same-frame delivery works
            input.update();
            eventBus.update();

            // Test input and shader loading
            if (input.is_action_just_pressed("ToggleGui") && !input.want_text_input())
            {
                gui.visible(!gui.visible());
            }

            input.set_overlay_visible(gui.visible());
            gui.update(1.f/60.f, runtime.extent());

            {
                ImGuiIO& io = ImGui::GetIO();
                input.set_imgui_capture(io.WantCaptureMouse, io.WantCaptureKeyboard, io.WantTextInput);
            }

#if defined(_WIN32)
            // Scale mouse client coords to swapchain extent when DPI / letterboxing differs
            if (auto* hwnd = reinterpret_cast<HWND>(nullptr); false) { (void)hwnd; }
#endif

            // Scale display for runtime uniforms from client size if available via extent ratio
            runtime.set_display_scale(1.0f, 1.0f);

            bool missingSwapchain = false;
            bool trackedDepth = false;

            // For each swapchain being presented
            for (uint32_t i = 0; i < pPresentInfo->swapchainCount; i++)
            {
                VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[i];
                uint32_t imageIndex = pPresentInfo->pImageIndices[i];

                auto swapIt = g_runtimes.find(swapchain);
                if (swapIt == g_runtimes.end())
                {
                    vkShade::Logger::error("Swapchain not found in present");
                    missingSwapchain = true;
                    break;
                }

                auto& scRuntime = swapIt->second;

                // Select from copies submitted before this present, then hand the image to the
                // runtime. begin_frame() runs after rendering so it does not wipe that choice.
                if (thisDevice.depthTracker)
                {
                    thisDevice.depthTracker->on_present(scRuntime.extent());
                    scRuntime.set_depth_image(thisDevice.depthTracker->sampled_depth(),
                                              thisDevice.depthTracker->has_depth());
                    trackedDepth = true;
                }

                // First swapchain waits on the app's present waits; later ones wait on previous signal
                if (i == 0)
                {
                    signalSemaphore = scRuntime.render(
                        imageIndex, pPresentInfo->waitSemaphoreCount, pPresentInfo->pWaitSemaphores);
                }
                else
                {
                    signalSemaphore = scRuntime.render(imageIndex, 1, &signalSemaphore);
                }
            }

            if (trackedDepth && thisDevice.depthTracker)
                thisDevice.depthTracker->begin_frame();

            layerRendered = !missingSwapchain;
        }
    }

    if (!layerRendered)
        return thisDevice.dispatch.QueuePresentKHR(queue, pPresentInfo);

    // Present must wait on the layer's signal semaphore
    VkPresentInfoKHR modifiedPresent = *pPresentInfo;
    modifiedPresent.waitSemaphoreCount = 1;
    modifiedPresent.pWaitSemaphores = &signalSemaphore;

    return thisDevice.dispatch.QueuePresentKHR(queue, &modifiedPresent);
}
