#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
namespace tr::renderer {
struct ImageMemoryBarrier {
  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_NONE,
      .dstAccessMask = VK_ACCESS_NONE,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = VK_NULL_HANDLE,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_NONE,
              .baseMipLevel = 0,
              .levelCount = 0,
              .baseArrayLayer = 0,
              .layerCount = 0,
          },
  };
  constexpr auto src_access_mask(VkAccessFlags src_access_mask) -> ImageMemoryBarrier& {
    barrier.srcAccessMask = src_access_mask;
    return *this;
  }
  constexpr auto layouts(VkImageLayout old_layout, VkImageLayout new_layout) -> ImageMemoryBarrier& {
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    return *this;
  }

  constexpr auto subresource_range(VkImageSubresourceRange subresource_range) -> ImageMemoryBarrier& {
    barrier.subresourceRange = subresource_range;
    return *this;
  }
  constexpr auto queues(uint32_t src_queue_family, uint32_t dst_queue_family) -> ImageMemoryBarrier& {
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dst_queue_family;
    return *this;
  }
  constexpr auto image(VkImage image) -> ImageMemoryBarrier& {
    barrier.image = image;
    return *this;
  }
  [[nodiscard]] constexpr auto copy() const -> ImageMemoryBarrier { return *this; }
  void submit(VkCommandBuffer cmd, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask) const {
    vkCmdPipelineBarrier(cmd, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }
};

static constexpr ImageMemoryBarrier ColorAttachmentToPresentImageBarrier =
    ImageMemoryBarrier{}
        .src_access_mask(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .layouts(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        .subresource_range({
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        });

}  // namespace tr::renderer
