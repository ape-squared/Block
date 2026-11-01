#pragma once

#include <assert.h>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include <vk_mem_alloc.h>
#include <vulkan/utility/vk_dispatch_table.h>
#include <vulkan/vulkan_core.h>

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
