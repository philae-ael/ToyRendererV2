
#include "vertex.h"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
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
        .pos = {-1, -1, 0},
        .base_color = {1, 0, 0},
        .uv = {0, 0},
    },
    {
        .pos = {1, -1, 0},
        .base_color = {0, 0, 1},
        .uv = {0, 0},
    },
    {
        .pos = {0, 1, 0},
        .base_color = {0, 1, 0},
        .uv = {0, 0},
    },
}};

auto tr::renderer::TriangleVertexBuffer(tr::renderer::Device &device, VkCommandBuffer cmd,
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

  VK_UNWRAP(vkCreateBuffer, device.vk_device, &buffer_create_info, nullptr, &buf);

  VkMemoryRequirements memory_requirements{};
  vkGetBufferMemoryRequirements(device.vk_device, buf, &memory_requirements);

  std::uint32_t memory_type =
      device.find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkMemoryAllocateInfo allocation_info{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memory_requirements.size,
      .memoryTypeIndex = memory_type,
  };

  VkDeviceMemory memory = VK_NULL_HANDLE;

  VK_UNWRAP(vkAllocateMemory, device.vk_device, &allocation_info, nullptr, &memory);
  vkBindBufferMemory(device.vk_device, buf, memory, 0);

  staging_buffer.stage(device.vk_device, cmd, buf,
                       std::span{reinterpret_cast<const std::byte *>(triangle.data()), buffer_create_info.size});

  return {buf, memory};
}

void tr::renderer::StagingBuffer::stage(VkDevice device, VkCommandBuffer cmd, VkBuffer dst,
                                        std::span<const std::byte> data) {
  TR_ASSERT(data.size() <= size, "trying to stage too much data");
  void *mem = nullptr;
  VK_UNWRAP(vkMapMemory, device, buffer.device_memory, 0, size, 0, &mem);

  std::memcpy(mem, triangle.data(), data.size());

  vkUnmapMemory(device, buffer.device_memory);

  VkBufferCopy region{
      .srcOffset = 0,
      .dstOffset = 0,
      .size = data.size(),
  };
  vkCmdCopyBuffer(cmd, buffer.vk_buffer, dst, 1, &region);
}

auto tr::renderer::StagingBuffer::init(Device &device, std::size_t size) -> StagingBuffer {
  VkBuffer buf = VK_NULL_HANDLE;

  VkBufferCreateInfo buffer_create_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size,
      .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
  };

  VK_UNWRAP(vkCreateBuffer, device.vk_device, &buffer_create_info, nullptr, &buf);

  VkMemoryRequirements memory_requirements{};
  vkGetBufferMemoryRequirements(device.vk_device, buf, &memory_requirements);

  std::uint32_t memory_type = device.find_memory_type(
      memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkMemoryAllocateInfo allocation_info{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memory_requirements.size,
      .memoryTypeIndex = memory_type,
  };

  VkDeviceMemory memory = VK_NULL_HANDLE;

  VK_UNWRAP(vkAllocateMemory, device.vk_device, &allocation_info, nullptr, &memory);
  vkBindBufferMemory(device.vk_device, buf, memory, 0);

  return {size, {buf, memory}};
}
