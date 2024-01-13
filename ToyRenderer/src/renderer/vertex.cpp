
#include "vertex.h"

#include <utils/cast.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "ressources.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/misc.h"

const std::array<VkVertexInputAttributeDescription, 6> tr::renderer::Vertex::attributes = {{
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
    },    {
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, tangent),
    },
    {
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color),
    },
    {
        .location = 4,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, uv1),
    },
    {
        .location = 5,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, uv2),
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
  return *this;
}
auto tr::renderer::StagingBuffer::commit_image(VkCommandBuffer cmd, const ImageRessource &image, VkRect2D r)
    -> StagingBuffer & {
  VkBufferImageCopy region{
      .bufferOffset = offset,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
      .imageOffset = {r.offset.x, r.offset.y, 0},
      .imageExtent = {r.extent.width, r.extent.height, 1},
  };

  vkCmdCopyBufferToImage(cmd, buffer, image.image, image.sync_info.layout, 1, &region);

  offset += to_upload;
  return *this;
}

auto tr::renderer::StagingBuffer::consume(std::size_t size, std::size_t alignement) -> std::span<std::byte> {
  TR_ASSERT(available(alignement) >= size, "StagingBuffer already filled");
  offset = utils::align(offset, alignement);

  auto out = std::as_writable_bytes(std::span{reinterpret_cast<std::byte *>(alloc_info.pMappedData), alloc_info.size})
                 .subspan(offset, size);
  to_upload = utils::narrow_cast<uint32_t>(size);
  return out;
}

void tr::renderer::StagingBuffer::reset() { to_upload = offset = 0; }

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
