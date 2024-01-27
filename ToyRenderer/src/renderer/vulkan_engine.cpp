#include "vulkan_engine.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <utils/cast.h>
#include <utils/misc.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <optional>
#include <span>
#include <vector>

#include "command_pool.h"
#include "constants.h"
#include "debug.h"
#include "deletion_stack.h"
#include "descriptors.h"
#include "queue.h"
#include "ressource_definition.h"
#include "ressources.h"
#include "swapchain.h"
#include "timeline_info.h"
#include "timestamp.h"
#include "uploader.h"
#include "utils.h"

void tr::renderer::VulkanEngine::rebuild_invalidated() {
  sync();
  {
    Lifetime cleanup;
    for (auto& image_storage : rm.image_storages()) {
      if (image_storage.invalidated) {
        image_storage.tie(cleanup);
      }
    }
    cleanup.cleanup(ctx.device.vk_device, allocator);
  }

  auto ib = image_builder();
  for (auto& image_storage : rm.image_storages()) {
    if (image_storage.invalidated) {
      image_storage.init(ib);
    }
  }
  rm.has_invalidated = true;
}

auto tr::renderer::VulkanEngine::start_frame() -> std::optional<Frame> {
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_TOP);
  if (swapchain_need_to_be_rebuilt) {
    rebuild_swapchain();
    swapchain_need_to_be_rebuilt = false;
  }

  if (rm.has_invalidated) {
    rebuild_invalidated();
  }

  frame_id += 1;

  // Try to get frame or bail early
  Frame frame{
      .swapchain_image_index = static_cast<uint32_t>(-1),
      .synchro = frame_synchronisation_pool[frame_id % MAX_FRAMES_IN_FLIGHT],
      .cmd = graphics_command_buffers[frame_id % MAX_FRAMES_IN_FLIGHT],
      .descriptor_allocator = frame_descriptor_allocators[frame_id % MAX_FRAMES_IN_FLIGHT],
      .frm = rm.frame(frame_id % MAX_FRAMES_IN_FLIGHT),
      .ctx = this,
  };

  VK_UNWRAP(vkWaitForFences, ctx.device.vk_device, 1, &frame.synchro.render_fence, VK_TRUE, 1000000000);
  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_WAIT_FENCE);

  VkResult const result =
      vkAcquireNextImageKHR(ctx.device.vk_device, ctx.swapchain.vk_swapchain, 1000000000,
                            frame.synchro.present_semaphore, VK_NULL_HANDLE, &frame.swapchain_image_index);
  switch (result) {
    case VK_ERROR_OUT_OF_DATE_KHR:
      rebuild_swapchain();
      return std::nullopt;
    case VK_SUBOPTIMAL_KHR:
    default:
      VK_CHECK(result, swapchain.acquire_next_frame);
  }

  VK_UNWRAP(vkResetFences, ctx.device.vk_device, 1, &frame.synchro.render_fence);
  VK_UNWRAP(vkResetCommandPool, ctx.device.vk_device, graphic_command_pools[frame_id % MAX_FRAMES_IN_FLIGHT], 0);
  VK_UNWRAP(frame.cmd.begin);
  frame.descriptor_allocator.reset(ctx.device.vk_device);

  vmaSetCurrentFrameIndex(allocator, frame_id);
  debug_info.set_frame_id(frame.cmd.vk_cmd, frame_id);

  // TODO: better from external images
  frame.frm.get_image(ImageRessourceId::Swapchain) = ImageRessource::from_external_image(
      ctx.swapchain.images[frame.swapchain_image_index], ctx.swapchain.image_views[frame.swapchain_image_index],
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, ctx.swapchain.extent);

  frame.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_ACQUIRE_FRAME_BOTTOM);

  if (!graphic_command_buffers_for_next_frame.empty()) {
    vkCmdExecuteCommands(frame.cmd.vk_cmd, utils::narrow_cast<uint32_t>(graphic_command_buffers_for_next_frame.size()),
                         graphic_command_buffers_for_next_frame.data());
    graphic_command_buffers_for_next_frame.clear();

    // TODO: free the command buffers we need to wait until they are not used anymore?
    // we could use a fence for this
    // std::queue<std::pair<VkFence, std::vector<VkCommandBuffer>>>
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
  VK_UNWRAP(frame.submitCmds, ctx.device.graphics_queue);

  // Present
  // TODO: there should be a queue ownership transfer if graphics queue != present queue
  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#multiple-queues
  const VkResult result = frame.present(ctx.device, ctx.swapchain.vk_swapchain);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapchain_need_to_be_rebuilt) {
    swapchain_need_to_be_rebuilt = true;
  } else {
    VK_CHECK(result, vkQueuePresentKHR);
  }

  // per frame cleanup
  lifetime.frame.device.cleanup(ctx.device.vk_device);
  lifetime.frame.allocator.cleanup(allocator);

  debug_info.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_PRESENT_BOTTOM);
}

