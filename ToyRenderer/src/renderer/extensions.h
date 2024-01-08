#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

enum ExtensionFlags : uint64_t {
  DEFAULT = 0,
  DEBUG_UTILS = 1,
};

void load_extensions(VkInstance instance, ExtensionFlags flags);
