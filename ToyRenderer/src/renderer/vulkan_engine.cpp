#include "vulkan_engine.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <span>

#include "command_pool.h"
#include "timeline_info.h"
#include "utils.h"

struct OneTimeCommandBuffer {
  VkCommandBuffer vk_cmd;

  [[nodiscard]] auto begin() const -> VkResult {
    VkCommandBufferBeginInfo cmd_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    return vkBeginCommandBuffer(vk_cmd, &cmd_begin_info);
  }

  [[nodiscard]] auto end() const -> VkResult { return vkEndCommandBuffer(vk_cmd); }
};

void tr::renderer::VulkanEngine::write_cpu_timestamp(tr::renderer::CPUTimestampIndex index) {
  cpu_timestamps[index] = cpu_timestamp_clock::now();
}
void tr::renderer::VulkanEngine::write_gpu_timestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage,
                                                     GPUTimestampIndex index) {
  gpu_timestamps.write_cmd_query(cmd, pipelineStage, frame_id, index);
}

void tr::renderer::VulkanEngine::draw() {
  Frame frame = swapchain.acquire_next_frame(device, frame_synchronisation_pool.pop(device.vk_device));

  VK_UNWRAP(vkResetCommandBuffer, main_command_buffer, 0);

  OneTimeCommandBuffer cmd{main_command_buffer};
  VK_UNWRAP(cmd.begin);

  gpu_timestamps.reset_queries(cmd.vk_cmd, frame_id);
  write_gpu_timestamp(cmd.vk_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_TOP);
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_TOP);

  VkClearValue clear_value{.color = {.float32 = {1.0, 1.0, 1.0, 1.0}}};
  renderpass.begin(cmd.vk_cmd, frame.swapchain_image_index, VkRect2D{{0, 0}, swapchain.extent},
                   std::span{&clear_value, 1});
  renderpass.end(cmd.vk_cmd);

  write_gpu_timestamp(cmd.vk_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_BOTTOM);
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_BOTTOM);
  VK_UNWRAP(cmd.end);

  VK_UNWRAP(frame.submitCmds, device, std::span{&cmd.vk_cmd, 1});
  switch (VkResult result = frame.present(device, swapchain.vk_swapchain); result) {
    case VkResult::VK_ERROR_OUT_OF_DATE_KHR:
      spdlog::debug("OUT OF DATE");
      rebuild_swapchain();
      break;
    default:
      VK_CHECK(result, vkQueuePresentKHR);
  }
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_END);

  frame_synchronisation_pool.push_frame_synchro(frame_device_deletion_stack, frame.synchro);
  frame_device_deletion_stack.cleanup(device.vk_device);
}

void tr::renderer::VulkanEngine::rebuild_swapchain() {
  sync();
  swapchain_device_deletion_stack.cleanup(device.vk_device);

  swapchain = tr::renderer::Swapchain::init_with_config(swapchain.config, device, surface, window);
  swapchain.defer_deletion(swapchain_device_deletion_stack);

  renderpass = tr::renderer::Renderpass::init(device.vk_device, swapchain);
  renderpass.defer_deletion(swapchain_device_deletion_stack);
}

void tr::renderer::VulkanEngine::init(tr::Options& options, std::span<const char*> required_instance_extensions,
                                      GLFWwindow* window) {
  if (options.debug.renderdoc) {
    renderdoc = tr::renderer::Renderdoc::init();
  }

  this->window = window;
  instance = Instance::init(options, required_instance_extensions);
  instance.defer_deletion(global_deletion_stacks.instance);

  surface = Surface::init(instance.vk_instance, window);
  Surface::defer_deletion(surface, global_deletion_stacks.instance);

  device = Device::init(instance.vk_instance, surface);
  device.defer_deletion(global_deletion_stacks.instance);

  swapchain = Swapchain::init(options, device, surface, window);
  swapchain.defer_deletion(swapchain_device_deletion_stack);

  renderpass = tr::renderer::Renderpass::init(device.vk_device, swapchain);
  renderpass.defer_deletion(swapchain_device_deletion_stack);

  graphics_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Graphics);
  CommandPool::defer_deletion(graphics_command_pool, global_deletion_stacks.device);

  gpu_timestamps.reinit(device);
  gpu_timestamps.defer_deletion(global_deletion_stacks.device);

  VkCommandBufferAllocateInfo cmd_alloc_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = graphics_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VK_UNWRAP(vkAllocateCommandBuffers, device.vk_device, &cmd_alloc_info, &main_command_buffer);
}

tr::renderer::VulkanEngine::~VulkanEngine() {
  sync();
  frame_synchronisation_pool.cleanup(global_deletion_stacks.device);

  // TODO: add a free queue for buffers?
  vkFreeCommandBuffers(device.vk_device, graphics_command_pool, 1, &main_command_buffer);
  swapchain_device_deletion_stack.cleanup(device.vk_device);
  global_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.instance.cleanup(instance.vk_instance);
  vkDestroyInstance(instance.vk_instance, nullptr);
}
