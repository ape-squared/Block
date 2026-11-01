#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

// Defined in hooks/hooks.hpp. Forward declared here because hooks.hpp embeds a
// DepthTracker in every VulkanDevice (so this header can't include it back).
struct VulkanDevice;

namespace vkShade
{
    class VulkanImage;

    // Tracks the application's depth buffers for a single VulkanDevice and keeps a
    // layer-owned, sampleable copy of the one that best matches the swapchain.
    //
    // Threading: the Vulkan hooks run on whatever thread the application records
    // commands on, so every public method is internally synchronised.
    //
    // Limitations: only dynamic rendering (vkCmdBeginRendering) is tracked, MSAA and
    // array/mipmapped depth images are tracked but never copied, and draws recorded in
    // secondary command buffers are invisible to the draw statistics. A copy is visible
    // to the runtime only after the command buffer that recorded it has been submitted.
    class DepthTracker
    {
    public:
        explicit DepthTracker(VulkanDevice& device);
        ~DepthTracker();

        DepthTracker(const DepthTracker&) = delete;
        DepthTracker& operator=(const DepthTracker&) = delete;
        DepthTracker(DepthTracker&&) = delete;
        DepthTracker& operator=(DepthTracker&&) = delete;

        // ---- Consumer API ----

        // True when a copy of the selected depth buffer was submitted into the current sampled image.
        bool has_depth() const;

        // Layer-owned depth image holding the copy (nullptr until one was created).
        VulkanImage* sampled_depth() const;

        // Call once per present, before the runtime renders: selects the depth buffer
        // for this frame and (re)creates the sampled image to match it.
        void on_present(VkExtent2D swapchainExtent);

        // Call once per present, after the runtime rendered: resets per-frame statistics.
        void begin_frame();

        // ---- Hook API ----

        // Fills `out` with a modified create info (depth images gain TRANSFER_SRC and,
        // when supported, SAMPLED usage). Returns false if `in` must be used untouched.
        bool adjust_image_create_info(const VkImageCreateInfo& in, VkImageCreateInfo& out);
        void on_image_created(VkImage image, const VkImageCreateInfo& createInfo);
        void on_image_destroyed(VkImage image);

        void on_image_view_created(const VkImageViewCreateInfo& createInfo, VkImageView view);
        void on_image_view_destroyed(VkImageView view);

        // Call before forwarding vkCmdBeginRendering (may record a copy-before-clear).
        void on_begin_rendering(VkCommandBuffer cmd, const VkRenderingInfo* renderingInfo);
        // Call after forwarding vkCmdEndRendering (may record the depth copy).
        void on_end_rendering(VkCommandBuffer cmd);

        void on_draw(VkCommandBuffer cmd, uint32_t drawCount, uint64_t vertexCount);

        // Call before forwarding vkCmdClearDepthStencilImage (records copy-before-clear).
        void on_clear_depth_stencil_image(VkCommandBuffer cmd, VkImage image, VkImageLayout layout);
        // Call before forwarding vkCmdClearAttachments. A transfer can't be recorded inside a
        // render scope, so this only records that the depth attachment was cleared mid-pass.
        void on_clear_attachments(VkCommandBuffer cmd, uint32_t attachmentCount, const VkClearAttachment* attachments);

        // A reset command buffer no longer contains any copy recorded earlier.
        void on_begin_command_buffer(VkCommandBuffer cmd);

        // Copies only count once the command buffer that recorded them is submitted.
        void on_queue_submit(uint32_t submitCount, const VkSubmitInfo* pSubmits);
        void on_queue_submit2(uint32_t submitCount, const VkSubmitInfo2* pSubmits);

    private:
        // Number of presents a replaced sampled image is kept alive for, so command
        // buffers that were already recorded against it can still be submitted.
        static constexpr uint32_t RETIRE_FRAMES = 8;

        struct ImageInfo
        {
            VkFormat   format {VK_FORMAT_UNDEFINED};
            VkExtent2D extent {0, 0};
            bool       copyable {false};  // Has TRANSFER_SRC usage and is a plain 2D single-sample image

