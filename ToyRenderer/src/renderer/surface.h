#pragma once

#include <vulkan/vulkan_core.h>

#include "deletion_queue.h"

struct GLFWwindow;
namespace tr::renderer {
struct Surface {
  static auto init(VkInstance instance, GLFWwindow *window) -> VkSurfaceKHR;
  static void defer_deletion(VkSurfaceKHR surface, InstanceDeletionStack &instance_deletion_queue)  {
    instance_deletion_queue.defer_deletion(InstanceHandle::SurfaceKHR, surface);
  }
};
}  // namespace tr::renderer
