#include "vulkan_engine.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <optional>
#include <span>

#include "command_pool.h"
#include "constants.h"
#include "debug.h"
#include "deletion_queue.h"
#include "passes/gbuffer.h"
#include "queue.h"
#include "ressources.h"
#include "swapchain.h"
#include "timeline_info.h"
#include "utils.h"
#include "vertex.h"

auto tr::renderer::VulkanEngine::start_frame() -> std::optional<Frame> {
  if (swapchain_need_to_be_rebuilt) {
    rebuild_swapchain();
    swapchain_need_to_be_rebuilt = false;
  }

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
    default:
      VK_CHECK(result, swapchain.acquire_next_frame);
  }
  VK_UNWRAP(vkResetFences, device.vk_device, 1, &frame.synchro.render_fence);

  VK_UNWRAP(vkResetCommandPool, device.vk_device, graphic_command_pools[frame_id % MAX_FRAMES_IN_FLIGHT], 0);
  frame.cmd = OneTimeCommandBuffer{graphics_command_buffers[frame_id % MAX_FRAMES_IN_FLIGHT]};
  VK_UNWRAP(frame.cmd.begin);

  // Update frame id for subsystems
  vmaSetCurrentFrameIndex(allocator, frame_id);
  debug_info.set_frame(frame, frame_id);
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_BOTTOM);

  // update ressources used this frame
  rm.swapchain =
      ImageRessource::from_external_image(swapchain.images[frame.swapchain_image_index],
                                          swapchain.image_views[frame.swapchain_image_index], ImageUsage::Color);
  rm.fb0 = fb0_ressources.get(frame_id);
  rm.fb1 = fb1_ressources.get(frame_id);
  rm.depth = depth_ressources.get(frame_id);

  return frame;
}

void tr::renderer::VulkanEngine::draw(Frame frame) {
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_TOP);
  debug_info.write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_TOP);

  passes.gbuffer.draw(frame.cmd.vk_cmd, rm, {{0, 0}, swapchain.extent}, [&] {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &triangle_vertex_buffer.vk_buffer, &offset);
    vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0);
  });

  debug_info.write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 GPU_TIMESTAMP_INDEX_GBUFFER_BOTTOM);

  passes.deferred.draw(frame.cmd.vk_cmd, rm, {{0, 0}, swapchain.extent}, [&] {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &triangle_vertex_buffer.vk_buffer, &offset);
    vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0);
  });

  debug_info.write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_BOTTOM);
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_BOTTOM);
}

void tr::renderer::VulkanEngine::end_frame(Frame frame) {
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_TOP);

  rm.swapchain.sync(frame.cmd.vk_cmd, SyncPresent);

  VK_UNWRAP(frame.cmd.end);
  VK_UNWRAP(frame.submitCmds, device.queues.graphics_queue);

  // Present
  // TODO: there should be a queue ownership transfer if graphics queue != present queue
  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#multiple-queues
  VkResult result = frame.present(device, swapchain.vk_swapchain);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapchain_need_to_be_rebuilt) {
    swapchain_need_to_be_rebuilt = true;
  } else {
    VK_CHECK(result, vkQueuePresentKHR);
  }

  // per frame cleanup
  frame_deletion_stacks.device.cleanup(device.vk_device);

  // Store back ressources state
  fb0_ressources.store(frame_id, rm.fb0);
  fb1_ressources.store(frame_id, rm.fb1);
  depth_ressources.store(frame_id, rm.depth);

  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_BOTTOM);
}

