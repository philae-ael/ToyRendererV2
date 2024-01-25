#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "utils/math.h"

namespace tr {

struct CameraInfo {
  glm::mat4x4 projMatrix;
  glm::mat4x4 viewMatrix;
  glm::vec3 cameraPosition;
  float padding = 0.0;
};

struct CameraInput {
  glm::vec2 MouseDelta{0.0F, 0.0F};
  bool Forward = false, Backward = false, Left = false, Right = false, Up = false, Down = false;
  bool RotUp = false, RotDown = false, RotLeft = false, RotRight = false;
};

struct Camera {
  glm::vec3 position{0.0};
  glm::vec3 eulerAngles{0.0};
  float fovy;
  float aspectRatio;
  float zNear;
  float zFar;

  [[nodiscard]] auto cameraInfo() const -> CameraInfo;
};

struct CameraController {
  CameraController()
      : camera{
            .position = glm::vec3(0.0F, 1.0F, 0.0F),
            .eulerAngles = {0.0, utils::math::PI_2, 0.0},
            .fovy = utils::math::PI_4,
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
