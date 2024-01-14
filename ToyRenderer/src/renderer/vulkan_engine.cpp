#include "vulkan_engine.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <optional>
#include <span>
#include <vector>

#include "../camera.h"
#include "command_pool.h"
#include "constants.h"
#include "debug.h"
#include "deletion_stack.h"
#include "descriptors.h"
#include "passes/deferred.h"
#include "passes/gbuffer.h"
#include "queue.h"
#include "ressources.h"
#include "swapchain.h"
#include "synchronisation.h"
#include "timeline_info.h"
#include "timestamp.h"
#include "uploader.h"
#include "utils.h"
#include "utils/misc.h"

auto tr::renderer::VulkanEngine::start_frame() -> std::optional<Frame> {
  if (swapchain_need_to_be_rebuilt) {
    rebuild_swapchain();
    swapchain_need_to_be_rebuilt = false;
  }

  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_TOP);
  frame_id += 1;

  // Try to get frame or bail early
  Frame frame{};
  frame.ctx = this;
  frame.synchro = frame_synchronisation_pool[frame_id % MAX_FRAMES_IN_FLIGHT];
  VK_UNWRAP(vkWaitForFences, device.vk_device, 1, &frame.synchro.render_fence, VK_TRUE, 1000000000);
  switch (const VkResult result = swapchain.acquire_next_frame(device, &frame); result) {
    case VK_ERROR_OUT_OF_DATE_KHR:
      rebuild_swapchain();
      return std::nullopt;
    case VK_SUBOPTIMAL_KHR:
    default:
      VK_CHECK(result, swapchain.acquire_next_frame);
  }
  VK_UNWRAP(vkResetFences, device.vk_device, 1, &frame.synchro.render_fence);

  VK_UNWRAP(vkResetCommandPool, device.vk_device, graphic_command_pools[frame_id % MAX_FRAMES_IN_FLIGHT], 0);

  frame.descriptor_allocator = frame_descriptor_allocators[frame_id % MAX_FRAMES_IN_FLIGHT];
  frame.descriptor_allocator.reset(device.vk_device);
  frame.frm = rm.frame(frame_id % MAX_FRAMES_IN_FLIGHT);

  // TODO: better from external images
  frame.frm.get_image(ImageRessourceId::Swapchain) = ImageRessource::from_external_image(
      swapchain.images[frame.swapchain_image_index], swapchain.image_views[frame.swapchain_image_index],
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, swapchain.extent);

  frame.cmd = graphics_command_buffers[frame_id % MAX_FRAMES_IN_FLIGHT];
  VK_UNWRAP(frame.cmd.begin);

  vmaSetCurrentFrameIndex(allocator, frame_id);
  debug_info.set_frame_id(frame.cmd.vk_cmd, frame_id);

  frame.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_BOTTOM);

  if (!graphic_command_buffers_for_next_frame.empty()) {
    vkCmdExecuteCommands(frame.cmd.vk_cmd, graphic_command_buffers_for_next_frame.size(),
                         graphic_command_buffers_for_next_frame.data());
    graphic_command_buffers_for_next_frame.clear();

    // TODO: free the command buffers
  }

  return frame;
}

// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
void tr::renderer::VulkanEngine::end_frame(Frame&& frame) {
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_TOP);

  ImageMemoryBarrier::submit<1>(frame.cmd.vk_cmd,
                                {{
                                    frame.frm.get_image(ImageRessourceId::Swapchain).prepare_barrier(SyncPresent),
                                }});

  VK_UNWRAP(frame.cmd.end);
  VK_UNWRAP(frame.submitCmds, device.queues.graphics_queue);

  // Present
  // TODO: there should be a queue ownership transfer if graphics queue != present queue
  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#multiple-queues
  const VkResult result = frame.present(device, swapchain.vk_swapchain);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapchain_need_to_be_rebuilt) {
    swapchain_need_to_be_rebuilt = true;
  } else {
    VK_CHECK(result, vkQueuePresentKHR);
  }

  // per frame cleanup
  frame_deletion_stacks.device.cleanup(device.vk_device);
  frame_deletion_stacks.allocator.cleanup(allocator);

  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_BOTTOM);
}

