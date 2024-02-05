#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "deletion_stack.h"

namespace tr::renderer {
struct QueuesInfo {
  std::uint32_t graphics_family;
  std::uint32_t present_family;
  std::uint32_t transfer_family;

  VkQueueFamilyProperties graphics_family_properties;
  VkQueueFamilyProperties present_family_properties;
  VkQueueFamilyProperties transfer_family_properties;
};

struct PhysicalDevice {
  VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
  std::set<std::string> extensions;

  VkPhysicalDeviceProperties device_properties{};
  VkPhysicalDeviceMemoryProperties memory_properties{};
  QueuesInfo queues;

  static auto init(VkInstance instance, VkSurfaceKHR surface) -> PhysicalDevice;
};

struct Device {
  static auto init(const PhysicalDevice& infos) -> Device;
  void defer_deletion(InstanceDeletionStack& instance_deletion_stack) const {
    instance_deletion_stack.defer_deletion(InstanceHandle::Device, vk_device);
  }

  VkDevice vk_device = VK_NULL_HANDLE;

  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue transfer_queue;

  std::vector<VkFormat> format_supported;
};
}  // namespace tr::renderer
