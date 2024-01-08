#pragma once

#include <vulkan/vulkan_core.h>

#include <span>

#include "../options.h"
#include "deletion_queue.h"


void load_extensions(VkInstance instance);

namespace tr::renderer {

struct Instance {
  static auto init(const tr::Options &options,
                   std::span<const char *> required_vulkan_instance_extensions)
      -> Instance;
  void defer_deletion(InstanceDeletionStack &instance_deletion_queue) const {
    instance_deletion_queue.defer_deletion(
        InstanceHandle::DebugUtilsMessengerEXT, vk_debug_utils_messenger_ext);
  }
  VkInstance vk_instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT vk_debug_utils_messenger_ext = VK_NULL_HANDLE;
};
}  // namespace tr::renderer
