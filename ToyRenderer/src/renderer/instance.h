#pragma once

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <set>
#include <span>
#include <string>

#include "deletion_stack.h"

namespace tr {
struct Options;
}  // namespace tr

namespace tr::renderer {

struct Instance {
  static auto init(const tr::Options &options, std::span<const char *> required_wsi_extensions) -> Instance;
  void defer_deletion(InstanceDeletionStack &instance_deletion_queue) const {
    instance_deletion_queue.defer_deletion(InstanceHandle::DebugUtilsMessengerEXT, vk_debug_utils_messenger_ext);
  }
  VkInstance vk_instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT vk_debug_utils_messenger_ext = VK_NULL_HANDLE;

  std::set<std::string> extensions;
  std::set<std::string> validation_layers;
};
}  // namespace tr::renderer
