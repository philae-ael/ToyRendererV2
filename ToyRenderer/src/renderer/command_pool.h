#pragma once

#include <vulkan/vulkan_core.h>

#include "deletion_queue.h"
#include "device.h"

namespace tr::renderer {

struct CommandPool {
  enum class TargetQueue { Graphics, Present };

  static auto init(Device& device, CommandPool::TargetQueue target_queue)
      -> VkCommandPool;
  static auto defer_deletion(VkCommandPool commandPool,
                             DeviceDeletionStack& device_deletion_stack) {
    device_deletion_stack.defer_deletion(DeviceHandle::CommandPool,
                                         commandPool);
  }
};
}  // namespace tr::renderer