            // Per-frame statistics
            uint64_t draws {0};
            uint64_t vertices {0};
            bool     rendered {false};
        };

        // State of the dynamic rendering instance currently recording in a command buffer
        struct CmdState
        {
            VkImage       image {VK_NULL_HANDLE};
            VkImageLayout layout {VK_IMAGE_LAYOUT_UNDEFINED};
            bool          storeDepth {false};
            bool          suspending {false};
            bool          clearedMidPass {false};
            uint64_t      draws {0};
            uint64_t      vertices {0};
        };

        struct CopyPlan
        {
            bool       valid {false};
            VkImage    src {VK_NULL_HANDLE};
            VkFormat   srcFormat {VK_FORMAT_UNDEFINED};
            VkImage    dst {VK_NULL_HANDLE};
            VkFormat   dstFormat {VK_FORMAT_UNDEFINED};
            VkExtent2D extent {0, 0};
        };

        // A copy baked into a command buffer. `dst` is the sampled image that was current
        // when the copy was recorded; a later recreate of that image invalidates it.
        struct RecordedCopy
        {
            VkCommandBuffer cmd {VK_NULL_HANDLE};
            VkImage         src {VK_NULL_HANDLE};
            VkImage         dst {VK_NULL_HANDLE};
        };

        struct RetiredImage
        {
            std::unique_ptr<VulkanImage> image;
            uint32_t                     framesLeft {0};
        };

        VulkanDevice& m_device;
        PFN_vkGetPhysicalDeviceFormatProperties m_getFormatProperties {nullptr};

        mutable std::mutex m_mutex;

        // Config
        bool m_enabled {true};
        bool m_useAspectRatio {true};
        bool m_useDrawStats {true};

        // Bookkeeping
        std::unordered_map<VkImage, ImageInfo>           m_images;
        std::unordered_map<VkImageView, VkImage>         m_views;
        std::unordered_map<VkCommandBuffer, CmdState>    m_cmdStates;
        std::unordered_map<VkFormat, VkFormatFeatureFlags> m_formatFeatures;
        std::unordered_map<VkFormat, VkFormat>           m_copyFormats;

        // Frame state
        VkExtent2D m_swapchainExtent {0, 0};
        VkImage    m_bestImage {VK_NULL_HANDLE};    // Best candidate seen so far this frame
        VkImage    m_selectedImage {VK_NULL_HANDLE};
        bool       m_hasDepth {false};
        uint32_t   m_midPassClears {0};

        // Copies recorded into command buffers, promoted into m_copiedThisFrame on submit.
        // Pending entries live until the command buffer is begun again so a baked command
        // buffer can be submitted on later frames.
        std::unordered_map<VkCommandBuffer, std::vector<RecordedCopy>> m_pendingCopies;
        std::vector<RecordedCopy> m_copiedThisFrame;

        // Layer-owned copy target
        std::unique_ptr<VulkanImage> m_sampled;
        VkFormat                     m_sampledFormat {VK_FORMAT_UNDEFINED};
        VkExtent2D                   m_sampledExtent {0, 0};
        std::vector<RetiredImage>    m_retired;

        void refresh_config();

        VkFormatFeatureFlags optimal_features_locked(VkFormat format);
        VkFormat             choose_copy_format_locked(VkFormat sourceFormat);

        bool passes_filters_locked(const ImageInfo& info) const;
        bool is_better_locked(const ImageInfo& a, const ImageInfo& b) const;
        VkImage select_best_locked() const;
        bool consider_best_locked(VkImage image);
        CopyPlan plan_copy_locked(VkImage image);

        void create_sampled_image_locked(VkFormat format, VkExtent2D extent);

        // False when the copy was skipped (unsupported layout, or no matching target).
        bool record_copy(VkCommandBuffer cmd, const CopyPlan& plan, VkImageLayout srcLayout);
        void remember_copy(VkCommandBuffer cmd, const CopyPlan& plan);
        void note_submitted_locked(VkCommandBuffer cmd);
        bool copy_submitted_locked(VkImage src) const;
    };
}  // namespace vkShade
