#include "gltf.h"

#include <spdlog/spdlog.h>

#include <optional>
#include <string_view>

#include "renderer/synchronisation.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "renderer/mesh.h"
#include "renderer/ressources.h"
#include "renderer/uploader.h"
#include "renderer/vertex.h"
#include "utils/assert.h"
#include "utils/cast.h"

template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <class T>
concept has_bytes = requires(T a) { std::span(a.bytes); };

auto load_materials(tr::renderer::ImageBuilder& ib, tr::renderer::Transferer& t, const fastgltf::Asset& asset)
    -> std::vector<std::shared_ptr<tr::renderer::Material>> {
  std::vector<std::shared_ptr<tr::renderer::Material>> materials;

  for (const auto& material : asset.materials) {
    TR_ASSERT(material.pbrData.baseColorTexture, "no base color texture, not supported");
    auto mat = std::make_shared<tr::renderer::Material>();
    const auto& color_texture = asset.textures[material.pbrData.baseColorTexture->textureIndex];

    TR_ASSERT(color_texture.imageIndex, "no image index, not supported");
    const auto& color_image = asset.images[*color_texture.imageIndex];

    uint32_t width = 0;
    uint32_t height = 0;
    std::span<const std::byte> image;

    std::visit(overloaded{
                   [&](const has_bytes auto& c) {
                     const auto bytes = std::as_bytes(std::span(c.bytes));

                     int x = 0;
                     int y = 0;
                     int channels = 0;
                     const auto* im =
                         stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                                               utils::narrow_cast<int>(bytes.size_bytes()), &x, &y, &channels, 4);
                     TR_ASSERT(im != nullptr, "Could not load image");

                     width = x;
                     height = y;
                     image = std::span{reinterpret_cast<const std::byte*>(im), static_cast<size_t>(x * y * 4)};
                   },
                   [](const auto&) { TR_ASSERT(false, "MEH"); },
               },
               color_image.data);

    mat->base_color_texture = ib.build_image(
        tr::renderer::ImageDefinition{
            .flags = tr::renderer::IMAGE_OPTION_FLAG_FORMAT_R8G8B8A8_UNORM_BIT |
                     tr::renderer::IMAGE_OPTION_FLAG_SIZE_CUSTOM_BIT,
            .usage = tr::renderer::IMAGE_USAGE_SAMPLED_BIT | tr::renderer::IMAGE_USAGE_TRANSFER_DST_BIT,
            .size =
                VkExtent2D{
                    .width = width,
                    .height = height,

                },
        },
        "texture image");

    tr::renderer::ImageMemoryBarrier::submit<1>(
        t.cmd.vk_cmd, {{
                          mat->base_color_texture.prepare_barrier(tr::renderer::SyncImageTransfer),
                      }});
    t.upload_image(mat->base_color_texture, {{0, 0}, {width, height}}, image);

    materials.push_back(std::move(mat));
  }
  return materials;
}

auto load_attribute(const fastgltf::Asset& asset, std::vector<tr::renderer::Vertex>& vertices,
                    const std::string& attribute, const fastgltf::Accessor& accessor, size_t start_v_idx) {
  vertices.resize(std::max(vertices.size(), start_v_idx + accessor.count));

  if (attribute == "POSITION") {
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        asset, accessor, [&](glm::vec3 pos, size_t v_idx) { vertices[start_v_idx + v_idx].pos = pos; });
    return;
  }
  if (attribute == "NORMAL") {
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        asset, accessor, [&](glm::vec3 normal, size_t v_idx) { vertices[start_v_idx + v_idx].normal = normal; });
    return;
  }
  if (attribute == "TEXCOORD_0") {
    fastgltf::iterateAccessorWithIndex<glm::vec2>(
        asset, accessor, [&](glm::vec2 uv, size_t v_idx) { vertices[start_v_idx + v_idx].uv = uv; });
    return;
  }
  if (attribute == "COLOR_0") {
    fastgltf::iterateAccessorWithIndex<glm::vec4>(
        asset, accessor, [&](glm::vec4 color, size_t v_idx) { vertices[start_v_idx + v_idx].color = color; });
    return;
  }

  static std::unordered_map<std::string, std::once_flag> warn_once_flags;
  std::call_once(warn_once_flags[attribute], [&] { spdlog::warn("Unknown attribute {}", attribute); });
}

