#include "gltf.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

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
#include <mutex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "renderer/mesh.h"
#include "renderer/ressources.h"
#include "renderer/uploader.h"
#include "renderer/vertex.h"
#include "renderer/vulkan_engine.h"
#include "utils/assert.h"
#include "utils/cast.h"

template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template <class T>
concept has_bytes = requires(T a) { std::span(a.bytes); };

auto load(tr::renderer::ImageBuilder& /*ib*/, tr::renderer::BufferBuilder& bb, tr::renderer::Transferer& t,
          const fastgltf::Asset& asset) -> std::vector<tr::renderer::Mesh> {
  {
    const auto err = fastgltf::validate(asset);
    TR_ASSERT(err == fastgltf::Error::None, "Invalid GLTF: {} {}", fastgltf::getErrorName(err),
              fastgltf::getErrorMessage(err));
  }

  std::vector<tr::renderer::Mesh> meshes{};
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
          asset_mesh.surfaces.push_back({
              .start = utils::narrow_cast<uint32_t>(start_i_idx),
              .count = utils::narrow_cast<uint32_t>(accessor.count),
          });

          indices.reserve(indices.size() + accessor.count);
          fastgltf::iterateAccessor<uint32_t>(asset, accessor, [&](uint32_t idx) {
            indices.push_back(utils::narrow_cast<uint32_t>(start_v_idx) + idx);
          });
        }

        for (const auto& [attribute, accessor_index] : primitive.attributes) {
          const auto& accessor = asset.accessors[accessor_index];
          vertices.resize(std::max(vertices.size(), start_v_idx + accessor.count));

          if (attribute == "POSITION") {
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, accessor, [&](glm::vec3 pos, size_t v_idx) { vertices[start_v_idx + v_idx].pos = pos; });
            continue;
          }
          if (attribute == "NORMAL") {
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 normal, size_t v_idx) {
              vertices[start_v_idx + v_idx].normal = normal;
            });
            continue;
          }
          if (attribute == "TEXCOORD_0") {
            fastgltf::iterateAccessorWithIndex<glm::vec2>(
                asset, accessor, [&](glm::vec2 uv, size_t v_idx) { vertices[start_v_idx + v_idx].uv = uv; });
            continue;
          }
          if (attribute == "COLOR_0") {
            fastgltf::iterateAccessorWithIndex<glm::vec4>(
                asset, accessor, [&](glm::vec4 color, size_t v_idx) { vertices[start_v_idx + v_idx].color = color; });
            continue;
          }

          static std::unordered_map<std::string, std::once_flag> warn_once_flags;
          std::call_once(warn_once_flags[attribute.c_str()], [&] { spdlog::warn("Unknown attribute {}", attribute); });
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
      t.upload(asset_mesh.buffers.vertices.buffer, 0, vertices_bytes);

      auto indices_bytes = std::as_bytes(std::span(indices));
      asset_mesh.buffers.indices = bb.build_buffer(
          {
              .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              .size = utils::narrow_cast<uint32_t>(indices_bytes.size_bytes()),
              .flags = 0,
          },
          std::format("index buffer for {}", mesh.name));
      t.upload(asset_mesh.buffers.indices->buffer, 0, indices_bytes);

      asset_mesh.transform = 0.1F * asset_mesh.transform;
      meshes.push_back(asset_mesh);
    }
  }

  return meshes;
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
      const auto options = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

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
