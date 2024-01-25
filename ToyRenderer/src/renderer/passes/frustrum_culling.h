#pragma once

#include <utils/types.h>

#include <glm/common.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/matrix.hpp>
#include <ranges>

#include "../mesh.h"
#include "utils/cast.h"

namespace tr::renderer {

struct Plane {
  glm::vec3 normal;
  float origin;

  [[nodiscard]] auto dist(glm::vec3 v) const -> float { return glm::dot(normal, v) - origin; }
  /* [[nodiscard]] auto transform(glm::mat4 M) const -> Plane { */
  /*   const auto n = glm::inverseTranspose(M) * glm::vec4(normal, 0.0); */
  /*   return { */
  /*       n, */
  /*       glm::dot(M * glm::vec4(origin * normal, 1.0), n), */
  /*   }; */
  /* } */
  static auto from_normal_point(glm::vec3 normal, glm::vec3 point) -> Plane {
    return {normal, glm::dot(normal, point)};
  }
};

struct Frustum {
  Plane front, back, right, left, top, bottom;

  glm::mat4 transform;

  static auto from_camera(const Camera& cam) -> Frustum {
    const float half_height = cam.zFar * utils::narrow_cast<float>(std::tan(cam.fovy / 2.0));
    const float half_width = cam.aspectRatio * half_height;

    const auto n_right = glm::cross(glm::vec3{0, 1, 0}, glm::vec3{half_width, 0, -cam.zFar});
    const auto n_left = glm::vec3{-n_right.x, n_right.y, n_right.z};

    const auto n_top = glm::cross(glm::vec3{0, half_height, -cam.zFar}, glm::vec3{1, 0, 0});
    const auto n_bottom = glm::vec3{n_top.x, -n_top.y, n_top.z};
    return Frustum{
        .front = Plane::from_normal_point({0, 0, -1}, {0, 0, -cam.zNear}),
        .back = Plane::from_normal_point({0, 0, 1}, {0, 0, -cam.zFar}),
        .right = {n_right, 0},
        .left = {n_left, 0},
        .top = {n_top, 0},
        .bottom = {n_bottom, 0},
        .transform = glm::identity<glm::mat4>(),
    };
  }
};

struct FrustrumCulling {
  static auto init() -> FrustrumCulling { return {}; }

  static auto filter_one(const Frustum& frustum, const AABB& bounding_box) -> bool {
    const auto bb = bounding_box.transform(frustum.transform);
    const auto center = 0.5F * (bb.max + bb.min);
    const auto half_extent = bb.max - center;

    const auto test_plane = [&](const Plane& plane) -> bool {
      const auto r = glm::dot(half_extent, glm::abs(plane.normal));
      return plane.dist(center) >= -r;
    };

    return test_plane(frustum.front) && test_plane(frustum.back) && test_plane(frustum.top) &&
           test_plane(frustum.bottom) && test_plane(frustum.right) && test_plane(frustum.left);
  }

  template <utils::types::range_of<const GeoSurface&> Range>
  static auto filter(const Frustum& frustum, Range surfaces) {
    return surfaces |
           std::views::filter([&](const GeoSurface& surface) { return filter_one(frustum, surface.bounding_box); });
  }
};

}  // namespace tr::renderer
