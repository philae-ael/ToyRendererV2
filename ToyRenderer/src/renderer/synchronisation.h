#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace tr::renderer {
struct ImageMemoryBarrier {
  VkImageMemoryBarrier2 barrier;

  void submit(VkCommandBuffer cmd) const {
    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dependency_info);
  }
};

struct SyncInfo {
  VkAccessFlags2 accessMask;
  VkPipelineStageFlags2 stageMask;
  VkImageLayout layout;
  uint32_t queueFamilyIndex;

  auto prepare_sync_transition(const SyncInfo& dst, VkImage image, VkImageSubresourceRange subressourceRange) const
      -> ImageMemoryBarrier {
    return {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = stageMask,
            .srcAccessMask = accessMask,
            .dstStageMask = dst.stageMask,
            .dstAccessMask = dst.accessMask,
            .oldLayout = layout,
            .newLayout = dst.layout,
            .srcQueueFamilyIndex = queueFamilyIndex,
            .dstQueueFamilyIndex = dst.queueFamilyIndex,
            .image = image,
            .subresourceRange = subressourceRange,
        },
    };
  }

  constexpr auto copy() -> SyncInfo { return *this; }
  constexpr auto queue(uint32_t queue_family_index) -> SyncInfo& {
    queueFamilyIndex = queue_family_index;
    return *this;
  }
};

static constexpr SyncInfo SrcImageMemoryBarrierUndefined{
    .accessMask = 0,
    .stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
    .queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

static constexpr SyncInfo SyncColorAttachment{
    .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

static constexpr SyncInfo SyncLateDepth{
    .accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    .stageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    .layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

static constexpr SyncInfo SyncPresent{
    .accessMask = 0,
    .stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

}  // namespace tr::renderer
