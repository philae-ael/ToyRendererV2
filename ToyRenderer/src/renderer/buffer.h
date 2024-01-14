#pragma once

#include <vulkan/vulkan_core.h>

#include "utils.h"
namespace tr::renderer {

struct OneTimeCommandBuffer {
  VkCommandBuffer vk_cmd;
  bool primary;

  [[nodiscard]] auto begin() const -> VkResult {
    VkCommandBufferBeginInfo cmd_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    if (primary) {
      return vkBeginCommandBuffer(vk_cmd, &cmd_begin_info);
    }

    const VkCommandBufferInheritanceInfo inheritance_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = nullptr,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .framebuffer = VK_NULL_HANDLE,
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = 0,
        .pipelineStatistics = 0,
    };
    cmd_begin_info.pInheritanceInfo = &inheritance_info;

    return vkBeginCommandBuffer(vk_cmd, &cmd_begin_info);
  }

  [[nodiscard]] auto end() const -> VkResult { return vkEndCommandBuffer(vk_cmd); }

  static auto allocate(VkDevice device, VkCommandPool command_pool, bool primary = true) -> OneTimeCommandBuffer {
    OneTimeCommandBuffer cmd{
        VK_NULL_HANDLE,
        primary,
    };

    VkCommandBufferAllocateInfo transfer_cmd_alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = command_pool,
        .level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        .commandBufferCount = 1,
    };

    VK_UNWRAP(vkAllocateCommandBuffers, device, &transfer_cmd_alloc_info, &cmd.vk_cmd);
    return cmd;
  }
};
}  // namespace tr::renderer
