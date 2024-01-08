#include "vulkan_engine.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
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
  vmaSetCurrentFrameIndex(allocator, frame_id);

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
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.image = swapchain.images[frame.swapchain_image_index];
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vkCmdPipelineBarrier(frame.cmd.vk_cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

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
                                      GLFWwindow* w) {
  if (options.debug.renderdoc) {
    renderdoc = tr::renderer::Renderdoc::init();
  }

  window = w;
  instance = Instance::init(options, required_instance_extensions);
  instance.defer_deletion(global_deletion_stacks.instance);

  surface = Surface::init(instance.vk_instance, w);
  Surface::defer_deletion(surface, global_deletion_stacks.instance);

  device = Device::init(instance.vk_instance, surface);
  device.defer_deletion(global_deletion_stacks.instance);

  swapchain = Swapchain::init(options, device, surface, w);
  swapchain.defer_deletion(swapchain_device_deletion_stack);

  renderpass = tr::renderer::Renderpass::init(device.vk_device, swapchain);
  renderpass.defer_deletion(swapchain_device_deletion_stack);

  graphics_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Graphics);
  CommandPool::defer_deletion(graphics_command_pool, global_deletion_stacks.device);
  transfer_command_pool = CommandPool::init(device, CommandPool::TargetQueue::Transfer);
  CommandPool::defer_deletion(transfer_command_pool, global_deletion_stacks.device);

  VmaAllocatorCreateInfo allocator_create_info{
      .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
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
  global_deletion_stacks.allocator.cleanup(allocator);
  vmaDestroyAllocator(allocator);
  vkFreeCommandBuffers(device.vk_device, graphics_command_pool,
                       utils::narrow_cast<uint32_t>(main_command_buffer_pool.size()), main_command_buffer_pool.data());
  swapchain_device_deletion_stack.cleanup(device.vk_device);
  global_deletion_stacks.device.cleanup(device.vk_device);
  global_deletion_stacks.instance.cleanup(instance.vk_instance);
  vkDestroyInstance(instance.vk_instance, nullptr);
}
void tr::renderer::VulkanEngine::imgui() {
  if (!ImGui::Begin("Vulkan Engine")) {
    ImGui::End();
    return;
  }

  if (ImGui::CollapsingHeader("Timings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SeparatorText("GPU Timings:");
    for (std::size_t i = 0; i < GPU_TIME_PERIODS.size(); i++) {
      auto history = gpu_timelines[i].history();
      ImGui::PlotLines(GPU_TIME_PERIODS[i].name, history.data(), utils::narrow_cast<int>(history.size()));
    }

    ImGui::SeparatorText("CPU Timings:");
    for (std::size_t i = 0; i < CPU_TIME_PERIODS.size(); i++) {
      auto history = cpu_timelines[i].history();
      ImGui::PlotLines(CPU_TIME_PERIODS[i].name, history.data(), utils::narrow_cast<int>(history.size()));
    }
  }
  if (ImGui::CollapsingHeader("GPU memory usage", ImGuiTreeNodeFlags_DefaultOpen)) {
    auto history = gpu_memory_usage.history();
    ImGui::PlotLines("Global GPU memory usage", history.data(), utils::narrow_cast<int>(history.size()));

    if (ImGui::CollapsingHeader("details")) {
      std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
      vmaGetHeapBudgets(allocator, budgets.data());

      for (std::size_t i = 0; i < device.memory_properties.memoryHeapCount; i++) {
        auto history = gpu_heaps_usage[i].history();
        auto label = fmt::format("Heap number {}", i);
        ImGui::PlotLines(label.c_str(), history.data(), utils::narrow_cast<int>(history.size()));
      }
      if (ImGui::BeginTable("Memory usage", 7)) {
        ImGui::TableSetupColumn("Heap Number");
        ImGui::TableSetupColumn("Usage");
        ImGui::TableSetupColumn("Budget");
        ImGui::TableSetupColumn("Allocation Bytes");
        ImGui::TableSetupColumn("Allocation Count");
        ImGui::TableSetupColumn("Block Bytes");
        ImGui::TableSetupColumn("Block Count");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < device.memory_properties.memoryHeapCount; i++) {
          auto& budget = budgets[i];
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%zu", i);
          ImGui::TableNextColumn();
          ImGui::Text("%zu", budget.usage);
          ImGui::TableNextColumn();
          ImGui::Text("%zu", budget.budget);
          ImGui::TableNextColumn();
          ImGui::Text("%lu", budget.statistics.allocationBytes);
          ImGui::TableNextColumn();
          ImGui::Text("%u", budget.statistics.allocationCount);
          ImGui::TableNextColumn();
          ImGui::Text("%lu", budget.statistics.blockBytes);
          ImGui::TableNextColumn();
          ImGui::Text("%u", budget.statistics.blockCount);
        }
        ImGui::EndTable();
      }
    }
  }

  ImGui::End();
}

void tr::renderer::VulkanEngine::record_timeline() {
  if (gpu_timestamps.get(device.vk_device, frame_id - 1)) {
    for (std::size_t i = 0; i < GPU_TIME_PERIODS.size(); i++) {
      auto& period = GPU_TIME_PERIODS[i];
      auto dt = gpu_timestamps.fetch_elsapsed(frame_id - 1, period.from, period.to);
      if (!dt) {
        continue;
      }
      avg_gpu_timelines[i].update(*dt);
      gpu_timelines[i].push(avg_gpu_timelines[i].state);

      spdlog::trace("GPU Took {:.3f}us (smoothed {:3f}us) for period {}", 1000. * *dt,
                    1000. * avg_gpu_timelines[i].state, period.name);
    }
  }

  for (std::size_t i = 0; i < CPU_TIME_PERIODS.size(); i++) {
    auto& period = CPU_TIME_PERIODS[i];
    float dt =
        std::chrono::duration<float, std::milli>(cpu_timestamps[period.to] - cpu_timestamps[period.from]).count();
    avg_cpu_timelines[i].update(dt);
    cpu_timelines[i].push(avg_cpu_timelines[i].state);

    spdlog::trace("CPU Took {:.3f}us (smoothed {:3f}us) for period {}", 1000. * dt, 1000. * avg_cpu_timelines[i].state,
                  period.name);
  }

  std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
  vmaGetHeapBudgets(allocator, budgets.data());
  float global_memory_usage{};
  for (std::size_t i = 0; i < device.memory_properties.memoryHeapCount; i++) {
    gpu_heaps_usage[i].push(utils::narrow_cast<float>(budgets[i].usage));
    global_memory_usage += utils::narrow_cast<float>(budgets[i].usage);
  }
  gpu_memory_usage.push(global_memory_usage);
}
