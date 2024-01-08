#pragma once

#include <vulkan/vulkan_core.h>

#include <span>

#define VK_UNWRAP(f, ...)                                              \
  do {                                                                 \
    VkResult result = f(__VA_ARGS__);                                  \
    TR_ASSERT(result == VK_SUCCESS,                                    \
              "error while calling " #f " got error code {}", result); \
  } while (false)

namespace tr::renderer {

auto check_extensions(const char* kind, std::span<const char* const> required,
                     std::span<VkExtensionProperties> available_extensions)
    -> bool;

auto vkObjectTypeName(VkObjectType) -> const char*;

}  // namespace tr::renderer