void tr::renderer::VulkanEngine::rebuild_swapchain() {
  spdlog::info("rebuilding swapchain");
  sync();
  swapchain_deletion_stacks.device.cleanup(device.vk_device);
  swapchain_deletion_stacks.allocator.cleanup(allocator);

  swapchain = tr::renderer::Swapchain::init_with_config(swapchain.config, device, surface, window);
  swapchain.defer_deletion(swapchain_deletion_stacks.device);

  ImageBuilder rb{device.vk_device, allocator, &swapchain};

  fb0_ressources.init(rb);
  fb1_ressources.init(rb);
  depth_ressources.init(rb);

  fb0_ressources.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
  fb1_ressources.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
  depth_ressources.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
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

  swapchain.config = {
      .prefered_present_mode = options.config.prefered_present_mode,
  };

  rebuild_swapchain();

  DeviceDeletionStack setup_device_deletion_stack;
  VmaDeletionStack setup_allocator_deletion_stack;

  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    graphic_command_pools[i] = CommandPool::init(device, CommandPool::TargetQueue::Graphics);
    CommandPool::defer_deletion(graphic_command_pools[i], global_deletion_stacks.device);

    VkCommandBufferAllocateInfo cmd_alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = graphic_command_pools[i],
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VK_UNWRAP(vkAllocateCommandBuffers, device.vk_device, &cmd_alloc_info, &graphics_command_buffers[i]);
  }

  upload(setup_device_deletion_stack, setup_allocator_deletion_stack);

  passes.gbuffer = GBuffer::init(device.vk_device, swapchain, setup_device_deletion_stack);
  passes.gbuffer.defer_deletion(global_deletion_stacks.device);

  passes.deferred = Deferred::init(device.vk_device, swapchain, setup_device_deletion_stack);
  passes.deferred.defer_deletion(global_deletion_stacks.device);

  debug_info.gpu_timestamps.reinit(device);
  debug_info.gpu_timestamps.defer_deletion(global_deletion_stacks.device);

  for (auto& synchro : frame_synchronisation_pool) {
    synchro = FrameSynchro::init(device.vk_device);
    synchro.defer_deletion(global_deletion_stacks.device);
  }

  setup_device_deletion_stack.cleanup(device.vk_device);
  setup_allocator_deletion_stack.cleanup(allocator);
}

void tr::renderer::VulkanEngine::upload(DeviceDeletionStack& device_deletion_stack,
                                        VmaDeletionStack& allocator_deletion_stack) {
  VkCommandPool transfer_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Transfer);
  CommandPool::defer_deletion(transfer_command_pool, device_deletion_stack);

  VkCommandBufferAllocateInfo transfer_cmd_alloc_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = transfer_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer transfer_command_buffer{};
  VK_UNWRAP(vkAllocateCommandBuffers, device.vk_device, &transfer_cmd_alloc_info, &transfer_command_buffer);

  // A triangle is 108 bytes (i think)
  StagingBuffer staging_buffer = tr::renderer::StagingBuffer::init(allocator, 4096);
  staging_buffer.defer_deletion(allocator_deletion_stack);

  OneTimeCommandBuffer transfer_cmd{transfer_command_buffer};
  VK_UNWRAP(transfer_cmd.begin);

  triangle_vertex_buffer = tr::renderer::TriangleVertexBuffer(device, allocator, transfer_cmd.vk_cmd, staging_buffer);
  triangle_vertex_buffer.defer_deletion(global_deletion_stacks.allocator);
  VK_UNWRAP(transfer_cmd.end);

  VkFence transfer_fence = VK_NULL_HANDLE;
  const VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VK_UNWRAP(vkCreateFence, device.vk_device, &fence_create_info, nullptr, &transfer_fence);
  device_deletion_stack.defer_deletion(DeviceHandle::Fence, transfer_fence);

  QueueSubmit{}.command_buffers({{transfer_cmd.vk_cmd}}).submit(device.queues.transfer_queue, transfer_fence);

  VK_UNWRAP(vkWaitForFences, device.vk_device, 1, &transfer_fence, 1, 1000000000);
}

tr::renderer::VulkanEngine::~VulkanEngine() {
  sync();
  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkFreeCommandBuffers(device.vk_device, graphic_command_pools[i], 1, &graphics_command_buffers[i]);
  }
  swapchain_deletion_stacks.device.cleanup(device.vk_device);
  swapchain_deletion_stacks.allocator.cleanup(allocator);
  global_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.allocator.cleanup(allocator);
  vmaDestroyAllocator(allocator);
  global_deletion_stacks.instance.cleanup(instance.vk_instance);
  vkDestroyInstance(instance.vk_instance, nullptr);
}
