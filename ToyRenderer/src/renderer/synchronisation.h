#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "utils/cast.h"
#include "utils/data/static_stack.h"

namespace tr::renderer {
struct ImageMemoryBarrier {
  static void submit(VkCommandBuffer cmd, std::span<VkImageMemoryBarrier2> barriers) {
    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = utils::narrow_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
    };
    vkCmdPipelineBarrier2(cmd, &dependency_info);
  }

  template <const size_t N>
    requires(N != std::dynamic_extent)
  static void submit(VkCommandBuffer cmd, std::span<const std::optional<VkImageMemoryBarrier2>, N> barriers) {
    utils::data::static_stack<VkImageMemoryBarrier2, N> barriers_;
    for (const auto& barrier : barriers) {
      if (barrier) {
        barriers_.push_back(*barrier);
      }
    }

    if (barriers_.size() > 0) {
      submit(cmd, barriers_);
    }
  }
};

struct SyncInfo {
  VkAccessFlags2 accessMask;
  VkPipelineStageFlags2 stageMask;
  VkImageLayout layout;
  uint32_t queueFamilyIndex;

  auto barrier(const SyncInfo& dst, VkImage image, VkImageSubresourceRange subressourceRange) const
      -> VkImageMemoryBarrier2 {
    return {
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

static constexpr SyncInfo SyncColorAttachmentOutput{
    .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

static constexpr SyncInfo SyncFragmentShaderReadOnly{
    .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    .queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

static constexpr SyncInfo SyncFragmentStorageRead{
    .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR,
    .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .layout = VK_IMAGE_LAYOUT_GENERAL,
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

static constexpr SyncInfo SyncImageTransfer{
    .accessMask = 0,
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

}  // namespace tr::renderer
