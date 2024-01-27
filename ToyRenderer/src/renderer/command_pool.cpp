#include "command_pool.h"

#include <utils/assert.h>
#include <vulkan/vulkan_core.h>

#include "device.h"
#include "utils.h"

auto tr::renderer::CommandPool::init(Lifetime& lifetime, Device& device, PhysicalDevice& physical_device,
                                     CommandPool::TargetQueue target_queue) -> VkCommandPool {
  VkCommandPool command_pool = VK_NULL_HANDLE;

  std::uint32_t queue_family_index{};
  switch (target_queue) {
    case TargetQueue::Graphics:
      queue_family_index = physical_device.queues.graphics_family;
      break;
    case TargetQueue::Present:
      queue_family_index = physical_device.queues.present_family;
      break;
    case TargetQueue::Transfer:
      queue_family_index = physical_device.queues.transfer_family;
      break;
  }
  const VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = queue_family_index,
  };

  VK_UNWRAP(vkCreateCommandPool, device.vk_device, &command_pool_create_info, nullptr, &command_pool);
  lifetime.tie(DeviceHandle::CommandPool, command_pool);
  return command_pool;
}
