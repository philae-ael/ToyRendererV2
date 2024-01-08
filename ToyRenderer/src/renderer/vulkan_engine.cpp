#include "vulkan_engine.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <optional>
#include <span>

#include "command_pool.h"
#include "debug.h"
#include "queue.h"
#include "swapchain.h"
#include "synchronisation.h"
#include "timeline_info.h"
#include "utils.h"
#include "utils/cast.h"
#include "vertex.h"

auto tr::renderer::VulkanEngine::start_frame() -> std::optional<Frame> {
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_TOP);
  frame_id += 1;

  // Try to get frame or bail early
  Frame frame{};
  frame.synchro = frame_synchronisation_pool[frame_id % MAX_FRAMES_IN_FLIGHT];
  VK_UNWRAP(vkWaitForFences, device.vk_device, 1, &frame.synchro.render_fence, VK_TRUE, 1000000000);
  switch (VkResult result = swapchain.acquire_next_frame(device, &frame); result) {
    case VK_ERROR_OUT_OF_DATE_KHR:
      rebuild_swapchain();
      return std::nullopt;
    case VK_SUBOPTIMAL_KHR:
      break;
    default:
      VK_CHECK(result, swapchain.acquire_next_frame);
  }
  VK_UNWRAP(vkResetFences, device.vk_device, 1, &frame.synchro.render_fence);

  frame.cmd = OneTimeCommandBuffer{main_command_buffer_pool[frame_id % MAX_FRAMES_IN_FLIGHT]};
  VK_UNWRAP(vkResetCommandBuffer, frame.cmd.vk_cmd, 0);
  VK_UNWRAP(frame.cmd.begin);

  // Update frame id for subsystems
  vmaSetCurrentFrameIndex(allocator, frame_id);
  debug_info.set_frame(frame, frame_id);
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_BOTTOM);

  return frame;
}

void tr::renderer::VulkanEngine::draw(Frame frame) {
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_TOP);
  debug_info.write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_TOP);

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
  debug_info.write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_BOTTOM);
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_BOTTOM);
}

void tr::renderer::VulkanEngine::end_frame(Frame frame) {
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_TOP);
  ColorAttachmentToPresentImageBarrier.copy()
      .image(swapchain.images[frame.swapchain_image_index])
      .submit(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  VK_UNWRAP(frame.cmd.end);
  VK_UNWRAP(frame.submitCmds, device.queues.graphics_queue);
  // TODO: there should be a queue ownership transfer if graphics queue != present queue
  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#multiple-queues
  VkResult result = frame.present(device, swapchain.vk_swapchain);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || fb_resized) {
    rebuild_swapchain();
    fb_resized = false;
  } else {
    VK_CHECK(result, vkQueuePresentKHR);
  }

  frame_deletion_stacks.allocator.cleanup(allocator);
  frame_deletion_stacks.device.cleanup(device.vk_device);
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_BOTTOM);
}

void tr::renderer::VulkanEngine::rebuild_swapchain() {
  sync();
  swapchain_deletion_stacks.device.cleanup(device.vk_device);

  swapchain = tr::renderer::Swapchain::init_with_config(swapchain.config, device, surface, window);
  swapchain.defer_deletion(swapchain_deletion_stacks.device);

  renderpass = tr::renderer::Renderpass::init(device.vk_device, swapchain);
  renderpass.defer_deletion(swapchain_deletion_stacks.device);
}

void tr::renderer::VulkanEngine::init(tr::Options& options, std::span<const char*> required_instance_extensions,
                                      GLFWwindow* w) {
  if (options.debug.renderdoc) {
    debug_info.renderdoc = tr::renderer::Renderdoc::init();
  }

  window = w;
  instance = Instance::init(options, required_instance_extensions);
  instance.defer_deletion(global_deletion_stacks.instance);

  surface = Surface::init(instance.vk_instance, w);
  Surface::defer_deletion(surface, global_deletion_stacks.instance);

  device = Device::init(instance.vk_instance, surface);
  device.defer_deletion(global_deletion_stacks.instance);

  swapchain = Swapchain::init(options, device, surface, w);
  swapchain.defer_deletion(swapchain_deletion_stacks.device);

  renderpass = tr::renderer::Renderpass::init(device.vk_device, swapchain);
  renderpass.defer_deletion(swapchain_deletion_stacks.device);

  graphics_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Graphics);
  CommandPool::defer_deletion(graphics_command_pool, global_deletion_stacks.device);

  transfer_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Transfer);
  CommandPool::defer_deletion(transfer_command_pool, global_deletion_stacks.device);

  VmaAllocatorCreateInfo allocator_create_info{
      .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
      .physicalDevice = device.physical_device,
      .device = device.vk_device,
      .preferredLargeHeapBlockSize = 0,
      .pAllocationCallbacks = nullptr,
      .pDeviceMemoryCallbacks = nullptr,
      .pHeapSizeLimit = nullptr,
      .pVulkanFunctions = nullptr,
      .instance = instance.vk_instance,
      .vulkanApiVersion = VK_API_VERSION_1_3,
      .pTypeExternalMemoryHandleTypes = nullptr,
  };
  if (device.extensions.contains(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
    allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    spdlog::debug("VMA flag VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT is set");
  }

  VK_UNWRAP(vmaCreateAllocator, &allocator_create_info, &allocator);

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

  staging_buffer = tr::renderer::StagingBuffer::init(allocator, 4096);  // A triangle is 108 bytes (i think)
  staging_buffer.defer_deletion(global_deletion_stacks.allocator);

  OneTimeCommandBuffer transfer_cmd{transfer_command_buffer};
  VK_UNWRAP(transfer_cmd.begin);

  triangle_vertex_buffer = tr::renderer::TriangleVertexBuffer(device, allocator, transfer_cmd.vk_cmd, staging_buffer);
  triangle_vertex_buffer.defer_deletion(global_deletion_stacks.allocator);

  VK_UNWRAP(transfer_cmd.end);
  QueueSubmit{}.command_buffers({{transfer_cmd.vk_cmd}}).submit(device.queues.transfer_queue, VK_NULL_HANDLE);

  // TODO: add a fence | semaphore (a fence i think should be more suited) to prevent Synchronization issues

  pipeline = Pipeline::init(device, renderpass.vk_renderpass);
  pipeline.defer_deletion(global_deletion_stacks.device);

  debug_info.gpu_timestamps.reinit(device);
  debug_info.gpu_timestamps.defer_deletion(global_deletion_stacks.device);

  for (auto& synchro : frame_synchronisation_pool) {
    synchro = FrameSynchro::init(device.vk_device);
    synchro.defer_deletion(global_deletion_stacks.device);
  }
}

tr::renderer::VulkanEngine::~VulkanEngine() {
  sync();
  global_deletion_stacks.allocator.cleanup(allocator);
  vmaDestroyAllocator(allocator);
  vkFreeCommandBuffers(device.vk_device, graphics_command_pool,
                       utils::narrow_cast<uint32_t>(main_command_buffer_pool.size()), main_command_buffer_pool.data());
  swapchain_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.instance.cleanup(instance.vk_instance);
  vkDestroyInstance(instance.vk_instance, nullptr);
}
