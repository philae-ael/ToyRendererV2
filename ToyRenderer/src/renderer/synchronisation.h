#pragma once

#include <vulkan/vulkan_core.h>

namespace tr::renderer {
struct ImageMemoryBarrier {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags srcStageMask;
  VkPipelineStageFlags dstStageMask;

  void submit(VkCommandBuffer cmd) const {
    vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }
};

struct DstImageMemoryBarrier {
  VkAccessFlags srcAccessMask;
  VkAccessFlags dstAccessMask;
  VkImageLayout newLayout;
  uint32_t dstQueueFamilyIndex;
  VkPipelineStageFlags dstStageMask;
};

struct SrcImageMemoryBarrier {
  VkAccessFlags srcAccessMask;
  VkImageLayout oldLayout;
  uint32_t srcQueueFamilyIndex;
  VkPipelineStageFlags srcStageMask;

  auto merge(DstImageMemoryBarrier dst, VkImage image, VkImageSubresourceRange subressourceRange) const
      -> ImageMemoryBarrier {
    return {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = srcAccessMask | dst.srcAccessMask,
            .dstAccessMask = dst.dstAccessMask,
            .oldLayout = oldLayout,
            .newLayout = dst.newLayout,
            .srcQueueFamilyIndex = srcQueueFamilyIndex,
            .dstQueueFamilyIndex = dst.dstQueueFamilyIndex,
            .image = image,
            .subresourceRange = subressourceRange,
        },
        srcStageMask,
        dst.dstStageMask,
    };
  }
};

static const SrcImageMemoryBarrier SrcImageMemoryBarrierUndefined{
    .srcAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .srcQueueFamilyIndex = 0,
    .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
};

static const DstImageMemoryBarrier DstImageMemoryBarrierColorAttachment{
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .dstQueueFamilyIndex = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
};

static const DstImageMemoryBarrier DstImageMemoryBarrierDepthAttachment{
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .dstQueueFamilyIndex = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
};

static const DstImageMemoryBarrier DstImageMemoryBarrierPresent{
    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .dstAccessMask = 0,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .dstQueueFamilyIndex = 0,
    .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
};

}  // namespace tr::renderer
