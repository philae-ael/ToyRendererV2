
#include "vertex.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utils/cast.h>

#include "utils.h"
#include "utils/assert.h"

const std::array<VkVertexInputAttributeDescription, 4> tr::renderer::Vertex::attributes = {{
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, pos),
    },
    {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, normal),
    },
    {
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color),
    },
    {
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, uv),
    },
}};

const std::array<VkVertexInputBindingDescription, 1> tr::renderer::Vertex::bindings{{
    {
        .binding = 0,
        .stride = sizeof(tr::renderer::Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
}};

auto tr::renderer::StagingBuffer::commit(VkCommandBuffer cmd, VkBuffer dst, uint32_t dst_offset) -> StagingBuffer & {
  VkBufferCopy region{
      .srcOffset = offset,
      .dstOffset = dst_offset,
      .size = to_upload,
  };
  vkCmdCopyBuffer(cmd, buffer, dst, 1, &region);

  offset += to_upload;
  used = false;
  return *this;
}

auto tr::renderer::StagingBuffer::consume(std::size_t size) -> std::span<std::byte> {
  auto out = std::as_writable_bytes(std::span{reinterpret_cast<std::byte *>(alloc_info.pMappedData), alloc_info.size})
                 .subspan(offset, size);
  TR_ASSERT(!out.empty(), "StagingBuffer full");
  TR_ASSERT(!used, "StagingBuffer already used");
  used = true;
  to_upload = utils::narrow_cast<uint32_t>(size);
  return out;
}

void tr::renderer::StagingBuffer::reset() {
  to_upload = offset = 0;
  used = false;
}

auto tr::renderer::StagingBuffer::init(VmaAllocator allocator, uint32_t size) -> StagingBuffer {
  VkBuffer buf = VK_NULL_HANDLE;

  VkBufferCreateInfo buffer_create_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
  };
  VmaAllocationCreateInfo allocation_create_info{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
      .requiredFlags = 0,
      .preferredFlags = 0,
      .memoryTypeBits = 0,
      .pool = VK_NULL_HANDLE,
      .pUserData = nullptr,
      .priority = 1.0F,
  };
  VmaAllocation alloc = nullptr;
  VmaAllocationInfo alloc_info;
  VK_UNWRAP(vmaCreateBuffer, allocator, &buffer_create_info, &allocation_create_info, &buf, &alloc, &alloc_info);

  return {buf, alloc, alloc_info};
}
