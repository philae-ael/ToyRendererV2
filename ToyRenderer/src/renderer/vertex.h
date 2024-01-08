#pragma once

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "deletion_queue.h"
#include "device.h"

namespace tr::renderer {
struct Vertex {
  glm::vec3 pos;
  glm::vec3 base_color;
  glm::vec2 uv;
  static const std::array<VkVertexInputAttributeDescription, 3> attributes;
  static const std::array<VkVertexInputBindingDescription, 1> bindings;
};

struct Buffer {
  VkBuffer vk_buffer = VK_NULL_HANDLE;
  VkDeviceMemory device_memory = VK_NULL_HANDLE;

  void defer_deletion(DeviceDeletionStack& device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Buffer, vk_buffer);
  }
};

struct StagingBuffer {
  static auto init(Device& device, std::size_t size) -> StagingBuffer;
  std::size_t size = 0;
  Buffer buffer;

  void defer_deletion(DeviceDeletionStack& device_deletion_stack) const {
    buffer.defer_deletion(device_deletion_stack);
  }

  void stage(VkDevice, VkCommandBuffer, VkBuffer, std::span<const std::byte>);
};

auto TriangleVertexBuffer(tr::renderer::Device& device, VkCommandBuffer cmd, StagingBuffer& staging_buffer) -> Buffer;
}  // namespace tr::renderer
