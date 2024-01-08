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

#include "constants.h"
#include "debug.h"
#include "deletion_queue.h"
#include "device.h"
#include "instance.h"
#include "passes/deferred.h"
#include "passes/gbuffer.h"
#include "ressources.h"
#include "surface.h"
#include "swapchain.h"
#include "utils.h"

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
  void draw(Frame);
  void end_frame(Frame);

  void sync() const { VK_UNWRAP(vkDeviceWaitIdle, device.vk_device); }

  ~VulkanEngine();

  VulkanEngine(const VulkanEngine&) = delete;
  VulkanEngine(VulkanEngine&&) = delete;
  auto operator=(const VulkanEngine&) -> VulkanEngine& = delete;
  auto operator=(VulkanEngine&&) -> VulkanEngine& = delete;

  friend VulkanEngineDebugInfo;
  VulkanEngineDebugInfo debug_info;

  FrameRessourceManager rm{};
  ImageRessourceStorage fb0_ressources{
      .definition = GBuffer::definitions[0],
      .ressources = {},
      .debug_name = "fb0",
  };
  ImageRessourceStorage fb1_ressources{
      .definition = GBuffer::definitions[1],
      .ressources = {},
      .debug_name = "fb1",
  };
  ImageRessourceStorage depth_ressources{
      .definition = GBuffer::definitions[2],
      .ressources = {},
      .debug_name = "depth",
  };

 private:
  void rebuild_swapchain();
  void build_ressources();
  void upload(DeviceDeletionStack&, VmaDeletionStack&);

  GLFWwindow* window{};

  struct {
    GBuffer gbuffer;
    Deferred deferred;
  } passes;

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
  } frame_deletion_stacks;

  Instance instance;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Device device;
  VmaAllocator allocator = nullptr;

  // Swapchain related
  Swapchain swapchain;
  bool swapchain_need_to_be_rebuilt = false;

  // FRAME STUFF
  std::size_t frame_id{};
  std::array<FrameSynchro, MAX_FRAMES_IN_FLIGHT> frame_synchronisation_pool;
  std::array<VkCommandPool, MAX_FRAMES_IN_FLIGHT> graphic_command_pools{};
  std::array<OneTimeCommandBuffer, MAX_FRAMES_IN_FLIGHT> graphics_command_buffers{};

  // Data shall be moved, one day
  BufferRessource triangle_vertex_buffer;

  friend system::Imgui;
};

}  // namespace tr::renderer
