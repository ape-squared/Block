#include "hooks/hooks.hpp"
#include "hooks/hooks_surface.hpp"

#include <string.h>
#include <vulkan/vulkan_core.h>

std::unordered_map<void*, VulkanInstance> g_vulkanInstances;
std::unordered_map<void*, VulkanDevice>   g_vulkanDevices;

std::shared_mutex g_globalLock;

#define GETPROCADDR(func) if(!strcmp(pName, "vk" #func)) return (PFN_vkVoidFunction)&vkShade_##func;

extern "C" VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkShade_GetDeviceProcAddr(VkDevice device, const char* pName)
{
    GETPROCADDR(GetDeviceProcAddr);

    GETPROCADDR(DestroyDevice);

    GETPROCADDR(CreateSwapchainKHR);
    GETPROCADDR(DestroySwapchainKHR);
    GETPROCADDR(QueuePresentKHR);

    GETPROCADDR(AllocateCommandBuffers);

    {
        std::shared_lock lock(g_globalLock);
        auto& thisDevice = get_device_from_handle(device);
        return reinterpret_cast<PFN_vkVoidFunction>(thisDevice.dispatch.GetDeviceProcAddr(device, pName));
    }
}

extern "C" VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkShade_GetInstanceProcAddr(VkInstance instance, const char* pName)
{
    GETPROCADDR(GetInstanceProcAddr);

    GETPROCADDR(CreateInstance);
    GETPROCADDR(DestroyInstance);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(DestroyDevice);

#if defined(_WIN32)
    GETPROCADDR(CreateWin32SurfaceKHR);
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    GETPROCADDR(CreateWaylandSurfaceKHR);
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
    GETPROCADDR(CreateXcbSurfaceKHR);
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    GETPROCADDR(CreateXlibSurfaceKHR);
#endif

    {
        std::shared_lock lock(g_globalLock);
        auto& thisInstance = get_instance_from_handle(instance);
        return reinterpret_cast<PFN_vkVoidFunction>(thisInstance.dispatch.GetInstanceProcAddr(instance, pName));
    }
}
