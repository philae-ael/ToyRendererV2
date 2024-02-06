#include "frustrum_culling.h"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float4.hpp>

namespace tr::renderer {

auto FrustrumCulling::filter_one(const Frustum& frustum, const AABB& bb) -> bool {
  // http://iquilezles.org/articles/frustumcorrect/
  const auto fully_outside = [&](const Plane& plane) -> bool {
    int c = 0;
    c += plane.dist(glm::vec3(bb.max.x, bb.max.y, bb.max.z)) < 0.0 ? 1 : 0;
    c += plane.dist(glm::vec3(bb.max.x, bb.max.y, bb.min.z)) < 0.0 ? 1 : 0;
    c += plane.dist(glm::vec3(bb.max.x, bb.min.y, bb.max.z)) < 0.0 ? 1 : 0;
    c += plane.dist(glm::vec3(bb.max.x, bb.min.y, bb.min.z)) < 0.0 ? 1 : 0;
    c += plane.dist(glm::vec3(bb.min.x, bb.max.y, bb.max.z)) < 0.0 ? 1 : 0;
    c += plane.dist(glm::vec3(bb.min.x, bb.max.y, bb.min.z)) < 0.0 ? 1 : 0;
    c += plane.dist(glm::vec3(bb.min.x, bb.min.y, bb.max.z)) < 0.0 ? 1 : 0;
    c += plane.dist(glm::vec3(bb.min.x, bb.min.y, bb.min.z)) < 0.0 ? 1 : 0;
    return c == 8;
  };

  return !(fully_outside(frustum.top) || fully_outside(frustum.bottom) || fully_outside(frustum.right) ||
           fully_outside(frustum.left));

  /*   // Frustrum outside bounding box */
  /*   const auto points = frustum.points(); */
  /*   std::array<int, 6> c{}; */
  /*   for (const auto point : points) { */
  /*     c[0] += point.x > bb.max.x ? 1 : 0; */
  /*     c[1] += point.x < bb.min.x ? 1 : 0; */
  /*     c[2] += point.y > bb.max.y ? 1 : 0; */
  /*     c[3] += point.y < bb.min.y ? 1 : 0; */
  /*     c[4] += point.z > bb.max.z ? 1 : 0; */
  /*     c[5] += point.z < bb.min.z ? 1 : 0; */
  /*   } */
  /**/
  /*   return !std::ranges::any_of(c, [](int c) { return c == 8; }); */
}

auto Frustum::transform(const glm::mat4& transform_) const -> Frustum {
  return {
      front.transform(transform_), back.transform(transform_), right.transform(transform_),
      left.transform(transform_),  top.transform(transform_),  bottom.transform(transform_),
  };
}

auto Frustum::from_camera(const Camera& cam) -> Frustum {
  const float half_height = cam.zFar * utils::narrow_cast<float>(std::tan(cam.fovy / 2.0));
  const float half_width = cam.aspectRatio * half_height;

  const auto n_right = glm::cross(glm::vec3{0, 1, 0}, glm::vec3{half_width, 0, -cam.zFar});
  const auto n_left = glm::vec3{-n_right.x, n_right.y, n_right.z};

  const auto n_top = glm::cross(glm::vec3{0, half_height, -cam.zFar}, glm::vec3{1, 0, 0});
  const auto n_bottom = glm::vec3{n_top.x, -n_top.y, n_top.z};
  return Frustum{
      .front = Plane::from_normal_point({0, 0, -1}, {0, 0, -cam.zNear}),
      .back = Plane::from_normal_point({0, 0, 1}, {0, 0, -cam.zFar}),
      .right = Plane::from_normal_point(n_right, {}),
      .left = Plane::from_normal_point(n_left, {}),
      .top = Plane::from_normal_point(n_top, {}),
      .bottom = Plane::from_normal_point(n_bottom, {}),

  };
}

// Those should be stored alongside the frustrum
auto Frustum::points() const -> std::array<glm::vec3, 8> {
  auto intersect_planes = [&](const Plane& a, const Plane& b, const Plane& c) -> glm::vec3 {
    const glm::vec3 r1{a.p.x, b.p.x, c.p.x};
    const glm::vec3 r2{a.p.y, b.p.y, c.p.y};
    const glm::vec3 r3{a.p.z, b.p.z, c.p.z};
    const auto inv = glm::inverse(glm::mat3(r1, r2, r3));
    return inv * glm::vec3(-a.p.w, -b.p.w, -c.p.w);
  };
  return {
      intersect_planes(front, bottom, right), intersect_planes(front, bottom, left),
      intersect_planes(front, top, right),    intersect_planes(front, top, left),
      intersect_planes(back, bottom, right),  intersect_planes(back, bottom, left),
      intersect_planes(back, top, right),     intersect_planes(back, top, left),
  };
}
}  // namespace tr::renderer
