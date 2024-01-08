#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <span>

#include "deletion_queue.h"
#include "ressources.h"

namespace tr::renderer {
struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv1;
  glm::vec2 uv2;
  static const std::array<VkVertexInputAttributeDescription, 5> attributes;
  static const std::array<VkVertexInputBindingDescription, 1> bindings;
};

struct StagingBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;
  VmaAllocationInfo alloc_info{};

  uint32_t offset = 0;
  uint32_t to_upload = 0;
  bool used = false;

  static auto init(VmaAllocator allocator, std::uint32_t size = 65536) -> StagingBuffer;
  void defer_deletion(VmaDeletionStack& vma_deletion_stack) const {
    vma_deletion_stack.defer_deletion(VmaHandle::Buffer, buffer, alloc);
  }

  auto consume(std::size_t size) -> std::span<std::byte>;
  auto commit(VkCommandBuffer, VkBuffer, uint32_t offset) -> StagingBuffer&;
  auto commit_image(VkCommandBuffer cmd, const ImageRessource& image, VkRect2D r) -> StagingBuffer&;
  void reset();
};

}  // namespace tr::renderer
