#include "deletion_stack.h"

#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#define DESTROY_WITH_INSTANCE(name)                                         \
  case InstanceHandle::name:                                                \
    vkDestroy##name(instance, reinterpret_cast<Vk##name>(handle), nullptr); \
    break;

void tr::renderer::InstanceDeletionStack::cleanup(VkInstance instance) {
  while (!stack.empty()) {
    auto [type, handle] = stack.back();
    stack.pop_back();
    // NOLINTBEGIN(performance-no-int-to-ptr)
    switch (type) {
      case tr::renderer::InstanceHandle::Device:
        vkDestroyDevice(reinterpret_cast<VkDevice>(handle), nullptr);
        break;
        DESTROY_WITH_INSTANCE(DebugUtilsMessengerEXT)
        DESTROY_WITH_INSTANCE(SurfaceKHR)
    }
    // NOLINTEND(performance-no-int-to-ptr)
  }
}

#undef DESTROY_WITH_INSTANCE

#define DESTROY_WITH_DEVICE(name)                                         \
  case DeviceHandle::name:                                                \
    vkDestroy##name(device, reinterpret_cast<Vk##name>(handle), nullptr); \
    break;

void tr::renderer::DeviceDeletionStack::cleanup(VkDevice device) {
  while (!stack.empty()) {
    auto [type, handle] = stack.back();
    stack.pop_back();
    // NOLINTBEGIN(performance-no-int-to-ptr)
    switch (type) {
      DESTROY_WITH_DEVICE(CommandPool)
      DESTROY_WITH_DEVICE(SwapchainKHR)
      DESTROY_WITH_DEVICE(ImageView)
      DESTROY_WITH_DEVICE(RenderPass)
      DESTROY_WITH_DEVICE(Framebuffer)
      DESTROY_WITH_DEVICE(Fence)
      DESTROY_WITH_DEVICE(Semaphore)
      DESTROY_WITH_DEVICE(QueryPool)
      DESTROY_WITH_DEVICE(Pipeline)
      DESTROY_WITH_DEVICE(PipelineLayout)
      DESTROY_WITH_DEVICE(Buffer)
      DESTROY_WITH_DEVICE(DescriptorPool)
      DESTROY_WITH_DEVICE(ShaderModule)
      DESTROY_WITH_DEVICE(DescriptorSetLayout)
      DESTROY_WITH_DEVICE(Sampler)
    }
    // NOLINTEND(performance-no-int-to-ptr)
  }
}
#undef DESTROY_WITH_DEVICE

#define DESTROY_WITH_ALLOCATOR(name)                                                      \
  case VmaHandle::name:                                                                   \
    vmaDestroy##name(allocator, reinterpret_cast<Vk##name>(handle.first), handle.second); \
    break;

void tr::renderer::VmaDeletionStack::cleanup(VmaAllocator allocator) {
  while (!stack.empty()) {
    auto [type, handle] = stack.back();
    stack.pop_back();
    // NOLINTBEGIN(performance-no-int-to-ptr)
    switch (type) {
      DESTROY_WITH_ALLOCATOR(Buffer)
      DESTROY_WITH_ALLOCATOR(Image)
    }
    // NOLINTEND(performance-no-int-to-ptr)
  }
}
#undef DESTROY_WITH_ALLOCATOR
