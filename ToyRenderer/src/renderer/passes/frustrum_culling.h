#pragma once

#include <utils/types.h>  // for range_of

#include <array>
#include <glm/common.hpp>  // for abs
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>                   // for vec3, mat4
#include <glm/geometric.hpp>             // for dot, cross
#include <glm/gtc/matrix_transform.hpp>  // for identity
#include <glm/mat4x4.hpp>                // for mat4
#include <glm/vec3.hpp>                  // for vec, operator*, operator+
#include <ranges>                        // for filter

#include "../../camera.h"  // for Camera
#include "../mesh.h"       // for AABB, GeoSurface

namespace tr::renderer {

struct Plane {
  glm::vec4 p;

  [[nodiscard]] inline auto dist(glm::vec3 v) const -> float { return dist(glm::vec4(v, 1.0)); }
  [[nodiscard]] inline auto dist(glm::vec4 v) const -> float { return glm::dot(p, v); }
  [[nodiscard]] auto transform(const glm::mat4& transform_) const -> Plane {
    // Proof :
    // <P, Mv> = transpose(P)Mv = transpose(transpose(M)P)v = <transpose(M)P, v>
    return {glm::transpose(transform_) * p};
  }
  static auto from_normal_point(glm::vec3 normal, glm::vec3 point) -> Plane {
    return {glm::vec4(normal, -glm::dot(normal, point))};
  }
};

struct Frustum {
  Plane front, back, right, left, top, bottom;

  static auto from_camera(const Camera& cam) -> Frustum;
  [[nodiscard]] auto transform(const glm::mat4& transform_) const -> Frustum;
  [[nodiscard]] auto points() const -> std::array<glm::vec3, 8>;
};

struct FrustrumCulling {
  static auto init() -> FrustrumCulling { return {}; }

  static auto filter_one(const Frustum&, const AABB&) -> bool;

  template <utils::types::range_of<const GeoSurface&> Range>
  static auto filter(const Frustum& frustum, Range surfaces) {
    return surfaces |
           std::views::filter([&](const GeoSurface& surface) { return filter_one(frustum, surface.bounding_box); });
  }
};

}  // namespace tr::renderer
