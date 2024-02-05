#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <optional>
#include <set>
#include <span>
#include <string>

#include "utils/assert.h"

#define VK_UNWRAP(f, ...) VK_CHECK(f(__VA_ARGS__), f)
#define VK_CHECK(result, name)                                                                               \
  do {                                                                                                       \
    VkResult __res = (result);                                                                               \
    TR_ASSERT(__res == VK_SUCCESS, "error while calling " #name " got error code {}", (std::uint32_t)__res); \
  } while (false)

static_assert(0 == static_cast<std::uint32_t>(0), "to remove warning about unused header cstdint, NOLINT doesnt works");

namespace tr::renderer {

struct VkBaseInOutStructure {
  VkStructureType sType;
  const VkBaseInStructure* pNext;
};

template <class Child, class T>
struct VkBuilder : T {
  constexpr explicit VkBuilder(T&& t) : T(std::move(t)) {}
  [[nodiscard]] constexpr auto inner() const -> const T& { return *this; }
  [[nodiscard]] constexpr auto inner_ptr() const -> const T* { return this; }
  constexpr auto inner() -> T& { return *this; }
  constexpr auto inner_ptr() -> T* { return this; }

  constexpr auto copy() -> Child { return *this; }

  template <class Next>
  constexpr auto chain(Next* next) -> Child& {
    next->pNext = inner().pNext;
    inner().pNext = next;

    return static_cast<Child&>(*this);
  }
};

auto check_extensions(const char* kind, std::span<const char* const> required, std::span<const char* const> optional,
                      std::span<const VkExtensionProperties> available) -> std::optional<std::set<std::string>>;

}  // namespace tr::renderer
