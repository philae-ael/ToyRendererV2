#include "camera.h"

#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>

namespace tr {

auto Camera::cameraInfo() const -> CameraInfo {
  auto projMatrix = glm::perspective(fov, aspectRatio, zNear, zFar);

  // opengl / gtlf style
  projMatrix[1][1] *= -1;

  const glm::quat orientation = glm::angleAxis(eulerAngles.x, glm::vec3{-1.0, 0.0, 0.0}) *
                                glm::angleAxis(eulerAngles.y, glm::vec3{0.0, -1.0, 0.0}) *
                                glm::angleAxis(eulerAngles.z, glm::vec3{0.0, 0.0, -1.0});

  return {
      .projMatrix = projMatrix,
      .viewMatrix = glm::toMat4(orientation) * glm::translate(glm::identity<glm::mat4>(), -position),
      .cameraPosition = position,
  };
}

void CameraController::update(CameraInput input, float Dt) {
  // In camera Space
  const glm::vec3 CamLocalDirectionHorizontal =
      (static_cast<float>(input.Forward) - static_cast<float>(input.Backward)) * glm::vec3(0.0, 0.0, -1.0) +
      (static_cast<float>(input.Right) - static_cast<float>(input.Left)) * glm::vec3(1.0, 0.0, 0.0);
  const glm::vec3 CamLocalDirectionUpDown =
      (static_cast<float>(input.Up) - static_cast<float>(input.Down)) * glm::vec3(0.0, 1.0, 0.0);

  const float RotX =
      static_cast<float>(input.RotUp) - static_cast<float>(input.RotDown) + mouseSpeed * input.MouseDelta.y;
  const float RotY =
      static_cast<float>(input.RotLeft) - static_cast<float>(input.RotRight) + mouseSpeed * input.MouseDelta.x;

  const glm::vec3 RotVelocity = glm::vec3{rotSpeed * RotX, rotSpeed * RotY, 0.0};
  camera.eulerAngles += Dt * RotVelocity;

  const glm::quat orientation = glm::angleAxis(camera.eulerAngles.x, glm::vec3{1.0, 0.0, 0.0}) *
                                glm::angleAxis(camera.eulerAngles.y, glm::vec3{0.0, 1.0, 0.0}) *
                                glm::angleAxis(camera.eulerAngles.z, glm::vec3{0.0, 0.0, 1.0});

  const glm::vec3 CamWorldVelocity =
      speed * (glm::rotate(orientation, CamLocalDirectionHorizontal) + CamLocalDirectionUpDown);

  camera.position += Dt * CamWorldVelocity;
}

}  // namespace tr
