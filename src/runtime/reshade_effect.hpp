#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "core/logger.hpp"
#include "core/uniform.hpp"
#include "hooks/hooks.hpp"
#include "vk/buffer.hpp"
#include "vk/shader_module.hpp"
#include "vk/swapchain_info.hpp"
#include "reshade_uniforms.hpp"

namespace reshadefx
{
    enum class blend_factor : uint8_t;
    enum class blend_op : uint8_t;
    enum class primitive_topology : uint8_t;
    enum class stencil_func : uint8_t;
    enum class stencil_op : uint8_t;
    struct effect_module;
    struct type;
    struct uniform;
}

namespace vkShade
{
    class VulkanImage;
    class VulkanSampler;

    class ReshadeEffect : public VulkanObject
    {
    public:
        struct TechniqueInfo
        {
            std::string name;
            std::string uiLabel;
            std::string uiTooltip;
            bool hidden {false};
            bool enabled {true};
        };

        ReshadeEffect(VulkanDevice& device, SwapchainInfo swapchainInfo, std::filesystem::path effectPath);
        ~ReshadeEffect() override;

        // Apply all enabled techniques. Returns true if the backbuffer was written.
        bool apply(VkCommandBuffer cmd, VulkanImage& outputImage);
        // Apply a single technique by index. Returns true if backbuffer was written.
        bool apply_technique(VkCommandBuffer cmd, VulkanImage& outputImage, size_t techniqueIndex);

        void bind_input(VulkanImage& colorImage, VulkanImage& depthImage);
        void bind_input_technique(VulkanImage& colorImage, VulkanImage& depthImage, size_t techniqueIndex);
        void update(const ReshadeFrameState& frame);

        std::string name() const { return m_fileName; }
        const std::vector<Uniform>& uniforms() const { return m_uniforms; }
        const std::vector<TechniqueInfo>& techniques() const { return m_techniqueInfos; }
        size_t technique_count() const { return m_techniques.size(); }
        void set_technique_enabled(size_t index, bool enabled);
        bool technique_enabled(size_t index) const;

        const std::vector<std::filesystem::path>& source_files() const { return m_sourceFiles; }

        void set_overlay_focus(uint32_t active, uint32_t hovered);
        uint32_t overlay_active() const { return m_overlayActive; }
        uint32_t overlay_hovered() const { return m_overlayHovered; }

        ReshadeEffect* mutable_self() { return this; }

    private:
        struct Pass
        {
            VkPipeline            pipeline       {VK_NULL_HANDLE};
            VkPipelineLayout      pipelineLayout {VK_NULL_HANDLE};
            VkDescriptorSetLayout imageSetLayout {VK_NULL_HANDLE};
            VkDescriptorSet       imageSet       {VK_NULL_HANDLE};

            std::shared_ptr<ShaderModule> vertexShader;
            std::shared_ptr<ShaderModule> fragmentShader;
            bool generateMipmaps {false};
        };

        struct Technique
        {
            TechniqueInfo info;
            std::vector<Pass> passes;
            size_t moduleIndex {0};
        };

        std::string   m_fileName;
        SwapchainInfo m_swapchainInfo;

        std::unique_ptr<reshadefx::effect_module> m_module {nullptr};
        std::unique_ptr<VulkanBuffer> m_uniformBuffer {nullptr};
        std::vector<Uniform> m_uniforms;
        std::vector<std::unique_ptr<ReshadeUniform>> m_builtinUniforms;
        std::vector<TechniqueInfo> m_techniqueInfos;
        std::vector<std::filesystem::path> m_sourceFiles;

        VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
        VkDescriptorSetLayout m_uniformSetLayout {VK_NULL_HANDLE};
        VkDescriptorSet m_uniformSet {VK_NULL_HANDLE};

        std::unordered_map<std::string, std::unique_ptr<VulkanImage>> m_textures;
        std::vector<std::unique_ptr<VulkanSampler>> m_samplers;
        std::unique_ptr<VulkanImage> m_stencilBuffer;

        std::vector<Technique> m_techniques;

        uint32_t m_overlayActive {0};
        uint32_t m_overlayHovered {0};

        bool compile(std::filesystem::path filePath);

        Uniform* find_uniform(const std::string& name)
        {
            for (auto& uniform : m_uniforms)
            {
                if (uniform.name == name)
                    return &uniform;
            }

            return nullptr;
        }

        template<typename T>
        void on_uniform_changed(const std::string& name, T value)
        {
            const Uniform* uniform = find_uniform(name);
            if (!uniform)
            {
                Logger::error("Unknown uniform '{}'", name);
                return;
            }

            if (uniform->baseType != UniformTraits<T>::base ||
                uniform->components != UniformTraits<T>::components)
            {
                Logger::error("Type mismatch for uniform '{}'", name);
                return;
            }

            if (!m_uniformBuffer)
            {
                Logger::error("Uniform buffer is not valid: {}", m_fileName);
                return;
            }

            if (!m_uniformBuffer->write(&value, sizeof(T), uniform->offset))
            {
                Logger::error("Failed to write uniform '{}'", name);
                return;
            }
        }

        void reflect_descriptors();
        void reflect_images();
        void reflect_pipeline();
        void reflect_samplers();
        void reflect_uniforms();

        bool apply_technique_internal(VkCommandBuffer cmd, VulkanImage& outputImage, Technique& technique);

        static VkBlendFactor       convert_blend_factor(reshadefx::blend_factor blendFactor);
        static VkBlendOp           convert_blend_op(reshadefx::blend_op blendOp);
        static VkPrimitiveTopology convert_primitive_topology(reshadefx::primitive_topology topology);
        static VkCompareOp         convert_stencil_func(reshadefx::stencil_func stencilFunc);
        static VkStencilOp         convert_stencil_op(reshadefx::stencil_op stencilOp);
        static Uniform::BaseType   convert_uniform_type(reshadefx::type type);
        static Uniform::UiType     convert_uniform_ui_type(const std::string& type);

        static uint32_t convert_color_space(VkColorSpaceKHR colorSpace);
        static uint32_t format_bit_depth(VkFormat format);
    };
} // namespace vkShade
