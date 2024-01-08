#include "descriptors.h"

#include <vulkan/vulkan_core.h>

#include <cstdint>

#include "deletion_queue.h"
#include "utils.h"
#include "utils/cast.h"

auto tr::renderer::DescriptorAllocator::init(VkDevice device, uint32_t max_sets,
                                             std::span<const VkDescriptorPoolSize> pool_sizes) -> DescriptorAllocator {
  DescriptorAllocator allocator{};

  const VkDescriptorPoolCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .maxSets = max_sets,
      .poolSizeCount = utils::narrow_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes = pool_sizes.data(),
  };

  VK_UNWRAP(vkCreateDescriptorPool, device, &create_info, nullptr, &allocator.pool);

  return allocator;
}

auto tr::renderer::DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) -> VkDescriptorSet {
  VkDescriptorSetAllocateInfo alloc_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout,
  };

  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  VK_UNWRAP(vkAllocateDescriptorSets, device, &alloc_info, &descriptor_set);
  return descriptor_set;
}

void tr::renderer::DescriptorAllocator::defer_deletion(DeviceDeletionStack& device_deletion_stack) {
  device_deletion_stack.defer_deletion(DeviceHandle::DescriptorPool, pool);
}

void tr::renderer::DescriptorAllocator::reset(VkDevice device) { VK_UNWRAP(vkResetDescriptorPool, device, pool, 0); }
