#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace tr::renderer {

template <const std::size_t N>
struct AttributeBuilder {
  std::array<VkVertexInputAttributeDescription, N> attributes{};

  uint32_t built = 0;
  uint32_t current_binding = 0;

  consteval AttributeBuilder() = default;

  consteval auto binding(uint32_t b) -> AttributeBuilder {
    current_binding = b;

    return *this;
  }

  consteval auto attribute(uint32_t location, std::size_t offset, VkFormat format) -> AttributeBuilder {
    attributes[built] = {
        .location = location,
        .binding = current_binding,
        .format = format,
        .offset = static_cast<uint32_t>(offset),
    };
    built += 1;
    return *this;
  }

  consteval auto build() -> std::array<VkVertexInputAttributeDescription, N> { return attributes; };
};
}  // namespace tr::renderer
