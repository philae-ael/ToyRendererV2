#include "gltf.h"

#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <utils/assert.h>
#include <utils/cast.h>
#include <utils/misc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <functional>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float2x2.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vector_relational.hpp>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "renderer/deletion_stack.h"
#include "renderer/mesh.h"
#include "renderer/ressources.h"
#include "renderer/synchronisation.h"
#include "renderer/uploader.h"

template <class T>
concept has_bytes = requires(T a) { std::span(a.bytes); };

auto load_texture(tr::renderer::Lifetime& lifetime, tr::renderer::ImageBuilder& ib, tr::renderer::Transferer& t,
                  const fastgltf::Image& image, std::string_view debug_name) -> tr::renderer::ImageRessource {
  uint32_t width = 0;
  uint32_t height = 0;
  std::span<const std::byte> image_data;

  std::visit(utils::overloaded{
                 [&](const has_bytes auto& c) {
                   const auto bytes = std::as_bytes(std::span(c.bytes));

                   int x = 0;
                   int y = 0;
                   int channels = 0;
                   const auto* im =
                       stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                                             utils::narrow_cast<int>(bytes.size_bytes()), &x, &y, &channels, 4);
                   TR_ASSERT(im != nullptr, "Could not load image");

                   width = utils::narrow_cast<uint32_t>(x);
                   height = utils::narrow_cast<uint32_t>(y);
                   image_data = std::as_bytes(std::span{im, static_cast<size_t>(x * y * 4)});
                 },
                 [](const auto&) { TR_ASSERT(false, "MEH"); },
             },
             image.data);

  auto image_ressource = ib.build_image(tr::renderer::ImageDefinition{
      .flags = 0,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .size =
          VkExtent2D{
              .width = width,
              .height = height,
          },
      // TODO: How to deal with RBG (non alpha images?)
      // and more generally with unsupported formats
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .debug_name = debug_name,
  });
  image_ressource.tie(lifetime);

  tr::renderer::ImageMemoryBarrier::submit<1>(t.cmd.vk_cmd,
                                              {{
                                                  image_ressource.prepare_barrier(tr::renderer::SyncImageTransfer),
                                              }});
  t.upload_image(image_ressource, {{0, 0}, {width, height}}, image_data, 4);
  tr::renderer::ImageMemoryBarrier::submit<1>(
      t.graphics_cmd.vk_cmd, {{
                                 image_ressource.prepare_barrier(tr::renderer::SyncFragmentShaderReadOnly),
                             }});
  return image_ressource;
}

auto load_materials(tr::renderer::Lifetime& lifetime, tr::renderer::ImageBuilder& ib, tr::renderer::Transferer& t,
                    const fastgltf::Asset& asset) -> std::vector<std::shared_ptr<tr::renderer::Material>> {
  std::vector<std::shared_ptr<tr::renderer::Material>> materials;

  for (const auto& material : asset.materials) {
    auto mat = std::make_shared<tr::renderer::Material>();

    TR_ASSERT(material.pbrData.baseColorTexture, "no base color texture, not supported");
    const auto& color_texture = asset.textures[material.pbrData.baseColorTexture->textureIndex];
    TR_ASSERT(color_texture.imageIndex, "no image index, not supported");
    mat->base_color_texture = load_texture(lifetime, ib, t, asset.images[*color_texture.imageIndex], "base color");

    if (material.pbrData.metallicRoughnessTexture) {
      const auto& metallic_roughness_texture = asset.textures[material.pbrData.metallicRoughnessTexture->textureIndex];
      TR_ASSERT(metallic_roughness_texture.imageIndex, "no image index, not supported");
      mat->metallic_roughness_texture =
          load_texture(lifetime, ib, t, asset.images[*metallic_roughness_texture.imageIndex], "metal roughness");
    }

    if (material.normalTexture) {
      const auto& normal_texture = asset.textures[material.normalTexture->textureIndex];
      TR_ASSERT(normal_texture.imageIndex, "no image index, not supported");
      mat->normal_texture = load_texture(lifetime, ib, t, asset.images[*normal_texture.imageIndex], "normal map");
    }

    materials.push_back(std::move(mat));
  }
  return materials;
}

