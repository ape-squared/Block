#pragma once

#include <filesystem>
#include <string>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "object.hpp"

namespace reshadefx
{
    struct texture;
    enum class texture_format : uint8_t;
}

namespace vkShade
{
    class VulkanImage : public VulkanObject
    {
    public:
        // Owning constructor: creates or loads an image according to info struct
        VulkanImage(VulkanDevice& device, const reshadefx::texture& info);

        // Owning constructor: creates image + view
        VulkanImage(VulkanDevice& device, VkExtent2D size, VkFormat format, VkImageUsageFlags usageFlags);

        // Non-owning constructor: wraps existing image + creates its own view
        VulkanImage(VulkanDevice& device, VkImage image, VkExtent2D size, VkFormat format);

        ~VulkanImage() override;

        // Blit FROM another image INTO this image (assumes same size, mip 0)
        void blit_from(VkCommandBuffer cmd, VkImage source);

        // Blit FROM this image INTO another image (assumes same size, mip 0)
        void blit_to(VkCommandBuffer cmd, VkImage destination);

        // Transition the image from its current layout into a new layout
        void transition_layout(VkCommandBuffer cmd, VkImageLayout newLayout);

        // Generate mip chain from level 0 (image must support TRANSFER_SRC|DST)
        void generate_mipmaps(VkCommandBuffer cmd);

        // Getters for handles
        const VkImage& image() const { return m_image; }
        const VkImageView& image_view() const { return m_imageView; }
        const VkImageView& srgb_image_view() const
        {
            return m_srgbImageView != VK_NULL_HANDLE ? m_srgbImageView : m_imageView;
        }
        const VkFormat format() const { return m_format; }
        const VkExtent3D extent() const { return m_extent; }
        uint32_t mip_levels() const { return m_mipLevels; }

    private:
        bool              m_owning = true;
        VkExtent3D        m_extent {0, 0, 0};
        VkImage           m_image = VK_NULL_HANDLE;
        VkImageView       m_imageView = VK_NULL_HANDLE;
        VkImageView       m_srgbImageView = VK_NULL_HANDLE;
        VkImageUsageFlags m_usageFlags = 0;
        VmaAllocation     m_allocation = VK_NULL_HANDLE;
        VkFormat          m_format = VK_FORMAT_UNDEFINED;
        VkImageLayout     m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t          m_mipLevels = 1;

        // The actual blit implementation
        void blit(VkCommandBuffer cmd, VkImage src, VkImage dest);

        void create_image();
        void create_image_view();

        void load_from_file(const std::string& filePath);

        void upload(const void* data, size_t size);

        static VkFormat convert_format(reshadefx::texture_format format);
        static VkFormat srgb_variant(VkFormat format);
        static std::filesystem::path find_texture_file(const std::string& fileName);
    };
} // namespace vkShade
