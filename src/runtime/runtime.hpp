#pragma once

#include <chrono>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "core/events/reload_effects.hpp"
#include "platform/file_watcher.hpp"
#include "vk/object.hpp"
#include "reshade_effect.hpp"

namespace vkShade
{
    class VulkanImage;

    class Runtime : public VulkanObject
    {
    public:
        Runtime(VulkanDevice& device, VkSwapchainKHR swapchain, VkSwapchainCreateInfoKHR swapchainInfo);
        ~Runtime();

        VkExtent2D extent() const { return m_extent; }
        VkFormat format() const { return m_format; }
        VulkanImage& image(size_t index) const;
        uint32_t image_count() const { return m_images.size(); }

        const ReshadeEffect* get_effect(std::string_view name) const
        {
            for (const auto& effect : m_effects)
            {
                if (effect->name() == name)
                    return effect.get();
            }

            return nullptr;
        }

        void on_effects_changed(const std::string& key, std::vector<std::string> effects);
        void on_techniques_changed(const std::string& key, std::vector<std::string> techniques);
        void on_reload_effects(const Events::ReloadEffects& event);

        // Render layer on top of swapchain. Waits on the app's present wait
        // semaphores and returns a semaphore the present must wait on.
        VkSemaphore render(uint32_t imageIndex,
                           uint32_t waitSemaphoreCount,
                           const VkSemaphore* waitSemaphores);

        ReshadeEffect* get_effect_mutable(std::string_view name);

        void set_depth_image(VulkanImage* depthImage, bool hasDepth);
        bool has_depth() const { return m_hasDepth; }
        VulkanImage* depth_image() const { return m_depthImage; }

        VkColorSpaceKHR color_space() const { return m_colorSpace; }
        void set_overlay_focus(uint32_t active, uint32_t hovered);
        void set_display_scale(float scaleX, float scaleY);

    private:
        using Clock = std::chrono::steady_clock;

        // Give the layer's render submission up to 10 seconds to complete. A timeout
        //  indicates an abnormal condition, so we'll abort rather than hang.
        static constexpr uint64_t FENCE_TIMEOUT_NS = 10'000'000'000;

        // Swapchain resources
        VkSwapchainKHR m_swapchain;
        VkFormat m_format;
        VkExtent2D m_extent;
        VkColorSpaceKHR m_colorSpace;
        std::vector<std::unique_ptr<VulkanImage>> m_images;
        std::unique_ptr<VulkanImage> m_depthDummy;
        VulkanImage* m_depthImage {nullptr};
        bool m_hasDepth {false};

        // Rendering resources
        VkFence m_fence {VK_NULL_HANDLE};
        VkSemaphore m_presentSemaphore {VK_NULL_HANDLE};
        VkCommandPool m_commandPool {VK_NULL_HANDLE};
        VkCommandBuffer m_commandBuffer {VK_NULL_HANDLE};
        struct TechniqueRef
        {
            std::shared_ptr<ReshadeEffect> effect;
            size_t techniqueIndex {0};
        };

        std::vector<std::shared_ptr<ReshadeEffect>> m_effects;
        std::vector<TechniqueRef> m_schedule;
        std::vector<std::unique_ptr<Platform::FileWatcher>> m_effectWatchers;
        std::shared_ptr<VulkanImage> m_pingPongA;
        std::shared_ptr<VulkanImage> m_pingPongB;

        uint32_t m_overlayActive {0};
        uint32_t m_overlayHovered {0};
        float m_displayScaleX {1.0f};
        float m_displayScaleY {1.0f};

        // ReShade runtime uniform state
        Clock::time_point m_start = Clock::now();
        Clock::time_point m_lastFrame = m_start;
        uint64_t m_frameCount {0};

        void rebuild_schedule();
        void watch_effect_sources();
        void poll_effect_changes();
        bool reload_single_effect(const std::string& effectName);

        [[nodiscard]]
        ReshadeFrameState update_time();
    };
}  // namespace vkShade
