#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
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
  VmaAllocation alloc = nullptr;
  VmaAllocationInfo alloc_info{};

  void defer_deletion(VmaDeletionStack& allocator_deletion_stack) const {
    allocator_deletion_stack.defer_deletion(VmaHandle::Buffer, vk_buffer, alloc);
  }
};

struct StagingBuffer {
  static auto init(VmaAllocator& allocator, std::uint32_t size = 65536) -> StagingBuffer;
  std::size_t size = 0;
  uint32_t offset = 0;
  uint32_t to_upload = 0;
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;
  VmaAllocationInfo alloc_info{};

  void defer_deletion(VmaDeletionStack& vma_deletion_stack) const {
    vma_deletion_stack.defer_deletion(VmaHandle::Buffer, buffer, alloc);
  }

  auto with_data(std::span<const std::byte>) -> StagingBuffer&;
  auto upload(VkCommandBuffer, VkBuffer) -> StagingBuffer&;
  void reset();
};

auto TriangleVertexBuffer(Device& device, VmaAllocator allocator, VkCommandBuffer cmd, StagingBuffer& staging_buffer)
    -> tr::renderer::Buffer;
}  // namespace tr::renderer