template <class T, class Fn>
void load_attribute_into(const fastgltf::Asset& asset, const fastgltf::Accessor& accessor,
                         std::span<tr::renderer::Vertex> vertices, Fn&& proj) {
  auto f = std::forward<Fn>(proj);
  fastgltf::iterateAccessorWithIndex<T>(asset, accessor,
                                        [&](T t, size_t v_idx) { std::invoke(f, vertices[v_idx], t); });
}

template <class T, T tr::renderer::Vertex::*proj>
auto make_vertex_attribute_setter() -> void (*)(tr::renderer::Vertex&, T) {
  return [](tr::renderer::Vertex& vertex, T t) { std::invoke(proj, vertex) = t; };
}

template <class T>
using vertex_attribute_setter = void (*)(tr::renderer::Vertex&, T);

const std::unordered_map<std::string, vertex_attribute_setter<glm::vec2>> vertex_projs_vec2{
    {"TEXCOORD_0", make_vertex_attribute_setter<glm::vec2, &tr::renderer::Vertex::uv1>()},
    {"TEXCOORD_1", make_vertex_attribute_setter<glm::vec2, &tr::renderer::Vertex::uv2>()},
};
const std::unordered_map<std::string, vertex_attribute_setter<glm::vec3>> vertex_projs_vec3{
    {"POSITION", make_vertex_attribute_setter<glm::vec3, &tr::renderer::Vertex::pos>()},
    {"NORMAL", make_vertex_attribute_setter<glm::vec3, &tr::renderer::Vertex::normal>()},
    {"COLOR_0", make_vertex_attribute_setter<glm::vec3, &tr::renderer::Vertex::color>()},
};
const std::unordered_map<std::string, vertex_attribute_setter<glm::vec4>> vertex_projs_vec4{
    {"TANGENT", [](tr::renderer::Vertex& vertex, glm::vec4 tangent) { vertex.tangent = tangent; }},
};

void load_attribute(const fastgltf::Asset& asset, const fastgltf::Accessor& accessor,
                    std::span<tr::renderer::Vertex> vertices, const std::string& attribute) {
  if (const auto proj_it = vertex_projs_vec2.find(attribute); proj_it != vertex_projs_vec2.end()) {
    return load_attribute_into<glm::vec2>(asset, accessor, vertices, proj_it->second);
  }
  if (const auto proj_it = vertex_projs_vec3.find(attribute); proj_it != vertex_projs_vec3.end()) {
    return load_attribute_into<glm::vec3>(asset, accessor, vertices, proj_it->second);
  }
  if (const auto proj_it = vertex_projs_vec4.find(attribute); proj_it != vertex_projs_vec4.end()) {
    return load_attribute_into<glm::vec4>(asset, accessor, vertices, proj_it->second);
  }

  static std::unordered_map<std::string, std::once_flag> warn_once_flags;
  std::call_once(warn_once_flags[attribute], [&] { spdlog::warn("Unknown attribute {}", attribute); });
}

