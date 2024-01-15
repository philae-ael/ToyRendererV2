#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../../camera.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "../ressources.h"
#include "utils/cast.h"

namespace tr::renderer {
struct DefaultRessources;

struct ShadowMap {
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  static constexpr uint32_t map_size = 4096;

  static constexpr std::array set_0 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_VERTEX_BIT)
          .build(),
  });

  static constexpr ImageRessourceDefinition attachment_depth{
      {
          .flags = 0,
          .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .size = VkExtent2D{map_size, map_size},
          .format = VK_FORMAT_D16_UNORM,
          .debug_name = "Shadow Map",
      },
      ImageRessourceId::ShadowMap,
  };

  static auto init(VkDevice &device, Swapchain &Swapchain, DeviceDeletionStack &device_deletion_stack) -> ShadowMap;

  static void register_ressources(RessourceManager &rm) {
    rm.define_image(attachment_depth);
    rm.define_buffer({
        .definition =
            {
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .size = utils::align(sizeof(CameraInfo), static_cast<size_t>(256)),
                .flags = BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT,
                .debug_name = "shadow camera uniforms",
            },
        .id = BufferRessourceId::ShadowCamera,
    });
  }

  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Pipeline, pipeline);
    device_deletion_stack.defer_deletion(DeviceHandle::PipelineLayout, pipeline_layout);
    for (auto descriptor_set_layout : descriptor_set_layouts) {
      device_deletion_stack.defer_deletion(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
    }
  }

  void start_draw(Frame &frame) const;
  void end_draw(VkCommandBuffer cmd) const;

  void draw(Frame &frame, const DirectionalLight &light, std::span<const Mesh> meshes);

  void draw_mesh(Frame &frame, const Mesh &mesh) const;
};

}  // namespace tr::renderer
