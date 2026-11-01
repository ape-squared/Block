#include "hooks.hpp"

#include <shared_mutex>
#include <vulkan/vulkan_core.h>

#include "core/logger.hpp"
#include "runtime/depth_tracker.hpp"

// Hooks feeding the per-device DepthTracker. Every hook forwards to the next layer
// unconditionally, the tracker only observes (and, for CreateImage, adds usage flags).

namespace
{
    struct DeviceRef
    {
        VulkanDevice*         device;
        vkShade::DepthTracker* tracker;
    };

    // Resolve the device behind any dispatchable handle. The lock is only held for the lookup:
    // entries are stable until DestroyDevice, and the application can't call into a device
    // that is being destroyed. Holding it any longer would deadlock when ImGui tears down its
    // resources through these hooks while DestroyDevice owns the lock.
    DeviceRef find_device(const void* handle)
    {
        std::shared_lock lock(g_globalLock);
        VulkanDevice& device = get_device_from_handle(handle);
        return { &device, device.depthTracker.get() };
    }
}  // namespace

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateImage(VkDevice                     device,
                                                        const VkImageCreateInfo*     pCreateInfo,
                                                        const VkAllocationCallbacks* pAllocator,
                                                        VkImage*                     pImage)
{
    auto [thisDevice, tracker] = find_device(device);

    // Depth images gain TRANSFER_SRC (and SAMPLED) usage so they can be copied
    VkImageCreateInfo modifiedCreateInfo;
    const VkImageCreateInfo* createInfo = pCreateInfo;
    if (tracker && tracker->adjust_image_create_info(*pCreateInfo, modifiedCreateInfo))
        createInfo = &modifiedCreateInfo;

    VkResult result = thisDevice->dispatch.CreateImage(device, createInfo, pAllocator, pImage);

    // Never let our usage changes make an image creation fail that would have succeeded
    if (result != VK_SUCCESS && createInfo != pCreateInfo)
    {
        vkShade::Logger::warn("Depth image creation failed with extra usages; retrying unmodified");
        createInfo = pCreateInfo;
        result = thisDevice->dispatch.CreateImage(device, createInfo, pAllocator, pImage);
    }

    if (result == VK_SUCCESS && tracker)
        tracker->on_image_created(*pImage, *createInfo);

    return result;
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_DestroyImage(VkDevice                     device,
                                                     VkImage                      image,
                                                     const VkAllocationCallbacks* pAllocator)
{
    auto [thisDevice, tracker] = find_device(device);

    if (tracker)
        tracker->on_image_destroyed(image);

    thisDevice->dispatch.DestroyImage(device, image, pAllocator);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_CreateImageView(VkDevice                      device,
                                                            const VkImageViewCreateInfo*  pCreateInfo,
                                                            const VkAllocationCallbacks*  pAllocator,
                                                            VkImageView*                  pView)
{
    auto [thisDevice, tracker] = find_device(device);

    VkResult result = thisDevice->dispatch.CreateImageView(device, pCreateInfo, pAllocator, pView);

    if (result == VK_SUCCESS && tracker)
        tracker->on_image_view_created(*pCreateInfo, *pView);

    return result;
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_DestroyImageView(VkDevice                     device,
                                                         VkImageView                  imageView,
                                                         const VkAllocationCallbacks* pAllocator)
{
    auto [thisDevice, tracker] = find_device(device);

    if (tracker)
        tracker->on_image_view_destroyed(imageView);

    thisDevice->dispatch.DestroyImageView(device, imageView, pAllocator);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdBeginRendering(VkCommandBuffer          commandBuffer,
                                                          const VkRenderingInfo*   pRenderingInfo)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    // May record a copy of the depth buffer right before it gets cleared
    if (tracker)
        tracker->on_begin_rendering(commandBuffer, pRenderingInfo);

    // vkCmdBeginRenderingKHR is the same command; devices exposing only the extension have no core entry
    auto next = thisDevice->dispatch.CmdBeginRendering ? thisDevice->dispatch.CmdBeginRendering
                                                       : thisDevice->dispatch.CmdBeginRenderingKHR;
    next(commandBuffer, pRenderingInfo);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdEndRendering(VkCommandBuffer commandBuffer)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    auto next = thisDevice->dispatch.CmdEndRendering ? thisDevice->dispatch.CmdEndRendering
                                                     : thisDevice->dispatch.CmdEndRenderingKHR;
    next(commandBuffer);

    // Records the depth copy after the render scope has ended
    if (tracker)
        tracker->on_end_rendering(commandBuffer);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDraw(VkCommandBuffer commandBuffer,
                                                uint32_t        vertexCount,
                                                uint32_t        instanceCount,
                                                uint32_t        firstVertex,
                                                uint32_t        firstInstance)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    if (tracker)
        tracker->on_draw(commandBuffer, 1, static_cast<uint64_t>(vertexCount) * instanceCount);

    thisDevice->dispatch.CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDrawIndexed(VkCommandBuffer commandBuffer,
                                                       uint32_t        indexCount,
                                                       uint32_t        instanceCount,
                                                       uint32_t        firstIndex,
                                                       int32_t         vertexOffset,
                                                       uint32_t        firstInstance)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    if (tracker)
        tracker->on_draw(commandBuffer, 1, static_cast<uint64_t>(indexCount) * instanceCount);

    thisDevice->dispatch.CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDrawIndirect(VkCommandBuffer commandBuffer,
                                                        VkBuffer        buffer,
                                                        VkDeviceSize    offset,
                                                        uint32_t        drawCount,
                                                        uint32_t        stride)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    // Vertex counts live in the GPU buffer, so only the draws are counted
    if (tracker)
        tracker->on_draw(commandBuffer, drawCount, 0);

    thisDevice->dispatch.CmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdDrawIndexedIndirect(VkCommandBuffer commandBuffer,
                                                               VkBuffer        buffer,
                                                               VkDeviceSize    offset,
                                                               uint32_t        drawCount,
                                                               uint32_t        stride)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    if (tracker)
        tracker->on_draw(commandBuffer, drawCount, 0);

    thisDevice->dispatch.CmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdClearDepthStencilImage(VkCommandBuffer                 commandBuffer,
                                                                  VkImage                         image,
                                                                  VkImageLayout                   imageLayout,
                                                                  const VkClearDepthStencilValue* pDepthStencil,
                                                                  uint32_t                        rangeCount,
                                                                  const VkImageSubresourceRange*  pRanges)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    // Save the depth before it is wiped
    if (tracker)
        tracker->on_clear_depth_stencil_image(commandBuffer, image, imageLayout);

    thisDevice->dispatch.CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

VK_LAYER_EXPORT void VKAPI_CALL vkShade_CmdClearAttachments(VkCommandBuffer       commandBuffer,
                                                            uint32_t              attachmentCount,
                                                            const VkClearAttachment* pAttachments,
                                                            uint32_t              rectCount,
                                                            const VkClearRect*    pRects)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    if (tracker)
        tracker->on_clear_attachments(commandBuffer, attachmentCount, pAttachments);

    thisDevice->dispatch.CmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_BeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                               const VkCommandBufferBeginInfo* pBeginInfo)
{
    auto [thisDevice, tracker] = find_device(commandBuffer);

    const VkResult result = thisDevice->dispatch.BeginCommandBuffer(commandBuffer, pBeginInfo);

    // The reset succeeded, so any copy previously recorded in this buffer is gone.
    if (result == VK_SUCCESS && tracker)
        tracker->on_begin_command_buffer(commandBuffer);

    return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_QueueSubmit(VkQueue queue,
                                                        uint32_t submitCount,
                                                        const VkSubmitInfo* pSubmits,
                                                        VkFence fence)
{
    auto [thisDevice, tracker] = find_device(queue);

    const VkResult result = thisDevice->dispatch.QueueSubmit(queue, submitCount, pSubmits, fence);

    if (result == VK_SUCCESS && tracker)
        tracker->on_queue_submit(submitCount, pSubmits);

    return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkShade_QueueSubmit2(VkQueue queue,
                                                         uint32_t submitCount,
                                                         const VkSubmitInfo2* pSubmits,
                                                         VkFence fence)
{
    auto [thisDevice, tracker] = find_device(queue);

    auto next = thisDevice->dispatch.QueueSubmit2 ? thisDevice->dispatch.QueueSubmit2
                                                  : thisDevice->dispatch.QueueSubmit2KHR;
    if (!next)
        return VK_ERROR_FEATURE_NOT_PRESENT;

    const VkResult result = next(queue, submitCount, pSubmits, fence);

    if (result == VK_SUCCESS && tracker)
        tracker->on_queue_submit2(submitCount, pSubmits);

    return result;
}
