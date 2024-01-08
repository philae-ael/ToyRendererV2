#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <span>
namespace tr::renderer {

struct QueueSubmit {
  VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = nullptr,
      .pWaitDstStageMask = nullptr,
      .commandBufferCount = 0,
      .pCommandBuffers = nullptr,
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = nullptr,
  };

  template <const std::size_t N>
  auto wait_semaphores(std::span<const VkSemaphore, N> semaphores,
                       std::span<const VkPipelineStageFlags, N> wait_dst_stage_mask) -> QueueSubmit& {
    submit_info.waitSemaphoreCount = N;
    submit_info.pWaitSemaphores = semaphores.data();
    submit_info.pWaitDstStageMask = wait_dst_stage_mask.data();
    return *this;
  }

  auto signal_semaphores(std::span<const VkSemaphore> semaphores) -> QueueSubmit& {
    submit_info.signalSemaphoreCount = semaphores.size();
    submit_info.pSignalSemaphores = semaphores.data();
    return *this;
  }

  auto command_buffers(std::span<const VkCommandBuffer> buffers) -> QueueSubmit& {
    submit_info.pCommandBuffers = buffers.data();
    submit_info.commandBufferCount = buffers.size();
    return *this;
  }

  auto submit(VkQueue queue, VkFence fence) -> VkResult { return vkQueueSubmit(queue, 1, &submit_info, fence); }
};
}  // namespace tr::renderer
