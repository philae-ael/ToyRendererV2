#pragma once

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <optional>

#include "constants.h"
#include "debug.h"
#include "deletion_stack.h"
#include "descriptors.h"
#include "device.h"
#include "frame.h"
#include "instance.h"
#include "ressources.h"
#include "surface.h"
#include "swapchain.h"
#include "uploader.h"
#include "utils.h"
#include "utils/data/static_stack.h"

namespace tr::system {
class Imgui;
}

namespace tr::renderer {

class VulkanEngine {
 public:
  VulkanEngine() = default;
  void init(tr::Options& options, std::span<const char*> required_instance_extensions, GLFWwindow* window);

  void on_resize() { swapchain_need_to_be_rebuilt = true; }

  auto start_frame() -> std::optional<Frame>;
  void end_frame(Frame&&);

  auto start_transfer() -> Transferer;
  void end_transfer(Transferer&&);

  void sync() const { VK_UNWRAP(vkDeviceWaitIdle, device.vk_device); }

  ~VulkanEngine();

  VulkanEngine(const VulkanEngine&) = delete;
  VulkanEngine(VulkanEngine&&) = delete;
  auto operator=(const VulkanEngine&) -> VulkanEngine& = delete;
  auto operator=(VulkanEngine&&) -> VulkanEngine& = delete;

  VulkanEngineDebugInfo debug_info;

  [[nodiscard]] auto image_builder() const -> ImageBuilder { return {device.vk_device, allocator, &swapchain}; }
  [[nodiscard]] auto buffer_builder() const -> BufferBuilder { return {device.vk_device, allocator}; }

  // Lifetimes
  struct {  // Cleaned at exit
    InstanceDeletionStack instance;
    DeviceDeletionStack device;
    VmaDeletionStack allocator;
  } global_deletion_stacks;

  struct {  // Cleaned on swapchain recreation
    DeviceDeletionStack device;
    VmaDeletionStack allocator;
  } swapchain_deletion_stacks;

  struct {
    DeviceDeletionStack device;
    VmaDeletionStack allocator;
  } frame_deletion_stacks;

  void rebuild_swapchain();
  void build_ressources();

  GLFWwindow* window{};

  RessourceManager rm{};

  Instance instance;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Device device;
  VmaAllocator allocator = nullptr;
  std::array<DescriptorAllocator, MAX_FRAMES_IN_FLIGHT> frame_descriptor_allocators{};

  // Swapchain related
  Swapchain swapchain;
  bool swapchain_need_to_be_rebuilt = false;

  // FRAME STUFF
  std::uint32_t frame_id{};
  std::array<FrameSynchro, MAX_FRAMES_IN_FLIGHT> frame_synchronisation_pool{};
  std::array<VkCommandPool, MAX_FRAMES_IN_FLIGHT> graphic_command_pools{};
  std::array<OneTimeCommandBuffer, MAX_FRAMES_IN_FLIGHT> graphics_command_buffers{};
  VkCommandPool graphic_command_pool_for_next_frame = VK_NULL_HANDLE;
  utils::data::static_stack<VkCommandBuffer, 2> graphic_command_buffers_for_next_frame{};

  VkCommandPool transfer_command_pool = VK_NULL_HANDLE;
};

}  // namespace tr::renderer
