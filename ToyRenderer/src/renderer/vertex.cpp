
#include "vertex.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "utils.h"
#include "utils/assert.h"

const std::array<VkVertexInputAttributeDescription, 3> tr::renderer::Vertex::attributes = {{
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
        .offset = offsetof(Vertex, base_color),
    },
    {
        .location = 2,
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

const std::array<tr::renderer::Vertex, 3> triangle{{
    {
        .pos = {-1, -1, 0.5},
        .base_color = {1, 0, 0},
        .uv = {0, 0},
    },
    {
        .pos = {1, -1, 0.5},
        .base_color = {0, 0, 1},
        .uv = {0, 0},
    },
    {
        .pos = {0, 1, 0.5},
        .base_color = {0, 1, 0},
        .uv = {0, 0},
    },
}};

auto tr::renderer::TriangleVertexBuffer(Device &device, VmaAllocator allocator, VkCommandBuffer cmd,
                                        StagingBuffer &staging_buffer) -> tr::renderer::Buffer {
  VkBuffer buf = VK_NULL_HANDLE;

  std::array<std::uint32_t, 2> queues{
      device.queues.graphics_family,
      device.queues.transfer_family,
  };
  VkBufferCreateInfo buffer_create_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = triangle.size() * sizeof(tr::renderer::Vertex),
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_CONCURRENT,
      .queueFamilyIndexCount = utils::narrow_cast<uint32_t>(queues.size()),
      .pQueueFamilyIndices = queues.data(),
  };

  if (device.queues.graphics_family == device.queues.transfer_family) {
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = nullptr;
  }

  VmaAllocationCreateInfo allocation_create_info{
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
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

  staging_buffer.with_data(std::span{reinterpret_cast<const std::byte *>(triangle.data()), buffer_create_info.size})
      .upload(cmd, buf);

  return {buf, alloc, alloc_info};
}

auto tr::renderer::StagingBuffer::upload(VkCommandBuffer cmd, VkBuffer dst) -> StagingBuffer & {
  VkBufferCopy region{
      .srcOffset = offset,
      .dstOffset = 0,
      .size = to_upload,
  };
  vkCmdCopyBuffer(cmd, buffer, dst, 1, &region);
  offset += to_upload;
  to_upload = 0;

  return *this;
}

void tr::renderer::StagingBuffer::reset() {
  to_upload = 0;
  offset = 0;
}

auto tr::renderer::StagingBuffer::with_data(std::span<const std::byte> data) -> StagingBuffer & {
  TR_ASSERT(offset + to_upload + data.size() <= size, "staging buffer too small");
  memcpy(alloc_info.pMappedData, data.data(), data.size());
  to_upload = data.size();
  offset = 0;
  return *this;
}

auto tr::renderer::StagingBuffer::init(VmaAllocator &allocator, uint32_t size) -> StagingBuffer {
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

  return {size, 0, 0, buf, alloc, alloc_info};
}
