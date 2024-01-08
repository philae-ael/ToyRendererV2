#include "debug.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/misc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <format>
#include <fstream>

#include "swapchain.h"
#include "utils/cast.h"
#include "vulkan_engine.h"

#if defined(_WIN32)
#include <wtypes.h>
#else
#include <dlfcn.h>
#endif

auto tr::renderer::Renderdoc::init() -> Renderdoc {
  Renderdoc renderdoc{};

  // Recommended usage is to passively load renderdoc dll (thus using GetModuleHandleA/dlopen RTLD_NOW | RTLD_NOLOAD)
  // but loading renderdoc is used here to get an FPS counter and to allow easy debugging while being able to record
  // renderdoc captures
#if defined(_WIN32)
  HMODULE mod = LoadLibraryA("renderdoc.dll");
  if (mod == nullptr) {
    mod = LoadLibraryA("C:/Program Files/RenderDoc/renderdoc.dll");
  }
#else
  void* mod = dlopen("librenderdoc.so", RTLD_NOW);
#endif

  if (mod != nullptr) {
#if defined(_WIN32)
    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
#else
    auto RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
#endif
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, reinterpret_cast<void**>(&renderdoc.rdoc_api));

    TR_ASSERT(ret == 1 && renderdoc.rdoc_api != nullptr, "Could not load renderdoc");
    spdlog::info("Renderdoc loaded!");
  } else {
    spdlog::info("Can't find renderdoc");
  }

  return renderdoc;
}

void tr::renderer::VulkanEngineDebugInfo::set_frame(tr::renderer::Frame frame, std::size_t frame_id) {
  current_frame = frame;
  current_frame_id = frame_id;
  gpu_timestamps.reset_queries(frame.cmd.vk_cmd, current_frame_id);
}

void tr::renderer::VulkanEngineDebugInfo::write_cpu_timestamp(tr::renderer::CPUTimestampIndex index) {
  cpu_timestamps[index] = cpu_timestamp_clock::now();
}

void tr::renderer::VulkanEngineDebugInfo::write_gpu_timestamp(VkCommandBuffer cmd,
                                                              VkPipelineStageFlagBits pipelineStage,
                                                              GPUTimestampIndex index) {
  gpu_timestamps.write_cmd_query(cmd, pipelineStage, current_frame_id, index);
}

