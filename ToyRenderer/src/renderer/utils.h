#pragma once

#include <vulkan/vulkan_core.h>

#include <span>

#define VK_UNWRAP(f, ...) VK_CHECK(f(__VA_ARGS__), f)
#define VK_CHECK(result, name)                                                            \
  do {                                                                                    \
    VkResult res = (result);                                                              \
    TR_ASSERT(res == VK_SUCCESS, "error while calling " #name " got error code {}", res); \
  } while (false)

namespace tr::renderer {

auto check_extensions(const char* kind, std::span<const char* const> required,
                      std::span<VkExtensionProperties> available_extensions) -> bool;

auto vkObjectTypeName(VkObjectType) -> const char*;

}  // namespace tr::renderer
