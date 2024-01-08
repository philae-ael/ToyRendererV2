#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <deque>

#include "utils/assert.h"
namespace tr::renderer {

template <class ObjectType>
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
  std::deque<std::pair<ObjectType, uint64_t>> stack;
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
};

class DeviceDeletionStack : public DeletionStack<DeviceHandle> {
 public:
  void cleanup(VkDevice device);
};

}  // namespace tr::renderer
