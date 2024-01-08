#include "camera.h"

#include <glm/mat4x4.hpp>

namespace tr {

auto Camera::cameraMatrices() const -> CameraMatrices {
  auto projMatrix = glm::perspective(fov, aspectRatio, zNear, zFar);

  // opengl / gtlf style
  projMatrix[1][1] *= -1;

  return {
      .projMatrix = projMatrix,
      .viewMatrix = glm::toMat4(orientation()) * glm::translate(glm::mat4(1.0), position),
  };
}

auto Camera::orientation() const -> glm::quat {
  return glm::angleAxis(eulerAngles.x, glm::vec3{1.0, 0.0, 0.0}) *
         glm::angleAxis(eulerAngles.y, glm::vec3{0.0, 1.0, 0.0});
}

void CameraController::update(CameraInput input, float Dt) {
  // In camera Space
  const glm::vec3 CamLocalDirectionHorizontal =
      (static_cast<float>(input.Forward) - static_cast<float>(input.Backward)) * glm::vec3(0.0, 0.0, 1.0) +
      (static_cast<float>(input.Left) - static_cast<float>(input.Right)) * glm::vec3(1.0, 0.0, 0.0);
  const glm::vec3 CamLocalDirectionUpDown =
      (static_cast<float>(input.Down) - static_cast<float>(input.Up)) * glm::vec3(0.0, 1.0, 0.0);

  const float RotX =
      static_cast<float>(input.RotDown) - static_cast<float>(input.RotUp) + mouseSpeed * input.MouseDelta.y;
  const float RotY =
      static_cast<float>(input.RotRight) - static_cast<float>(input.RotLeft) + mouseSpeed * input.MouseDelta.x;

  const glm::vec3 RotVelocity = glm::vec3{rotSpeed * RotX, rotSpeed * RotY, 0.0};
  camera.eulerAngles += Dt * RotVelocity;

  const glm::vec3 CamWorldVelocity =
      speed * (glm::rotate(glm::inverse(camera.orientation()), CamLocalDirectionHorizontal) + CamLocalDirectionUpDown);

  camera.position += Dt * CamWorldVelocity;
}

}  // namespace tr
