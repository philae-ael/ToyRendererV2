#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

#include "buffer.h"
#include "descriptors.h"
#include "device.h"
#include "queue.h"
#include "timeline_info.h"
#include "utils/types.h"

namespace tr {
namespace renderer {
class VulkanEngine;
struct FrameRessourceData;
struct Lifetime;
}  // namespace renderer
}  // namespace tr

namespace tr::renderer {

struct FrameSynchro {
  static auto init(Lifetime &lifetime, VkDevice device) -> FrameSynchro;

  VkFence render_fence;
  VkSemaphore render_semaphore;
  VkSemaphore present_semaphore;
};

struct Frame {
  std::uint32_t swapchain_image_index;
  FrameSynchro synchro;
  OneTimeCommandBuffer cmd;
  DescriptorAllocator descriptor_allocator;
  utils::types::not_null_pointer<tr::renderer::FrameRessourceData> frm;

  const VulkanEngine *ctx;

  auto submitCmds(VkQueue queue) const -> VkResult {
    return QueueSubmit{}
        .wait_semaphores<1>({{synchro.present_semaphore}}, {{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}})
        .signal_semaphores({{synchro.render_semaphore}})
        .command_buffers({{cmd.vk_cmd}})
        .submit(queue, synchro.render_fence);
  }

  auto present(Device &device, VkSwapchainKHR swapchain) const -> VkResult {
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &synchro.render_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &swapchain_image_index,
        .pResults = nullptr,
    };
    return vkQueuePresentKHR(device.present_queue, &present_info);
  }

  void write_gpu_timestamp(VkPipelineStageFlagBits pipelineStage, GPUTimestampIndex index) const;
  void write_cpu_timestamp(CPUTimestampIndex index) const;
  auto allocate_descriptor(VkDescriptorSetLayout layout) -> VkDescriptorSet;
};
}  // namespace tr::renderer
