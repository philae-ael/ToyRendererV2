#include "command_pool.h"

#include <utils/assert.h>
#include <vulkan/vulkan_core.h>

#include "device.h"
#include "utils.h"

auto tr::renderer::CommandPool::init(Device &device,
                                     CommandPool::TargetQueue target_queue)
    -> VkCommandPool {
  VkCommandPool command_pool = VK_NULL_HANDLE;

  std::uint32_t queue_family_index{};
  switch (target_queue) {
    case TargetQueue::Graphics:
      queue_family_index = device.queues.graphics_family;
      break;
    case TargetQueue::Present:
      queue_family_index = device.queues.present_family;
      break;
  }
  VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queue_family_index,

  };

  VK_UNWRAP(vkCreateCommandPool, device.vk_device, &command_pool_create_info,
            nullptr, &command_pool);
  return command_pool;
}
