#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include "deletion_queue.h"
#include "utils/assert.h"
#include "utils/types.h"
#include "utils/cast.h"

struct GLFWwindow;

namespace tr::renderer {

struct Device : utils::types::threadsafe {
  static auto init(VkInstance instance, VkSurfaceKHR surface) -> Device;
  void defer_deletion(InstanceDeletionStack& instance_deletion_stack) const {
    instance_deletion_stack.defer_deletion(InstanceHandle::Device, vk_device);
  }

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice vk_device = VK_NULL_HANDLE;

  VkPhysicalDeviceProperties device_properties{};
  VkPhysicalDeviceMemoryProperties memory_properties{};

  struct Queues {
    std::uint32_t graphics_family;
    std::uint32_t present_family;
    std::uint32_t transfer_family;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkQueueFamilyProperties graphics_family_properties;
    VkQueueFamilyProperties present_family_properties;
    VkQueueFamilyProperties transfer_family_properties;
  } queues{};

  auto find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) -> uint32_t {
    for (std::size_t i = 0; i < memory_properties.memoryTypeCount; i++) {
      const auto& memory_type = memory_properties.memoryTypes[i];

      bool suitable_memory = ((type_filter & (1 << i)) != 0) && (memory_type.propertyFlags & properties) == properties;
      if (suitable_memory) {
        return utils::narrow_cast<uint32_t>(i);
      }
    }

    TR_ASSERT(false, "could not find any memory");
  }
};
}  // namespace tr::renderer