void tr::renderer::VulkanEngineDebugInfo::stat_window(tr::renderer::VulkanEngine& engine) {
  if (!ImGui::Begin("Vulkan Engine")) {
    ImGui::End();
    return;
  }

  if (ImGui::CollapsingHeader("Timings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SeparatorText("GPU Timings:");
    if (ImGui::BeginTable("GPU Timings:", 2, ImGuiTableFlags_SizingStretchProp)) {
      for (std::size_t i = 0; i < GPU_TIME_PERIODS.size(); i++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        auto history = gpu_timelines[i].history();
        ImGui::PlotLines(GPU_TIME_PERIODS[i].name, history.data(), utils::narrow_cast<int>(history.size()));

        ImGui::TableNextColumn();
        auto text = std::format("{:7.1f}us", 1000.F * avg_gpu_timelines[i].state);
        ImGui::Text("%s", text.c_str());
      }
      ImGui::EndTable();
    }

    ImGui::SeparatorText("CPU Timings:");
    if (ImGui::BeginTable("CPU Timings:", 2, ImGuiTableFlags_SizingStretchProp)) {
      for (std::size_t i = 0; i < CPU_TIME_PERIODS.size(); i++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        auto history = cpu_timelines[i].history();
        ImGui::PlotLines(CPU_TIME_PERIODS[i].name, history.data(), utils::narrow_cast<int>(history.size()));

        ImGui::TableNextColumn();
        auto text = std::format("{:7.1f}us", 1000.F * avg_cpu_timelines[i].state);
        ImGui::Text("%s", text.c_str());
      }
      ImGui::EndTable();
    }
  }
  if (ImGui::CollapsingHeader("GPU memory usage", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("Memory usage", 2, ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();

      {
        auto history = gpu_memory_usage.history();
        ImGui::PlotLines("Global GPU memory usage", history.data(), utils::narrow_cast<int>(history.size()));

        ImGui::TableNextColumn();
        ImGui::Text("%.1f MB", history[history.size() - 1] / 1024 / 1024);
      }

      for (std::size_t i = 0; i < engine.device.memory_properties.memoryHeapCount; i++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const auto history = gpu_heaps_usage[i].history();
        const auto label = std::format("Heap number {}", i);
        ImGui::PlotLines(label.c_str(), history.data(), utils::narrow_cast<int>(history.size()));

        ImGui::TableNextColumn();
        ImGui::Text("%.1f MB", history[history.size() - 1] / 1024 / 1024);
      }
      ImGui::EndTable();
    }

    if (ImGui::BeginTable("details", 7, ImGuiTableFlags_SizingStretchProp)) {
      std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
      vmaGetHeapBudgets(engine.allocator, budgets.data());

      ImGui::TableSetupColumn("Heap Number");
      ImGui::TableSetupColumn("Usage");
      ImGui::TableSetupColumn("Budget");
      ImGui::TableSetupColumn("Allocation Bytes");
      ImGui::TableSetupColumn("Allocation Count");
      ImGui::TableSetupColumn("Block Bytes");
      ImGui::TableSetupColumn("Block Count");
      ImGui::TableHeadersRow();
      for (std::size_t i = 0; i < engine.device.memory_properties.memoryHeapCount; i++) {
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
    if (ImGui::Button("Dump allocation map as json")) {
      char* stats_string = nullptr;
      vmaBuildStatsString(engine.allocator, &stats_string, VK_TRUE);
      {
        std::ofstream f("gpu_memory_dump.json");
        if (!f.is_open()) {
          spdlog::error("Can't open file to dump gpu memory map");
        } else {
          f << stats_string;
        }
      }

      vmaFreeStatsString(engine.allocator, stats_string);
    }
  }

  ImGui::End();
}

void tr::renderer::VulkanEngineDebugInfo::option_window(tr::renderer::VulkanEngine& engine) {
  if (!ImGui::Begin("Options")) {
    ImGui::End();
    return;
  }

  std::array present_modes = utils::to_array<std::pair<const char*, VkPresentModeKHR> >({
      {"Immediate", VK_PRESENT_MODE_IMMEDIATE_KHR},
      {"MailBox", VK_PRESENT_MODE_MAILBOX_KHR},
      {"FIFO", VK_PRESENT_MODE_FIFO_KHR},
      {"FIFO Relaxed", VK_PRESENT_MODE_FIFO_RELAXED_KHR},
  });
  int present_mode_selected = -1;
  for (int i = 0; i < utils::narrow_cast<int>(present_modes.size()); i++) {
    if (engine.swapchain.config.prefered_present_mode == present_modes[i].second) {
      present_mode_selected = i;
      break;
    }
  }

  if (present_mode_selected >= 0 && ImGui::BeginCombo("Present mode", present_modes[present_mode_selected].first)) {
    for (int i = 0; i < utils::narrow_cast<int>(present_modes.size()); i++) {
      if (ImGui::Selectable(present_modes[i].first, i == present_mode_selected)) {
        present_mode_selected = i;
        engine.swapchain.config.prefered_present_mode = present_modes[i].second;
        engine.swapchain_need_to_be_rebuilt = true;
      }
    }
    ImGui::EndCombo();
  }

  ImGui::End();
}
void tr::renderer::VulkanEngineDebugInfo::imgui(tr::renderer::VulkanEngine& engine) {
  stat_window(engine);
  option_window(engine);
}

void tr::renderer::VulkanEngineDebugInfo::record_timeline(tr::renderer::VulkanEngine& engine) {
  {
    gpu_timestamps.get(engine.device.vk_device, current_frame_id - 1);
    for (std::size_t i = 0; i < GPU_TIME_PERIODS.size(); i++) {
      auto& period = GPU_TIME_PERIODS[i];
      auto dt = gpu_timestamps.fetch_elsapsed(current_frame_id - 1, period.from, period.to);

      if (dt) {
        avg_gpu_timelines[i].update(*dt);
      }
      gpu_timelines[i].push(avg_gpu_timelines[i].state);

      spdlog::trace("GPU Took {:3f}us for period {}", 1000. * avg_gpu_timelines[i].state, period.name);
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
  vmaGetHeapBudgets(engine.allocator, budgets.data());
  float global_memory_usage{};
  for (std::size_t i = 0; i < engine.device.memory_properties.memoryHeapCount; i++) {
    gpu_heaps_usage[i].push(utils::narrow_cast<float>(budgets[i].usage));
    global_memory_usage += utils::narrow_cast<float>(budgets[i].usage);
  }
  gpu_memory_usage.push(global_memory_usage);
}
