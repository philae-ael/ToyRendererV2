
#include "frame.h"

#include <vulkan/vulkan_core.h>

#include "vulkan_engine.h"

auto tr::renderer::FrameSynchro::init(VkDevice device) -> FrameSynchro {
  FrameSynchro synchro{};
  VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  VK_UNWRAP(vkCreateFence, device, &fence_create_info, nullptr, &synchro.render_fence);

  VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VK_UNWRAP(vkCreateSemaphore, device, &semaphore_create_info, nullptr, &synchro.render_semaphore);
  VK_UNWRAP(vkCreateSemaphore, device, &semaphore_create_info, nullptr, &synchro.present_semaphore);

  set_debug_object_name(device, VK_OBJECT_TYPE_SEMAPHORE, synchro.present_semaphore, " render_semaphore");
  set_debug_object_name(device, VK_OBJECT_TYPE_SEMAPHORE, synchro.present_semaphore, " present_semaphore");
  return synchro;
}
void tr::renderer::Frame::write_cpu_timestamp(CPUTimestampIndex index) const {
  ctx->debug_info.write_cpu_timestamp(index);
}

void tr::renderer::Frame::write_gpu_timestamp(VkPipelineStageFlagBits pipelineStage, GPUTimestampIndex index) const {
  ctx->debug_info.write_gpu_timestamp(cmd.vk_cmd, pipelineStage, index);
}
auto tr::renderer::Frame::allocate_descriptor(VkDescriptorSetLayout layout) -> VkDescriptorSet {
  return descriptor_allocator.allocate(ctx->device.vk_device, layout);
};
