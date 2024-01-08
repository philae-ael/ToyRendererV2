#pragma once

#include <vulkan/vulkan_core.h>

#include "deletion_queue.h"
#include "device.h"
namespace tr::renderer {

struct Pipeline {
  static auto init(Device& device) -> Pipeline;
  void defer_deletion(DeviceDeletionStack& device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::PipelineLayout, layout);
    device_deletion_stack.defer_deletion(DeviceHandle::Pipeline, vk_pipeline);
  }

  VkPipeline vk_pipeline = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
};

}  // namespace tr::renderer
