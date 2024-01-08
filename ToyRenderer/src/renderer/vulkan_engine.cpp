#include "vulkan_engine.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <span>

#include "command_pool.h"
#include "swapchain.h"
#include "timeline_info.h"
#include "utils.h"
#include "utils/cast.h"
#include "vertex.h"

void tr::renderer::VulkanEngine::write_cpu_timestamp(tr::renderer::CPUTimestampIndex index) {
  cpu_timestamps[index] = cpu_timestamp_clock::now();
}
void tr::renderer::VulkanEngine::write_gpu_timestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage,
                                                     GPUTimestampIndex index) {
  gpu_timestamps.write_cmd_query(cmd, pipelineStage, frame_id, index);
}

auto tr::renderer::VulkanEngine::start_frame() -> std::pair<bool, Frame> {
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_TOP);
  frame_id += 1;

  Frame frame{};
  frame.synchro = frame_synchronisation_pool[frame_id % MAX_IN_FLIGHT];
  VK_UNWRAP(vkWaitForFences, device.vk_device, 1, &frame.synchro.render_fence, VK_TRUE, 1000000000);

  switch (VkResult result = swapchain.acquire_next_frame(device, &frame); result) {
    case VK_ERROR_OUT_OF_DATE_KHR:
      rebuild_swapchain();
      return {false, frame};
    case VK_SUBOPTIMAL_KHR:
      break;
    default:
      VK_CHECK(result, swapchain.acquire_next_frame);
  }

  frame.cmd = OneTimeCommandBuffer{main_command_buffer_pool[frame_id % MAX_IN_FLIGHT]};
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_BOTTOM);
  return {true, frame};
}

void tr::renderer::VulkanEngine::draw(Frame frame) {
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_TOP);
  VK_UNWRAP(vkResetCommandBuffer, frame.cmd.vk_cmd, 0);

  VK_UNWRAP(frame.cmd.begin);
  gpu_timestamps.reset_queries(frame.cmd.vk_cmd, frame_id);
  write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_TOP);

  VkClearValue clear_value{.color = {.float32 = {1.0, 1.0, 1.0, 1.0}}};
  renderpass.begin(frame.cmd.vk_cmd, frame.swapchain_image_index, VkRect2D{{0, 0}, swapchain.extent},
                   std::span{&clear_value, 1});

  VkRect2D scissor{{0, 0}, swapchain.extent};
  vkCmdSetScissor(frame.cmd.vk_cmd, 0, 1, &scissor);

  VkViewport viewport{0,   0,  static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height),
                      0.0, 1.0};

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &triangle_vertex_buffer.vk_buffer, &offset);
  vkCmdSetViewport(frame.cmd.vk_cmd, 0, 1, &viewport);

  vkCmdBindPipeline(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vk_pipeline);
  vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0);

  renderpass.end(frame.cmd.vk_cmd);

  write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_BOTTOM);
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_BOTTOM);
}

void tr::renderer::VulkanEngine::end_frame(Frame frame) {
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_TOP);
  VK_UNWRAP(frame.cmd.end);
  VK_UNWRAP(vkResetFences, device.vk_device, 1, &frame.synchro.render_fence);
  VK_UNWRAP(frame.submitCmds, device, std::span{&frame.cmd.vk_cmd, 1});
  // TODO: there should be a queue ownership transfer if graphics queue != present queue
  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#multiple-queues
  VkResult result = frame.present(device, swapchain.vk_swapchain);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || fb_resized) {
    rebuild_swapchain();
    fb_resized = false;
  } else {
    VK_CHECK(result, vkQueuePresentKHR);
  }

  frame_device_deletion_stack.cleanup(device.vk_device);
  write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_BOTTOM);
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
  transfer_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Transfer);
  CommandPool::defer_deletion(transfer_command_pool, global_deletion_stacks.device);

  VkCommandBufferAllocateInfo cmd_alloc_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = graphics_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = utils::narrow_cast<uint32_t>(main_command_buffer_pool.size()),
  };

  VK_UNWRAP(vkAllocateCommandBuffers, device.vk_device, &cmd_alloc_info, main_command_buffer_pool.data());

  VkCommandBufferAllocateInfo transfer_cmd_alloc_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = transfer_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VK_UNWRAP(vkAllocateCommandBuffers, device.vk_device, &transfer_cmd_alloc_info, &transfer_command_buffer);

  staging_buffer = tr::renderer::StagingBuffer::init(device, 4096);  // A triangle is 108 bytes (i think)
  staging_buffer.defer_deletion(global_deletion_stacks.device);

  OneTimeCommandBuffer transfer_cmd{transfer_command_buffer};
  VK_UNWRAP(transfer_cmd.begin);

  triangle_vertex_buffer = tr::renderer::TriangleVertexBuffer(device, transfer_cmd.vk_cmd, staging_buffer);
  triangle_vertex_buffer.defer_deletion(global_deletion_stacks.device);

  VK_UNWRAP(transfer_cmd.end);
  VkSubmitInfo submit_info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = nullptr,
      .pWaitDstStageMask = nullptr,
      .commandBufferCount = 1,
      .pCommandBuffers = &transfer_cmd.vk_cmd,
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = nullptr,
  };
  vkQueueSubmit(device.queues.transfer_queue, 1, &submit_info, VK_NULL_HANDLE);

  // TODO: add a fence | semaphore (a fence i think should be more suited) to prevent Synchronization issues

  pipeline = Pipeline::init(device, renderpass.vk_renderpass);
  pipeline.defer_deletion(global_deletion_stacks.device);

  gpu_timestamps.reinit(device);
  gpu_timestamps.defer_deletion(global_deletion_stacks.device);

  for (auto& synchro : frame_synchronisation_pool) {
    synchro = FrameSynchro::init(device.vk_device);
    synchro.defer_deletion(global_deletion_stacks.device);
  }
}

tr::renderer::VulkanEngine::~VulkanEngine() {
  sync();
  // TODO: add a free queue for buffers?
  vkFreeMemory(device.vk_device, staging_buffer.buffer.device_memory, nullptr);
  vkFreeMemory(device.vk_device, triangle_vertex_buffer.device_memory, nullptr);
  vkFreeCommandBuffers(device.vk_device, graphics_command_pool, main_command_buffer_pool.size(),
                       main_command_buffer_pool.data());
  swapchain_device_deletion_stack.cleanup(device.vk_device);
  global_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.instance.cleanup(instance.vk_instance);
  vkDestroyInstance(instance.vk_instance, nullptr);
}
