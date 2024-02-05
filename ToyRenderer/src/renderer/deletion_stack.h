#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <deque>
#include <iterator>
#include <utility>

#include "utils/assert.h"

namespace tr::renderer {

template <class ObjectType, class Handle = uint64_t>
class DeletionStack {
 public:
  template <class T>
  void defer_deletion(ObjectType type, T handle) {
    stack.emplace_back(type, reinterpret_cast<uint64_t>(handle));
  }

  ~DeletionStack() {
    TR_ASSERT(stack.empty(),
              "deletion queue is not empty, please call the cleanup function "
              "as needed");
  }

  DeletionStack() = default;
  DeletionStack(const DeletionStack &) = delete;
  DeletionStack(DeletionStack &&) noexcept = default;
  auto operator=(const DeletionStack &) -> DeletionStack & = delete;
  auto operator=(DeletionStack &&) -> DeletionStack & = default;

 protected:
  std::deque<std::pair<ObjectType, Handle>> stack;
};

enum class InstanceHandle { Device, DebugUtilsMessengerEXT, SurfaceKHR };

class InstanceDeletionStack : public DeletionStack<InstanceHandle> {
 public:
  void cleanup(VkInstance instance);
};

enum class DeviceHandle {
  CommandPool,
  ImageView,
  SwapchainKHR,
  Framebuffer,
  RenderPass,
  Fence,
  Semaphore,
  QueryPool,
  Pipeline,
  PipelineLayout,
  Buffer,
  DescriptorPool,
  ShaderModule,
  DescriptorSetLayout,
  Sampler,
};

class DeviceDeletionStack : public DeletionStack<DeviceHandle> {
 public:
  void cleanup(VkDevice device);
};

enum class VmaHandle {
  Buffer,
  Image,
};

class VmaDeletionStack : public DeletionStack<VmaHandle, std::pair<uint64_t, VmaAllocation>> {
 public:
  template <class T>
  void defer_deletion(VmaHandle type, T handle, VmaAllocation alloc) {
    stack.emplace_back(type, std::pair{reinterpret_cast<uint64_t>(handle), alloc});
  }

  void cleanup(VmaAllocator allocator);
};

struct Lifetime {
  DeviceDeletionStack device;
  VmaDeletionStack allocator;

  template <class T>
  void tie(VmaHandle type, T handle, VmaAllocation alloc) {
    allocator.defer_deletion(type, handle, alloc);
  }

  template <class T>
  void tie(DeviceHandle type, T handle) {
    device.defer_deletion(type, handle);
  }

  void cleanup(VkDevice device_, VmaAllocator allocator_) {
    device.cleanup(device_);
    allocator.cleanup(allocator_);
  }
};

}  // namespace tr::renderer
