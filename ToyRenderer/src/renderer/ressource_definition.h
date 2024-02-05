#pragma once

#include <utils/cast.h>
#include <vulkan/vulkan_core.h>

#include "../camera.h"
#include "../registry.h"
#include "mesh.h"
#include "ressource_manager.h"
#include "ressources.h"
#include "utils/misc.h"

namespace tr::renderer {

CVAR_FLOAT(internal_resolution_scale, 1.0)
CVAR_EXTENT2D(shadow_map_extent, 1024, 1024)

struct DefaultRessources {
  VkSampler sampler;
  ImageRessource metallic_roughness;
  image_ressource_handle metallic_roughness_handle;
  ImageRessource normal_map;
  image_ressource_handle normal_map_handle;
};

constexpr ImageRessourceDefinition SWAPCHAIN{
    .id = ImageRessourceId::Swapchain,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .size = {SwapchainExtent{}},
            .format = {SwapchainFormat{}},
            .debug_name = "swapchain",
        },
    .scope = RessourceScope::Extern,
};

constexpr ImageRessourceDefinition RENDERED{
    .id = ImageRessourceId::Rendered,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {InternalResolutionExtent{}},
            .format = {SwapchainFormat{}},
            .debug_name = "rendered",
        },
    .scope = RessourceScope::Transient,
};

constexpr ImageRessourceDefinition AO{
    .id = ImageRessourceId::AO,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {InternalResolutionExtent{}},
            .format = {StaticFormat{VK_FORMAT_R32G32B32A32_SFLOAT}},
            .debug_name = "rendered",
        },
    .scope = RessourceScope::Transient,
};

constexpr ImageRessourceDefinition GBUFFER_0{
    .id = ImageRessourceId::GBuffer0,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {InternalResolutionExtent{}},
            .format = {StaticFormat{VK_FORMAT_R32G32B32A32_SFLOAT}},
            .debug_name = "GBuffer0 (RGB: color, A: roughness)",
        },
    .scope = RessourceScope::Transient,
};

constexpr ImageRessourceDefinition GBUFFER_1{
    .id = ImageRessourceId::GBuffer1,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {InternalResolutionExtent{}},
            .format = {StaticFormat{VK_FORMAT_R32G32B32A32_SFLOAT}},
            .debug_name = "GBuffer1 (RGB: normal, A: metallic)",
        },
    .scope = RessourceScope::Transient,
};

constexpr ImageRessourceDefinition GBUFFER_2{
    .id = ImageRessourceId::GBuffer2,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {InternalResolutionExtent{}},
            .format = {StaticFormat{VK_FORMAT_R32G32B32A32_SFLOAT}},
            .debug_name = "GBuffer2 (RGB: viewDir)",
        },
    .scope = RessourceScope::Transient,
};

constexpr ImageRessourceDefinition GBUFFER_3{
    .id = ImageRessourceId::GBuffer3,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {InternalResolutionExtent{}},
            .format = {StaticFormat{VK_FORMAT_R32G32B32A32_SFLOAT}},
            .debug_name = "GBuffer3 (RGB: Position)",
        },
    .scope = RessourceScope::Transient,
};
constexpr ImageRessourceDefinition DEPTH{
    .id = ImageRessourceId::Depth,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {InternalResolutionExtent{}},
            .format = {StaticFormat{VK_FORMAT_D16_UNORM}},
            .debug_name = "Depth",
        },
    .scope = RessourceScope::Transient,
};

constexpr ImageRessourceDefinition SHADOW_MAP{
    .id = ImageRessourceId::ShadowMap,
    .definition =
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = {CVarExtent{shadow_map_extent}},
            .format = {StaticFormat{VK_FORMAT_D16_UNORM}},
            .debug_name = "Shadow Map",
        },
    .scope = RessourceScope::Transient,
};

static constexpr BufferRessourceDefinition CAMERA{
    .id = BufferRessourceId::Camera,
    .definition =
        {
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .size = utils::align<uint32_t>(utils::narrow_cast<uint32_t>(sizeof(CameraInfo)), 256),
            .flags = BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT,
            .debug_name = "camera uniform",
        },
    .scope = RessourceScope::Transient,
};
static constexpr BufferRessourceDefinition SHADOW_CAMERA{
    .id = BufferRessourceId::ShadowCamera,
    .definition =
        {
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .size = utils::align<uint32_t>(utils::narrow_cast<uint32_t>(sizeof(CameraInfo)), 256),
            .flags = BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT,
            .debug_name = "shadow camera uniforms",
        },
    .scope = RessourceScope::Transient,
};

static constexpr BufferRessourceDefinition DEBUG_VERTICES{
    .id = BufferRessourceId::DebugVertices,
    .definition =
        {
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .size = utils::align<uint32_t>(utils::narrow_cast<uint32_t>(sizeof(Vertex)) * 3 * 1024, 256),
            .flags = BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT | BUFFER_OPTION_FLAG_CREATE_MAPPED_BIT,
            .debug_name = "debug vertices",
        },
    .scope = RessourceScope::Transient,
};
}  // namespace tr::renderer
