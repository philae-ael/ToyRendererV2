#pragma once

#include <vulkan/vulkan_core.h>

namespace tr {
namespace renderer {
struct Device;
struct Lifetime;
struct PhysicalDevice;
}  // namespace renderer
}  // namespace tr

namespace tr::renderer {

struct CommandPool {
  enum class TargetQueue { Graphics, Present, Transfer };

  static auto init(Lifetime& lifetime, Device& device, PhysicalDevice& physical_device,
                   CommandPool::TargetQueue target_queue) -> VkCommandPool;
};
}  // namespace tr::renderer