auto load_primitive(const fastgltf::Primitive& primitive, const fastgltf::Asset& asset, std::vector<uint32_t>& indices,
                    std::vector<tr::renderer::Vertex>& vertices, std::shared_ptr<tr::renderer::Material> material)
    -> tr::renderer::GeoSurface {
  const auto vertex_idx_offset = utils::narrow_cast<uint32_t>(vertices.size());
  const auto index_idx_offset = utils::narrow_cast<uint32_t>(indices.size());

  const auto primitive_indices = INLINE_LAMBDA {
    TR_ASSERT(primitive.indicesAccessor, "index buffer");
    const auto& accessor = asset.accessors[*primitive.indicesAccessor];

    indices.resize(index_idx_offset + accessor.count);
    auto primitive_indices_ = std::span(indices).subspan(index_idx_offset);

    fastgltf::iterateAccessorWithIndex<uint32_t>(
        asset, accessor, [&](uint32_t idx, std::size_t i_idx) { primitive_indices_[i_idx] = vertex_idx_offset + idx; });
    return primitive_indices_;
  };
  const auto count = primitive_indices.size();

  bool has_tangent_attribute = false;
  for (const auto& [attribute, accessor_index] : primitive.attributes) {
    const auto& accessor = asset.accessors[accessor_index];
    vertices.resize(std::max(vertices.size(), vertex_idx_offset + accessor.count));
    load_attribute(asset, accessor, std::span(vertices).subspan(vertex_idx_offset), std::string{attribute});

    if (attribute == "TANGENT") {
      has_tangent_attribute = true;
    }
  }
  auto primitive_vertices = std::span(vertices).subspan(vertex_idx_offset);

  if (!has_tangent_attribute) {
    spdlog::debug("model does not contain tangents, computing them! (at least trying)");
    TR_ASSERT(primitive_indices.size() % 3 == 0, "HUHU,  number of indices is not divisible by 3");

    auto it = primitive_indices.begin();
    auto end = primitive_indices.end();
    while (it != end) {
      const auto i0 = *(it++);
      const auto i1 = *(it++);
      const auto i2 = *(it++);

      auto& v0 = vertices[i0];
      auto& v1 = vertices[i1];
      auto& v2 = vertices[i2];

      // p182
      // https://canvas.projekti.info/ebooks/Mathematics%20for%203D%20Game%20Programming%20and%20Computer%20Graphics,%20Third%20Edition.pdf
      // Notes: what is a vertex is reused? The tangent is dependant on the order of iteration, this is bad?
      const auto Q1 = v1.pos - v0.pos;
      const auto S1 = v1.uv1 - v0.uv1;

      const auto Q2 = v2.pos - v0.pos;
      const auto S2 = v2.uv1 - v0.uv1;

      const auto Q = glm::mat2{Q1, Q2};
      const auto S_inv = glm::inverse(glm::mat2{S1, S2});
      const glm::mat2x3 TB = S_inv * Q;

      // gram-schmidt
      v0.tangent = glm::normalize(TB[0] - glm::dot(TB[0], v0.normal) * v0.normal);
      v1.tangent = glm::normalize(TB[0] - glm::dot(TB[0], v1.normal) * v1.normal);
      v2.tangent = glm::normalize(TB[0] - glm::dot(TB[0], v2.normal) * v2.normal);
    }
  }

  const auto bounding_box = INLINE_LAMBDA->tr::renderer::AABB {
    auto mmin = glm::vec3(std::numeric_limits<float>::infinity());
    auto mmax = glm::vec3(-std::numeric_limits<float>::infinity());

    for (const auto& vertex : primitive_vertices) {
      mmin.x = std::min(mmin.x, vertex.pos.x);
      mmin.y = std::min(mmin.y, vertex.pos.y);
      mmin.z = std::min(mmin.z, vertex.pos.z);

      mmax.x = std::max(mmax.x, vertex.pos.x);
      mmax.y = std::max(mmax.y, vertex.pos.y);
      mmax.z = std::max(mmax.z, vertex.pos.z);
    }

    TR_ASSERT(glm::all(glm::lessThanEqual(mmin, mmax)), "bounding box is malformed: {} {}", mmin, mmax);
    return {mmin, mmax};
  };

  return {
      .start = utils::narrow_cast<uint32_t>(index_idx_offset),
      .count = utils::narrow_cast<uint32_t>(count),
      .material = std::move(material),
      .bounding_box = bounding_box,
  };
}

