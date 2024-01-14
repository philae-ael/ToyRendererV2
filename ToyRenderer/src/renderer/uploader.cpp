#include "uploader.h"

#include <cstddef>

#include "utils/assert.h"

auto tr::renderer::StagingBuffer::commit(VkCommandBuffer cmd, VkBuffer dst, uint32_t dst_offset) -> StagingBuffer& {
  const VkBufferCopy region{
      .srcOffset = offset,
      .dstOffset = dst_offset,
      .size = to_upload,
  };
  vkCmdCopyBuffer(cmd, buffer, dst, 1, &region);

  offset += to_upload;
  return *this;
}
auto tr::renderer::StagingBuffer::commit_image(VkCommandBuffer cmd, const ImageRessource& image, VkRect2D r)
    -> StagingBuffer& {
  const VkBufferImageCopy region{
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
  offset = utils::align(offset, utils::narrow_cast<uint32_t>(alignement));

  auto out = std::as_writable_bytes(std::span{reinterpret_cast<std::byte*>(alloc_info.pMappedData), alloc_info.size})
                 .subspan(offset, size);
  to_upload = utils::narrow_cast<uint32_t>(size);
  return out;
}

void tr::renderer::StagingBuffer::reset() { to_upload = offset = 0; }

auto tr::renderer::StagingBuffer::init(VmaAllocator allocator, uint32_t size) -> StagingBuffer {
  VkBuffer buf = VK_NULL_HANDLE;

  const VkBufferCreateInfo buffer_create_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
  };
  const VmaAllocationCreateInfo allocation_create_info{
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
auto tr::renderer::Uploader::init(VmaAllocator allocator) -> Uploader {
  Uploader u{allocator};
  return u;
}

void tr::renderer::Uploader::defer_trim(VmaDeletionStack& allocator_deletion_queue) {
  for (auto& sb : staging_buffers) {
    sb.defer_deletion(allocator_deletion_queue);
  }
  staging_buffers.clear();
}
auto tr::renderer::Uploader::map(std::size_t size, std::size_t alignement) -> MappedMemoryRange {
  TR_ASSERT(staging_buffer_size > size, "Buffer too big: staging_buffer_size {}, size {}", staging_buffer_size, size);
  if (staging_buffers.empty() || staging_buffers.back().available() < size) {
    staging_buffers.push_back(StagingBuffer::init(allocator, staging_buffer_size));
  }

  return {staging_buffers.back().consume(size, alignement)};
}

void tr::renderer::Uploader::commit_buffer(VkCommandBuffer cmd, MappedMemoryRange /*mapped*/, VkBuffer buf,
                                           std::size_t size, std::size_t offset) {
  staging_buffers.back().to_upload = utils::narrow_cast<uint32_t>(size);
  staging_buffers.back().commit(cmd, buf, utils::narrow_cast<uint32_t>(offset));
}

void tr::renderer::Uploader::commit_image(VkCommandBuffer cmd, MappedMemoryRange /*mapped*/,
                                          const ImageRessource& image, VkRect2D r, std::size_t size) {
  staging_buffers.back().to_upload = utils::narrow_cast<uint32_t>(size);
  staging_buffers.back().commit_image(cmd, image, r);
}
