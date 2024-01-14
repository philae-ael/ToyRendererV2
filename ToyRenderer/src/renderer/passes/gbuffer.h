#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "../ressources.h"
#include "utils/cast.h"

namespace tr::renderer {
struct DefaultRessources;

struct GBuffer {
  std::array<VkDescriptorSetLayout, 2> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  static constexpr std::array set_0 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_VERTEX_BIT)
          .build(),
  });

  static constexpr std::array set_1 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(3)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),
  });

  static constexpr std::array attachments_color = utils::to_array<ImageRessourceDefinition>({
      {
          {
              .flags = 0,
              .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
              .size = FramebufferExtent{},
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .debug_name = "GBuffer0 (RGB: color, A: roughness)",
          },
          ImageRessourceId::GBuffer0,
      },
      {
          // RGB: normal, A: metallic
          {
              .flags = 0,
              .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
              .size = FramebufferExtent{},
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .debug_name = "GBuffer1 (RGB: normal, A: metallic)",
          },
          ImageRessourceId::GBuffer1,
      },
      {
          // RGB: ViewDir
          {
              .flags = 0,
              .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
              .size = FramebufferExtent{},
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .debug_name = "GBuffer2 (RGB: viewDir)",
          },
          ImageRessourceId::GBuffer2,
      },
  });

  static constexpr ImageRessourceDefinition attachment_depth{
      {
          .flags = 0,
          .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          .size = FramebufferExtent{},
          .format = VK_FORMAT_D16_UNORM,
          .debug_name = "Depth",
      },
      ImageRessourceId::Depth,
  };

  static auto init(VkDevice &device, Swapchain &Swapchain, DeviceDeletionStack &device_deletion_stack) -> GBuffer;

  static void register_ressources(RessourceManager &rm) {
    for (const auto &attachment : attachments_color) {
      rm.define_image(attachment);
    }
    rm.define_image(attachment_depth);
  }

  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Pipeline, pipeline);
    device_deletion_stack.defer_deletion(DeviceHandle::PipelineLayout, pipeline_layout);
    for (auto descriptor_set_layout : descriptor_set_layouts) {
      device_deletion_stack.defer_deletion(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
    }
  }

  void start_draw(Frame &frame, VkRect2D render_area) const;
  void end_draw(VkCommandBuffer cmd) const;

  void draw(Frame &frame, VkRect2D render_area, std::span<const Mesh> meshes, DefaultRessources default_ressources);

  void draw_mesh(Frame &frame, const Mesh &mesh, const DefaultRessources &default_ressources);
};

}  // namespace tr::renderer
