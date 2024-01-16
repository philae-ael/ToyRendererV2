#pragma once

#include <vulkan/vulkan_core.h>

#include "deletion_stack.h"
#include "device.h"

namespace tr::renderer {

struct CommandPool {
  enum class TargetQueue { Graphics, Present, Transfer };

  static auto init(Lifetime& lifetime, Device& device, CommandPool::TargetQueue target_queue) -> VkCommandPool;
};
}  // namespace tr::renderer
