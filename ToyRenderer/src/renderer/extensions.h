#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

enum ExtensionFlags : uint64_t {
  EXTENSION_FLAG_DEFAULT = 0,
  EXTENSION_FLAG_DEBUG_UTILS = 1,
};

void load_extensions(VkInstance instance, ExtensionFlags flags);
