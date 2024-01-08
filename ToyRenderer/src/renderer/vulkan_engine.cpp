#include "vulkan_engine.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <span>

#include "command_pool.h"
#include "swapchain.h"
#include "timeline_info.h"
#include "utils.h"
#include "utils/cast.h"

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
  std::size_t in_flight_index = frame_id % MAX_IN_FLIGHT;
  Frame frame{};
  frame.synchro = frame_synchronisation_pool[in_flight_index];
  VK_UNWRAP(vkWaitForFences, device.vk_device, 1, &frame.synchro.render_fence, VK_TRUE, 1000000000);

  switch (VkResult result = swapchain.acquire_next_frame(device, &frame); result) {
    case VK_ERROR_OUT_OF_DATE_KHR:
      rebuild_swapchain();
      return;
    case VK_SUBOPTIMAL_KHR:
      break;
    default:
      VK_CHECK(result, swapchain.acquire_next_frame);
  }

  OneTimeCommandBuffer cmd{main_command_buffer_pool[in_flight_index]};
  VK_UNWRAP(vkResetCommandBuffer, cmd.vk_cmd, 0);

  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_TOP);
  VK_UNWRAP(cmd.begin);
  gpu_timestamps.reset_queries(cmd.vk_cmd, frame_id);
  write_gpu_timestamp(cmd.vk_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_TOP);

  VkClearValue clear_value{.color = {.float32 = {1.0, 1.0, 1.0, 1.0}}};
  renderpass.begin(cmd.vk_cmd, frame.swapchain_image_index, VkRect2D{{0, 0}, swapchain.extent},
                   std::span{&clear_value, 1});
  renderpass.end(cmd.vk_cmd);

  write_gpu_timestamp(cmd.vk_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_BOTTOM);
  VK_UNWRAP(cmd.end);
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_BOTTOM);

  VK_UNWRAP(vkResetFences, device.vk_device, 1, &frame.synchro.render_fence);
  VK_UNWRAP(frame.submitCmds, device, std::span{&cmd.vk_cmd, 1});
  VkResult result = frame.present(device, swapchain.vk_swapchain);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || fb_resized) {
    rebuild_swapchain();
    fb_resized = false;
  } else {
    VK_CHECK(result, vkQueuePresentKHR);
  }
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_END);

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

  for (auto& synchro : frame_synchronisation_pool) {
    synchro = FrameSynchro::init(device.vk_device);
    synchro.defer_deletion(global_deletion_stacks.device);
  }

  VkCommandBufferAllocateInfo cmd_alloc_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = graphics_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = utils::narrow_cast<uint32_t>(main_command_buffer_pool.size()),
  };

  VK_UNWRAP(vkAllocateCommandBuffers, device.vk_device, &cmd_alloc_info, main_command_buffer_pool.data());
}

tr::renderer::VulkanEngine::~VulkanEngine() {
  sync();
  // TODO: add a free queue for buffers?
  vkFreeCommandBuffers(device.vk_device, graphics_command_pool, main_command_buffer_pool.size(),
                       main_command_buffer_pool.data());
  swapchain_device_deletion_stack.cleanup(device.vk_device);
  global_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.instance.cleanup(instance.vk_instance);
  vkDestroyInstance(instance.vk_instance, nullptr);
}
