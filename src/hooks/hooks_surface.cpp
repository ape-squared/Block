#include "hooks.hpp"
#include "hooks_surface.hpp"

#include "core/logger.hpp"

#include "core/service_locator.hpp"
#include "input/input_manager.hpp"

#if defined(_WIN32)
#include "input/input_backend_win32.hpp"
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include "input/input_backend_wayland.hpp"
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
#include "input/input_backend_xcb.hpp"
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include "input/input_backend_xlib.hpp"
#endif

#if defined(_WIN32)
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateWin32SurfaceKHR(VkInstance                          instance,
                                                                  const VkWin32SurfaceCreateInfoKHR* pCreateInfo,
                                                                  const VkAllocationCallbacks*       pAllocator,
                                                                  VkSurfaceKHR*                      pSurface)
{
    vkShade::Logger::trace("Intercepted VkCreateWin32SurfaceKHR");

    auto& thisInstance = get_instance_from_handle(instance);

    PFN_vkCreateWin32SurfaceKHR createWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)
        thisInstance.dispatch.GetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

    if (!createWin32SurfaceKHR)
    {
        vkShade::Logger::error("vkCreateWin32SurfaceKHR not available");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkResult result = createWin32SurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
    if (result == VK_SUCCESS)
    {
        vkShade::Locator<vkShade::InputManager>::reset();
        vkShade::Locator<vkShade::InputManager>::emplace<vkShade::InputBackendWin32>(pCreateInfo->hwnd);
        vkShade::Logger::debug("Win32 surface created");
    }

    return result;
}
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateWaylandSurfaceKHR(VkInstance                           instance,
                                                                    const VkWaylandSurfaceCreateInfoKHR* pCreateInfo,
                                                                    const VkAllocationCallbacks*         pAllocator,
                                                                    VkSurfaceKHR*                        pSurface)
{
    vkShade::Logger::trace("Intercepted VkCreateWaylandSurfaceKHR");

    auto& thisInstance = get_instance_from_handle(instance);

    PFN_vkCreateWaylandSurfaceKHR createWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)
        thisInstance.dispatch.GetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");

    VkResult result = createWaylandSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
    if (result == VK_SUCCESS)
    {
        if (!vkShade::Locator<vkShade::InputManager>::has())
            vkShade::Locator<vkShade::InputManager>::emplace<vkShade::InputBackendWayland>(
                pCreateInfo->display);
        vkShade::Logger::debug("Wayland surface created");
    }

    return result;
}
#endif

#if defined(VK_USE_PLATFORM_XCB_KHR)
VK_LAYER_EXPORT VkResult vkShade_CreateXcbSurfaceKHR(VkInstance                       instance,
                                                     const VkXcbSurfaceCreateInfoKHR* pCreateInfo,
                                                     const VkAllocationCallbacks*     pAllocator,
                                                     VkSurfaceKHR*                    pSurface)
{
    vkShade::Logger::trace("Intercepted VkCreateXcbSurfaceKHR");

    auto* xcbCreateInfo = reinterpret_cast<const VkXcbSurfaceCreateInfoKHR*>(pCreateInfo);
    xcb_connection_t* connection = xcbCreateInfo->connection;
    xcb_window_t window = xcbCreateInfo->window;

    auto& thisInstance = get_instance_from_handle(instance);

    PFN_vkCreateXcbSurfaceKHR createXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)
        thisInstance.dispatch.GetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR");

    VkResult result = createXcbSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
    if (result == VK_SUCCESS)
    {
        vkShade::Locator<vkShade::InputManager>::reset();
        vkShade::Locator<vkShade::InputManager>::emplace<vkShade::InputBackendXcb>(connection, window);
        vkShade::Logger::debug("X11 (XCB) surface created");
    }

    return result;
}
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateXlibSurfaceKHR(VkInstance                        instance,
                                                                 const VkXlibSurfaceCreateInfoKHR* pCreateInfo,
                                                                 const VkAllocationCallbacks*      pAllocator,
                                                                 VkSurfaceKHR*                     pSurface)
{
    vkShade::Logger::trace("Intercepted VkCreateXlibSurfaceKHR");

    auto* xlibCreateInfo = reinterpret_cast<const VkXlibSurfaceCreateInfoKHR*>(pCreateInfo);
    Window window = xlibCreateInfo->window;
    Display* display = xlibCreateInfo->dpy;

    auto& thisInstance = get_instance_from_handle(instance);

    PFN_vkCreateXlibSurfaceKHR createXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)
        thisInstance.dispatch.GetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");

    VkResult result = createXlibSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
    if (result == VK_SUCCESS)
    {
        vkShade::Locator<vkShade::InputManager>::reset();
        vkShade::Locator<vkShade::InputManager>::emplace<vkShade::InputBackendXlib>(display, window);
        vkShade::Logger::debug("X11 (Xlib) surface created");
    }

    return result;
}
#endif