void tr::renderer::VulkanEngine::rebuild_swapchain() {
  spdlog::info("rebuilding swapchain");

  sync();
  lifetime.swapchain.cleanup(ctx.device.vk_device, allocator);
  ctx.rebuild_swapchain(lifetime.swapchain, window);

  for (auto& image_storage : rm.image_storages()) {
    if (image_storage.definition.depends_on_swapchain()) {
      image_storage.invalidated = true;
    }
  }
  rm.has_invalidated = true;

  rm.get_image_storage(ImageRessourceId::Swapchain)
      .from_external_images(ctx.swapchain.images, ctx.swapchain.image_views, ctx.swapchain.extent);
}

void tr::renderer::VulkanEngine::init(tr::Options& options, std::span<const char*> required_instance_extensions,
                                      GLFWwindow* w) {
  if (options.debug.renderdoc) {
    debug_info.renderdoc = tr::renderer::Renderdoc::init();
  }

  window = w;
  ctx = VulkanContext::init(lifetime.swapchain, options, required_instance_extensions, w);

  VmaAllocatorCreateInfo allocator_create_info{
      .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
      .physicalDevice = ctx.physical_device.vk_physical_device,
      .device = ctx.device.vk_device,
      .preferredLargeHeapBlockSize = 0,
      .pAllocationCallbacks = nullptr,
      .pDeviceMemoryCallbacks = nullptr,
      .pHeapSizeLimit = nullptr,
      .pVulkanFunctions = nullptr,
      .instance = ctx.instance.vk_instance,
      .vulkanApiVersion = VK_API_VERSION_1_3,
      .pTypeExternalMemoryHandleTypes = nullptr,
  };
  if (ctx.physical_device.extensions.contains(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
    allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    spdlog::debug("VMA flag VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT is set");
  }

  VK_UNWRAP(vmaCreateAllocator, &allocator_create_info, &allocator);

  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    graphic_command_pools[i] =
        CommandPool::init(lifetime.global, ctx.device, ctx.physical_device, CommandPool::TargetQueue::Graphics);
    graphics_command_buffers[i] = OneTimeCommandBuffer::allocate(ctx.device.vk_device, graphic_command_pools[i]);
  }
  graphic_command_pool_for_next_frame =
      CommandPool::init(lifetime.global, ctx.device, ctx.physical_device, CommandPool::TargetQueue::Graphics);

  transfer_command_pool =
      CommandPool::init(lifetime.global, ctx.device, ctx.physical_device, CommandPool::TargetQueue::Transfer);

  for (auto& frame_descriptor_allocator : frame_descriptor_allocators) {
    frame_descriptor_allocator = DescriptorAllocator::init(lifetime.global, ctx.device.vk_device, 8192,
                                                           {{
                                                               {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048},
                                                               {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2048},
                                                               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2048},
                                                           }});
  }

  // Rm could take care of this
  for (const auto& def : image_definition) {
    rm.define_image(def);
  }
  for (const auto& def : buffer_definition) {
    rm.define_buffer(def);
  }

  auto ib = image_builder();
  for (auto& image_storage : rm.image_storages()) {
    if ((image_storage.definition.flags & IMAGE_OPTION_FLAG_EXTERNAL_BIT) != 0) {
      continue;
    }

    image_storage.init(ib);
  }

  auto bb = buffer_builder();
  for (auto& buffer_storage : rm.buffer_storages()) {
    buffer_storage.init(lifetime.global, bb);
  }

  debug_info.gpu_timestamps =
      decltype(debug_info.gpu_timestamps)::init(lifetime.global, ctx.device, ctx.physical_device);

  for (auto& synchro : frame_synchronisation_pool) {
    synchro = FrameSynchro::init(lifetime.global, ctx.device.vk_device);
  }
}

auto tr::renderer::VulkanEngine::start_transfer() -> Transferer {
  utils::ignore_unused(this);
  auto cmd = OneTimeCommandBuffer::allocate(ctx.device.vk_device, transfer_command_pool);
  auto graphics_cmd = OneTimeCommandBuffer::allocate(ctx.device.vk_device, graphic_command_pool_for_next_frame, false);

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

  QueueSubmit{}.command_buffers({{t.cmd.vk_cmd}}).submit(ctx.device.transfer_queue, VK_NULL_HANDLE);

  // WARN: THIS IS BAD: we should keep the uploader until all the uploads are done
  // Maybe within a deletion queue with a fence ? Or smthg like that
  sync();
  t.uploader.defer_trim(lifetime.frame.allocator);
}

tr::renderer::VulkanEngine::~VulkanEngine() {
  sync();

  for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkFreeCommandBuffers(ctx.device.vk_device, graphic_command_pools[i], 1, &graphics_command_buffers[i].vk_cmd);
  }

  for (const auto& image_storage : rm.image_storages()) {
    image_storage.tie(lifetime.global);
  }

  lifetime.swapchain.cleanup(ctx.device.vk_device, allocator);
  lifetime.global.cleanup(ctx.device.vk_device, allocator);

  vmaDestroyAllocator(allocator);

  InstanceDeletionStack instance_deletion_stack;
  ctx.device.defer_deletion(instance_deletion_stack);
  Surface::defer_deletion(ctx.surface, instance_deletion_stack);
  ctx.instance.defer_deletion(instance_deletion_stack);

  instance_deletion_stack.cleanup(ctx.instance.vk_instance);
  vkDestroyInstance(ctx.instance.vk_instance, nullptr);
}
