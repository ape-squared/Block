#pragma once

#include <assert.h>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include <vk_mem_alloc.h>
#include <vulkan/utility/vk_dispatch_table.h>
#include <vulkan/vulkan_core.h>

#include "runtime/depth_tracker.hpp"

#undef VK_LAYER_EXPORT
#if defined(_WIN32)
#define VK_LAYER_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#else
#error "Unsupported platform!"
#endif

// Structs to store our instances/devices and their dispatch tables
struct VulkanInstance
{
    VkInstance handle;
    const uint32_t apiVersion;
    VkuInstanceDispatchTable dispatch;
};

struct VulkanDevice
{
    VkDevice handle;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties2 properties;
    VkInstance instance;
    VkuDeviceDispatchTable dispatch;
    VmaAllocator allocator;

    VkQueue       queue;
    uint32_t      queueFamilyIndex;
    VkCommandPool commandPool;

    // Depth buffer tracking. Created once the device is registered, destroyed in DestroyDevice.
    // (Makes VulkanDevice move-only.)
    std::unique_ptr<vkShade::DepthTracker> depthTracker;
};

inline void* const dispatch_key_from_handle(const void* handle)
{
    // Vulkan handles are pointers to dispatch tables - dereference to get the key
    assert(handle != nullptr);
    return *reinterpret_cast<void* const*>(handle);
}

namespace vkShade
{
    class Runtime;
}

// Layer book-keeping information
// These are only modified during create/destroy
extern std::unordered_map<void*, VulkanInstance> g_vulkanInstances;
extern std::unordered_map<void*, VulkanDevice>   g_vulkanDevices;

// Runtime book-keeping
extern std::unordered_map<VkSwapchainKHR, vkShade::Runtime> g_runtimes;

// Convenience functions for getting handles
inline VulkanInstance& get_instance_from_handle(const void* handle)
{
    auto it = g_vulkanInstances.find(dispatch_key_from_handle(handle));
    assert(it != g_vulkanInstances.end() && "Invalid instance key");
    return it->second;
}

inline VulkanDevice& get_device_from_handle(const void* handle)
{
    auto it = g_vulkanDevices.find(dispatch_key_from_handle(handle));
    assert(it != g_vulkanDevices.end() && "Invalid device key");
    return it->second;
}

// Reader/writer lock for concurrent read access to bookkeeping maps.
extern std::shared_mutex g_globalLock;

// Layer entrypoints — declarations must match VK_LAYER_EXPORT on definitions (MSVC).
extern "C" {
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkShade_GetDeviceProcAddr(VkDevice device, const char* pName);
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkShade_GetInstanceProcAddr(VkInstance instance, const char* pName);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);
VK_LAYER_EXPORT void     VKAPI_CALL vkShade_DestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator);

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
VK_LAYER_EXPORT void     VKAPI_CALL vkShade_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator);

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
VK_LAYER_EXPORT void     VKAPI_CALL vkShade_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator);
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_AllocateCommandBuffers(VkDevice                           device,
                                                                   const VkCommandBufferAllocateInfo* pAllocateInfo,
                                                                   VkCommandBuffer*                   pCommandBuffers);

// Depth buffer tracking (hooks_depth.cpp)
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage);
VK_LAYER_EXPORT void     VKAPI_CALL vkShade_DestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator);
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView);
VK_LAYER_EXPORT void     VKAPI_CALL vkShade_DestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator);

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo);
VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdEndRendering(VkCommandBuffer commandBuffer);

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects);

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_BeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo);
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_QueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_QueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence);
