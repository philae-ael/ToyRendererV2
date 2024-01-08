#pragma once

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <optional>

#include "debug.h"
#include "deletion_queue.h"
#include "device.h"
#include "instance.h"
#include "pipeline.h"
#include "renderpass.h"
#include "surface.h"
#include "swapchain.h"
#include "utils.h"
#include "vertex.h"

namespace tr::system {
class Imgui;
}

namespace tr::renderer {

class VulkanEngine {
 public:
  VulkanEngine() = default;
  void init(tr::Options& options, std::span<const char*> required_instance_extensions, GLFWwindow* window);

  void on_resize() { fb_resized = true; }

  auto start_frame() -> std::optional<Frame>;
  void draw(Frame);
  void end_frame(Frame);

  void sync() const { VK_UNWRAP(vkDeviceWaitIdle, device.vk_device); }
  void rebuild_swapchain();

  ~VulkanEngine();

  VulkanEngine(const VulkanEngine&) = delete;
  VulkanEngine(VulkanEngine&&) = delete;
  auto operator=(const VulkanEngine&) -> VulkanEngine& = delete;
  auto operator=(VulkanEngine&&) -> VulkanEngine& = delete;

  friend VulkanEngineDebugInfo;
  VulkanEngineDebugInfo debug_info;

 private:
  GLFWwindow* window{};

  // Lifetimes
  struct {  // Cleaned at exit
    InstanceDeletionStack instance;
    DeviceDeletionStack device;
    VmaDeletionStack allocator;
  } global_deletion_stacks;

  struct {  // Cleaned on swapchain recreation
    DeviceDeletionStack device;
  } swapchain_deletion_stacks;

  struct {
    DeviceDeletionStack device;
    VmaDeletionStack allocator;
  } frame_deletion_stacks;

  Instance instance;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Device device;
  VmaAllocator allocator = nullptr;

  VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
  VkCommandPool transfer_command_pool = VK_NULL_HANDLE;
  VkCommandBuffer transfer_command_buffer{};

  // Swapchain related
  Swapchain swapchain;
  bool fb_resized = false;
  Renderpass renderpass;

  // FRAME STUFF
  std::size_t frame_id{};
  std::array<FrameSynchro, MAX_FRAMES_IN_FLIGHT> frame_synchronisation_pool;
  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> main_command_buffer_pool{};

  // Data shall be moved, one day
  Pipeline pipeline;
  Buffer triangle_vertex_buffer;
  StagingBuffer staging_buffer;

  friend system::Imgui;
};

}  // namespace tr::renderer
