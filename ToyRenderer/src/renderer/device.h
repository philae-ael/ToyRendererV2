#pragma once

#include <vulkan/vulkan_core.h>

#include "deletion_queue.h"

struct GLFWwindow;

namespace tr::renderer {

struct Device {
  static auto init(VkInstance instance, VkSurfaceKHR surface) -> Device;
  void defer_deletion(InstanceDeletionStack &instance_deletion_stack) const {
    instance_deletion_stack.defer_deletion(InstanceHandle::Device, vk_device);
  }

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice vk_device = VK_NULL_HANDLE;

  VkPhysicalDeviceProperties device_properties = {};

  struct Queues {
    std::uint32_t graphics_family;
    std::uint32_t present_family;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkQueueFamilyProperties graphics_family_properties;
    VkQueueFamilyProperties present_family_properties;
  } queues{};
};
}  // namespace tr::renderer
