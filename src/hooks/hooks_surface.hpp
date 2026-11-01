#pragma once

#include "hooks.hpp"
#include <vulkan/vulkan_core.h>

#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <vulkan/vulkan_wayland.h>
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateWaylandSurfaceKHR(VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
#endif

#if defined(VK_USE_PLATFORM_XCB_KHR)
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
VK_LAYER_EXPORT VkResult vkShade_CreateXcbSurfaceKHR(VkInstance instance, const VkXcbSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateXlibSurfaceKHR(VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
#endif
