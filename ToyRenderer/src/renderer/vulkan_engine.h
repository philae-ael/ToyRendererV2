#pragma once

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <optional>
#include <utility>

#include "constants.h"
#include "context.h"
#include "debug.h"
#include "deletion_stack.h"
#include "descriptors.h"
#include "device.h"
#include "frame.h"
#include "ressource_manager.h"
#include "ressources.h"
#include "surface.h"
#include "uploader.h"
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

  template <class Fn>
  void frame(Fn&& f) {
    auto f_ = std::forward<Fn>(f);
    if (auto frame = start_frame(); frame) {
      f_(*frame);
      end_frame(std::move(*frame));
    }
  }

  auto start_transfer() -> Transferer;
  void end_transfer(Transferer&&);

  template <class Fn>
  void transfer(Fn&& f) {
    auto f_ = std::forward<Fn>(f);
    auto transferer = start_transfer();
    f_(transferer);
    end_transfer(std::move(transferer));
  }

  void sync();
  void imgui() { debug_info.imgui(*this); }

  [[nodiscard]] auto image_builder() const -> ImageBuilder { return {ctx.device.vk_device, allocator, &ctx.swapchain}; }
  [[nodiscard]] auto buffer_builder() const -> BufferBuilder { return {ctx.device.vk_device, allocator}; }

  ~VulkanEngine();

  VulkanEngine(const VulkanEngine&) = delete;
  VulkanEngine(VulkanEngine&&) = delete;
  auto operator=(const VulkanEngine&) -> VulkanEngine& = delete;
  auto operator=(VulkanEngine&&) -> VulkanEngine& = delete;

  struct Lifetimes {
    Lifetime global;
    Lifetime swapchain;
    Lifetime frame;
  } lifetime;

  VulkanContext ctx;
  VmaAllocator allocator = nullptr;
  tr::renderer::RessourceManager rm{};
  std::array<std::optional<tr::renderer::FrameRessourceData>, MAX_FRAMES_IN_FLIGHT> frame_ressource_data{};
  image_ressource_handle swapchain_handle{};

  mutable VulkanEngineDebugInfo debug_info;

 private:
  void rebuild_swapchain();
  void rebuild_invalidated();
  void build_ressources();

  GLFWwindow* window{};

  std::array<DescriptorAllocator, MAX_FRAMES_IN_FLIGHT> frame_descriptor_allocators{};

  // Swapchain related
  bool swapchain_need_to_be_rebuilt = false;

  // FRAME STUFF
  std::uint32_t frame_id{};
  std::array<FrameSynchro, MAX_FRAMES_IN_FLIGHT> frame_synchronisation_pool{};
  std::array<VkCommandPool, MAX_FRAMES_IN_FLIGHT> graphic_command_pools{};
  std::array<OneTimeCommandBuffer, MAX_FRAMES_IN_FLIGHT> graphics_command_buffers{};
  VkCommandPool graphic_command_pool_for_next_frame = VK_NULL_HANDLE;
  utils::data::static_stack<VkCommandBuffer, 2> graphic_command_buffers_for_next_frame{};

  VkCommandPool transfer_command_pool = VK_NULL_HANDLE;
  friend VulkanEngineDebugInfo;
};

}  // namespace tr::renderer
