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
  bool Forward = false, Backward = false, Left = false, Right = false, Up = false, Down = false;
  bool RotUp = false, RotDown = false, RotLeft = false, RotRight = false;
  glm::vec2 MouseDelta{0.0F, 0.0F};
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
            .position = glm::vec3(0.0F, 0.0F, -3.0F),
            .fov = utils::math::PI_4,
            .aspectRatio = 1.0F,
            .zNear = 0.1F,
            .zFar = 100.0F,
        } {}
  Camera camera;

  float speed = 2.5;
  float rotSpeed = utils::math::PI_4;
  float mouseSpeed = 0.5;

  void update(CameraInput input, float Dt);
};

}  // namespace tr
