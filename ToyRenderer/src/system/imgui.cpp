
#include "imgui.h"
void tr::system::Imgui::init(GLFWwindow *window, renderer::VulkanEngine &engine) {
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  std::array<VkDescriptorPoolSize, 11> pool_sizes = {{
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
  }};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();

  VkDescriptorPool imgui_pool{};
  VK_UNWRAP(vkCreateDescriptorPool, engine.ctx.device.vk_device, &pool_info, nullptr, &imgui_pool);

  ImGui_ImplGlfw_InitForVulkan(window, true);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = engine.ctx.instance.vk_instance;
  init_info.PhysicalDevice = engine.ctx.device.physical_device;
  init_info.Device = engine.ctx.device.vk_device;
  init_info.Queue = engine.ctx.device.queues.graphics_queue;
  init_info.DescriptorPool = imgui_pool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;
  init_info.ColorAttachmentFormat = engine.ctx.swapchain.surface_format.format;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

  ImGui_ImplVulkan_CreateFontsTexture();

  engine.lifetime.global.tie(renderer::DeviceHandle::DescriptorPool, imgui_pool);
  valid = true;
}

auto tr::system::Imgui::start_frame(renderer::Frame &frame) const -> bool {
  frame.write_cpu_timestamp(renderer::CPU_TIMESTAMP_INDEX_IMGUI_TOP);
  if (!valid) {
    return false;
  }
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  return true;
}

void tr::system::Imgui::draw(renderer::Frame &frame) const {
  if (!valid) {
    return;
  }
  ImGui::Render();

  renderer::DebugCmdScope const scope{frame.cmd.vk_cmd, "Imgui"};
  frame.write_gpu_timestamp(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderer::GPU_TIMESTAMP_INDEX_IMGUI_TOP);

  const auto &swapchain_ressource = frame.frm.get_image(renderer::ImageRessourceId::Swapchain);
  VkRenderingAttachmentInfo const colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = swapchain_ressource.view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .resolveMode = VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {},
  };
  VkRenderingInfo const render_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderArea = {{0, 0}, swapchain_ressource.extent},
      .layerCount = 1,
      .viewMask = 0,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
      .pDepthAttachment = nullptr,
      .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(frame.cmd.vk_cmd, &render_info);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.cmd.vk_cmd);
  vkCmdEndRendering(frame.cmd.vk_cmd);

  if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }

  frame.write_gpu_timestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderer::GPU_TIMESTAMP_INDEX_IMGUI_BOTTOM);
  frame.write_cpu_timestamp(renderer::CPU_TIMESTAMP_INDEX_IMGUI_BOTTOM);
}

tr::system::Imgui::~Imgui() {
  if (valid) {
    ImGui_ImplVulkan_Shutdown();
  }
}

auto tr::system::Imgui::operator=(Imgui &&other) noexcept -> Imgui & {
  valid = other.valid;
  other.valid = false;
  return *this;
};
