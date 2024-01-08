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
#include "timestamp.h"
#include "uploader.h"
#include "utils.h"
#include "vertex.h"

const std::array triangle = std::to_array<tr::renderer::Vertex>({
    {
        .pos = {-1, -1, 0.},
        .base_color = {1, 0, 0},
        .uv = {0, 0},
    },
    {
        .pos = {1, -1, 0.},
        .base_color = {0, 0, 1},
        .uv = {0, 0},
    },
    {
        .pos = {0, 1, 0.},
        .base_color = {0, 1, 0},
        .uv = {0, 0},
    },
});

const std::array screen_space_triangle = std::to_array<tr::renderer::Vertex>({
    {
        .pos = {-1, 3, 0.},
        .base_color = {1, 0, 0},
        .uv = {0, 0},
    },
    {
        .pos = {-1, -1, 0.},
        .base_color = {0, 0, 1},
        .uv = {0, 0},
    },
    {
        .pos = {3, -1, 0.},
        .base_color = {0, 1, 0},
        .uv = {0, 0},
    },
});

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

  frame.cmd = graphics_command_buffers[frame_id % MAX_FRAMES_IN_FLIGHT];
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
    vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &triangle_vertex_buffer.buffer, &offset);
    vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0);
  });

  debug_info.write_gpu_timestamp(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 GPU_TIMESTAMP_INDEX_GBUFFER_BOTTOM);

  passes.deferred.draw(frame.cmd.vk_cmd, rm, {{0, 0}, swapchain.extent},
                       [&] { vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0); });

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

void tr::renderer::VulkanEngine::build_ressources() {
  ImageBuilder rb{device.vk_device, allocator, &swapchain};

  fb0_ressources.init(rb);
  fb1_ressources.init(rb);
  depth_ressources.init(rb);

  fb0_ressources.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
  fb1_ressources.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
  depth_ressources.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
}

void tr::renderer::VulkanEngine::rebuild_swapchain() {
  spdlog::info("rebuilding swapchain");
  sync();
  swapchain_deletion_stacks.device.cleanup(device.vk_device);
  swapchain_deletion_stacks.allocator.cleanup(allocator);

  swapchain.reinit(device, surface, window);
  swapchain.defer_deletion(swapchain_deletion_stacks.device);

  // There is only swapchain-specific ressource for now. If there is non swapchain-specific ressources they should not
  // be rebuilt every swapchain rebuild (or they can be, who cares)
  build_ressources();
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

  DeviceDeletionStack setup_device_deletion_stack;
  VmaDeletionStack setup_allocator_deletion_stack;

  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    graphic_command_pools[i] = CommandPool::init(device, CommandPool::TargetQueue::Graphics);
    CommandPool::defer_deletion(graphic_command_pools[i], global_deletion_stacks.device);
    graphics_command_buffers[i] = OneTimeCommandBuffer::allocate(device.vk_device, graphic_command_pools[i]);
  }

  BufferBuilder bb{device.vk_device, allocator};
  triangle_vertex_buffer = bb.build_buffer(
      {
          .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          .size = triangle.size() * sizeof(decltype(triangle)::value_type),
      },
      "Triangle");
  triangle_vertex_buffer.defer_deletion(global_deletion_stacks.allocator);

  upload(setup_device_deletion_stack, setup_allocator_deletion_stack);

  swapchain = Swapchain::init_with_config({options.config.prefered_present_mode}, device, surface, window);
  swapchain.defer_deletion(swapchain_deletion_stacks.device);
  build_ressources();

  // Passes pipelines should be rebuilt everytime there the swapchain changes format
  // It should not happen but it could happen, who knows when?

  passes.gbuffer = GBuffer::init(device.vk_device, swapchain, setup_device_deletion_stack);
  passes.gbuffer.defer_deletion(global_deletion_stacks.device);

  passes.deferred = Deferred::init(device.vk_device, swapchain, setup_device_deletion_stack);
  passes.deferred.defer_deletion(global_deletion_stacks.device);

  debug_info.gpu_timestamps = decltype(debug_info.gpu_timestamps)::init(device);
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

  auto cmd = OneTimeCommandBuffer::allocate(device.vk_device, transfer_command_pool);
  VK_UNWRAP(cmd.begin);

  auto uploader = Uploader::init(allocator);
  uploader.upload(cmd.vk_cmd, triangle_vertex_buffer.buffer, 0, std::as_bytes(std::span{screen_space_triangle}));

  VK_UNWRAP(cmd.end);

  QueueSubmit{}.command_buffers({{cmd.vk_cmd}}).submit(device.queues.transfer_queue, VK_NULL_HANDLE);
  uploader.defer_trim(allocator_deletion_stack);

  sync();
}

tr::renderer::VulkanEngine::~VulkanEngine() {
  sync();
  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkFreeCommandBuffers(device.vk_device, graphic_command_pools[i], 1, &graphics_command_buffers[i].vk_cmd);
  }
  swapchain_deletion_stacks.device.cleanup(device.vk_device);
  swapchain_deletion_stacks.allocator.cleanup(allocator);
  global_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.allocator.cleanup(allocator);
  vmaDestroyAllocator(allocator);
  global_deletion_stacks.instance.cleanup(instance.vk_instance);
  vkDestroyInstance(instance.vk_instance, nullptr);
}
