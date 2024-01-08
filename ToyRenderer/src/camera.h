#pragma once

#include <glm/gtx/quaternion.hpp>
#include <glm/vec4.hpp>

#include "utils/math.h"

namespace tr {

struct CameraMatrices {
  glm::mat4x4 projMatrix;
  glm::mat4x4 viewMatrix;
};

struct CameraInput {
  bool Forward, Backward, Left, Right, Up, Down;
  bool RotUp, RotDown, RotLeft, RotRight;
  glm::vec2 MouseDelta;
};

struct Camera {
  glm::vec3 position{0.0};
  glm::vec3 eulerAngles{0.0};
  float fov;
  float aspectRatio;
  float zNear;
  float zFar;

  [[nodiscard]] auto cameraMatrices() const -> CameraMatrices;
  [[nodiscard]] auto orientation() const -> glm::quat;
};

struct CameraController {
  CameraController()
      : camera{
            .position = glm::vec3(0.0, 0.0, -3.0),
            .fov = utils::math::PI_4,
            .aspectRatio = 1.0,
            .zNear = 0.1,
            .zFar = 100.0,
        } {}
  Camera camera;

  float speed = 2.5;
  float rotSpeed = utils::math::PI_4;
  float mouseSpeed = 0.5;

  void update(CameraInput input, float Dt);
};

}  // namespace tr