void tr::renderer::VulkanEngine::rebuild_swapchain() {
  spdlog::info("rebuilding swapchain");
  sync();
  swapchain_deletion_stacks.device.cleanup(device.vk_device);
  swapchain_deletion_stacks.allocator.cleanup(allocator);

  swapchain.reinit(device, surface, window);
  swapchain.defer_deletion(swapchain_deletion_stacks.device);

  rm.get_image_storage(ImageRessourceId::Swapchain)
      .from_external_images(swapchain.images, swapchain.image_views, swapchain.extent);

  ImageBuilder ib{device.vk_device, allocator, &swapchain};
  for (auto& image_storage : rm.image_storages()) {
    if (image_storage.definition.depends_on_swapchain()) {
      image_storage.init(ib);
      image_storage.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
    }
  }
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
  graphic_command_pool_for_next_frame = CommandPool::init(device, CommandPool::TargetQueue::Graphics);
  CommandPool::defer_deletion(graphic_command_pool_for_next_frame, global_deletion_stacks.device);

  transfer_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Transfer);
  CommandPool::defer_deletion(transfer_command_pool, global_deletion_stacks.device);

  swapchain = Swapchain::init_with_config({options.config.prefered_present_mode}, device, surface, window);
  swapchain.defer_deletion(swapchain_deletion_stacks.device);

  for (auto& frame_descriptor_allocator : frame_descriptor_allocators) {
    frame_descriptor_allocator = DescriptorAllocator::init(device.vk_device, 8192,
                                                           {{
                                                               {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048},
                                                               {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2048},
                                                               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2048},
                                                           }});
    frame_descriptor_allocator.defer_deletion(global_deletion_stacks.device);
  }

  // Passes pipelines should be rebuilt everytime there the swapchain changes format
  // It should not happen but it could happen, who knows when?

  tr::renderer::GBuffer::register_ressources(rm);
  tr::renderer::Deferred::register_ressources(rm);
  rm.define_buffer({
      .definition =
          {
              .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              .size = utils::align(sizeof(CameraInfo), static_cast<size_t>(256)),
              .flags = BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT,
              .debug_name = "camera uniform",
          },
      .id = BufferRessourceId::Camera,
  });

  auto ib = image_builder();
  for (auto& image_storage : rm.image_storages()) {
    if ((image_storage.definition.flags & IMAGE_OPTION_FLAG_EXTERNAL_BIT) != 0) {
      continue;
    }

    image_storage.init(ib);
    if (image_storage.definition.depends_on_swapchain()) {
      image_storage.defer_deletion(swapchain_deletion_stacks.allocator, swapchain_deletion_stacks.device);
    } else {
      image_storage.defer_deletion(global_deletion_stacks.allocator, global_deletion_stacks.device);
    }
  }

  auto bb = buffer_builder();
  for (auto& buffer_storage : rm.buffer_storages()) {
    buffer_storage.init(bb);
    buffer_storage.defer_deletion(global_deletion_stacks.allocator);
  }

  debug_info.gpu_timestamps = decltype(debug_info.gpu_timestamps)::init(device);
  debug_info.gpu_timestamps.defer_deletion(global_deletion_stacks.device);

  for (auto& synchro : frame_synchronisation_pool) {
    synchro = FrameSynchro::init(device.vk_device);
    synchro.defer_deletion(global_deletion_stacks.device);
  }

  setup_device_deletion_stack.cleanup(device.vk_device);
  setup_allocator_deletion_stack.cleanup(allocator);
}

auto tr::renderer::VulkanEngine::start_transfer() -> Transferer {
  utils::ignore_unused(this);
  auto cmd = OneTimeCommandBuffer::allocate(device.vk_device, transfer_command_pool);
  auto graphics_cmd = OneTimeCommandBuffer::allocate(device.vk_device, graphic_command_pool_for_next_frame, false);

  VK_UNWRAP(cmd.begin);
  VK_UNWRAP(graphics_cmd.begin);
  return {
      cmd,
      graphics_cmd,
      Uploader::init(allocator),
  };
}
void tr::renderer::VulkanEngine::end_transfer(Transferer&& t_in) {
  Transferer t{std::move(t_in)};
  VK_UNWRAP(t.cmd.end);
  VK_UNWRAP(t.graphics_cmd.end);
  graphic_command_buffers_for_next_frame.push_back(t.graphics_cmd.vk_cmd);

  QueueSubmit{}.command_buffers({{t.cmd.vk_cmd}}).submit(device.queues.transfer_queue, VK_NULL_HANDLE);

  // WARN: THIS IS BAD: we should keep the uploader until all the uploads are done
  // Maybe within a deletion queue with a fence ? Or smthg like that
  sync();
  t.uploader.defer_trim(frame_deletion_stacks.allocator);
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