auto load_meshes(tr::renderer::Lifetime& lifetime, tr::renderer::BufferBuilder& bb, tr::renderer::Transferer& t,
                 const fastgltf::Asset& asset, std::span<std::shared_ptr<tr::renderer::Material>> materials)
    -> std::vector<tr::renderer::Mesh> {
  std::vector<tr::renderer::Mesh> meshes;
  for (const auto& scene : asset.scenes) {
    for (auto node_idx : scene.nodeIndices) {
      const auto& node = asset.nodes[node_idx];

      tr::renderer::Mesh asset_mesh;
      std::visit(
          utils::overloaded{
              [&](const fastgltf::Node::TRS& trs) {
                const auto rot = glm::mat4_cast(glm::make_quat(trs.rotation.data()));
                const auto scale = glm::scale(glm::identity<glm::mat4>(), glm::make_vec3(trs.scale.data()));
                const auto translate =
                    glm::translate(glm::identity<glm::mat4>(), glm::make_vec3(trs.translation.data()));

                asset_mesh.transform = translate * rot * scale;
              },
              [&](const fastgltf::Node::TransformMatrix& trs) { asset_mesh.transform = glm::make_mat4(trs.data()); },
          },
          node.transform);

      TR_ASSERT(node.meshIndex, "Child not supported");
      const auto& mesh = asset.meshes[*node.meshIndex];

      std::vector<uint32_t> indices;
      std::vector<tr::renderer::Vertex> vertices;
      for (const auto& primitive : mesh.primitives) {
        TR_ASSERT(primitive.materialIndex, "material needed");

        asset_mesh.surfaces.push_back(
            load_primitive(primitive, asset, indices, vertices, materials[*primitive.materialIndex]));
      }

      auto vertices_bytes = std::as_bytes(std::span(vertices));
      asset_mesh.buffers.vertices =
          bb.build_buffer(lifetime, {
                                        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        .size = utils::narrow_cast<uint32_t>(vertices_bytes.size_bytes()),
                                        .flags = 0,
                                        .debug_name = std::format("vertex buffer for {}", mesh.name),
                                    });
      t.upload_buffer(asset_mesh.buffers.vertices.buffer, 0, vertices_bytes);

      auto indices_bytes = std::as_bytes(std::span(indices));
      asset_mesh.buffers.indices =
          bb.build_buffer(lifetime, {
                                        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        .size = utils::narrow_cast<uint32_t>(indices_bytes.size_bytes()),
                                        .flags = 0,
                                        .debug_name = std::format("index buffer for {}", mesh.name),
                                    });
      t.upload_buffer(asset_mesh.buffers.indices->buffer, 0, indices_bytes);

      meshes.push_back(asset_mesh);
    }
  }

  return meshes;
}
auto load(tr::renderer::Lifetime& lifetime, tr::renderer::ImageBuilder& ib, tr::renderer::BufferBuilder& bb,
          tr::renderer::Transferer& t, const fastgltf::Asset& asset)
    -> std::pair<std::vector<std::shared_ptr<tr::renderer::Material>>, std::vector<tr::renderer::Mesh>> {
  {
    const auto err = fastgltf::validate(asset);
    TR_ASSERT(err == fastgltf::Error::None, "Invalid GLTF: {} {}", fastgltf::getErrorName(err),
              fastgltf::getErrorMessage(err));
  }

  // TODO: use an id rather than a pointer -> Allows to sort and more
  std::vector<std::shared_ptr<tr::renderer::Material>> materials = load_materials(lifetime, ib, t, asset);
  return {
      materials,
      load_meshes(lifetime, bb, t, asset, materials),
  };
}

auto tr::Gltf::load_from_file(tr::renderer::Lifetime& lifetime, tr::renderer::ImageBuilder& ib,
                              tr::renderer::BufferBuilder& bb, tr::renderer::Transferer& t,
                              const std::filesystem::path& path)
    -> std::pair<std::vector<std::shared_ptr<tr::renderer::Material>>, std::vector<tr::renderer::Mesh>> {
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
  return load(lifetime, ib, bb, t, asset);
}

template <>
struct std::formatter<glm::vec4> : formatter<std::string_view> {
  auto format(glm::vec4 val, format_context& ctx) const -> format_context::iterator {  // NOLINT
    return std::format_to(ctx.out(), "{{{}, {}, {}, {}}}", val.x, val.y, val.z, val.w);
  }
};