auto load_meshes(tr::renderer::BufferBuilder& bb, tr::renderer::Transferer& t, const fastgltf::Asset& asset,
                 std::span<std::shared_ptr<tr::renderer::Material>> materials) -> std::vector<tr::renderer::Mesh> {
  std::vector<tr::renderer::Mesh> meshes;
  for (const auto& scene : asset.scenes) {
    for (auto idx : scene.nodeIndices) {
      const auto& node = asset.nodes[idx];

      tr::renderer::Mesh asset_mesh;
      std::visit(
          overloaded{
              [&](const fastgltf::Node::TRS& trs) {
                const auto rot = glm::mat4_cast(glm::make_quat(trs.rotation.data()));
                const auto scale = glm::scale(glm::identity<glm::mat4>(), glm::make_vec3(trs.scale.data()));

                asset_mesh.transform = glm::translate(rot * scale, glm::make_vec3(trs.translation.data()));
              },
              [&](const fastgltf::Node::TransformMatrix& trs) { asset_mesh.transform = glm::make_mat4(trs.data()); },
          },
          node.transform);

      TR_ASSERT(node.meshIndex, "Child not supported");
      const auto& mesh = asset.meshes[*node.meshIndex];

      std::vector<uint32_t> indices{};
      std::vector<tr::renderer::Vertex> vertices;
      for (const auto& primitive : mesh.primitives) {
        const auto start_v_idx = vertices.size();

        {
          TR_ASSERT(primitive.indicesAccessor, "index buffer");
          const auto& accessor = asset.accessors[*primitive.indicesAccessor];
          const auto start_i_idx = indices.size();

          TR_ASSERT(primitive.materialIndex, "material needed");
          asset_mesh.surfaces.push_back({
              .start = utils::narrow_cast<uint32_t>(start_i_idx),
              .count = utils::narrow_cast<uint32_t>(accessor.count),
              .material = materials[*primitive.materialIndex],
          });

          indices.reserve(indices.size() + accessor.count);
          fastgltf::iterateAccessor<uint32_t>(asset, accessor, [&](uint32_t idx) {
            indices.push_back(utils::narrow_cast<uint32_t>(start_v_idx) + idx);
          });
        }

        for (const auto& [attribute, accessor_index] : primitive.attributes) {
          const auto& accessor = asset.accessors[accessor_index];
          load_attribute(asset, vertices, std::string{attribute}, accessor, start_v_idx);
        }
      }

      auto vertices_bytes = std::as_bytes(std::span(vertices));
      asset_mesh.buffers.vertices = bb.build_buffer(
          {
              .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              .size = utils::narrow_cast<uint32_t>(vertices_bytes.size_bytes()),
              .flags = 0,
          },
          std::format("vertex buffer for {}", mesh.name));
      t.upload_buffer(asset_mesh.buffers.vertices.buffer, 0, vertices_bytes);

      auto indices_bytes = std::as_bytes(std::span(indices));
      asset_mesh.buffers.indices = bb.build_buffer(
          {
              .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              .size = utils::narrow_cast<uint32_t>(indices_bytes.size_bytes()),
              .flags = 0,
          },
          std::format("index buffer for {}", mesh.name));
      t.upload_buffer(asset_mesh.buffers.indices->buffer, 0, indices_bytes);

      asset_mesh.transform = 0.1F * asset_mesh.transform;
      meshes.push_back(asset_mesh);
    }
  }

  return meshes;
}
auto load(tr::renderer::ImageBuilder& ib, tr::renderer::BufferBuilder& bb, tr::renderer::Transferer& t,
          const fastgltf::Asset& asset) -> std::vector<tr::renderer::Mesh> {
  {
    const auto err = fastgltf::validate(asset);
    TR_ASSERT(err == fastgltf::Error::None, "Invalid GLTF: {} {}", fastgltf::getErrorName(err),
              fastgltf::getErrorMessage(err));
  }

  // TODO: use an id rather than a pointer -> Allows to sort and more
  std::vector<std::shared_ptr<tr::renderer::Material>> materials = load_materials(ib, t, asset);
  return load_meshes(bb, t, asset, materials);
}

auto tr::Gltf::load_from_file(tr::renderer::ImageBuilder& ib, tr::renderer::BufferBuilder& bb,
                              tr::renderer::Transferer& t, const std::filesystem::path& path)
    -> std::vector<tr::renderer::Mesh> {
  fastgltf::Parser parser;
  fastgltf::GltfDataBuffer data;
  fastgltf::Asset asset;

  TR_ASSERT(data.loadFromFile(path), "can't load file {}", path.string());

  {
    auto loaded = [&] {
      const auto options = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
                           fastgltf::Options::LoadExternalImages;

      switch (fastgltf::determineGltfFileType(&data)) {
        case fastgltf::GltfType::glTF:
          return parser.loadGLTF(&data, path.parent_path(), options);
        case fastgltf::GltfType::GLB:
          return parser.loadBinaryGLTF(&data, path.parent_path(), options);
        case fastgltf::GltfType::Invalid:
          break;
      }

      TR_ASSERT(false, "invalid gltf file");
    }();

    if (loaded) {
      asset = std::move(loaded.get());
    } else {
      const auto err = loaded.error();
      TR_ASSERT(err == fastgltf::Error::None, "GLTF Error: {} {}", fastgltf::getErrorName(err),
                fastgltf::getErrorMessage(err));
    }
  }
  return load(ib, bb, t, asset);
}
