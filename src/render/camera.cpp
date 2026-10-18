#include "render/camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace aimc {
namespace {
constexpr float kMouseSensitivity = 0.10f;  // degrees per pixel
constexpr float kBaseSpeed = 16.0f;
constexpr float kBoostMultiplier = 4.0f;
}  // namespace

glm::vec3 Camera::forward() const {
    const float cy = std::cos(glm::radians(yawDeg));
    const float sy = std::sin(glm::radians(yawDeg));
    const float cp = std::cos(glm::radians(pitchDeg));
    const float sp = std::sin(glm::radians(pitchDeg));
    return glm::normalize(glm::vec3(cp * cy, sp, cp * sy));
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::mat4 Camera::view() const {
    return glm::lookAt(pos, pos + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::proj(float aspect) const {
    return glm::perspective(glm::radians(fovDeg), aspect, nearPlane, farPlane);
}

glm::mat4 Camera::viewProj(float aspect) const { return proj(aspect) * view(); }

void updateFreeCamera(Camera& cam, const CameraInput& input, float dt) {
    if (input.mouseLook) {
        cam.yawDeg += input.mouseDx * kMouseSensitivity;
        cam.pitchDeg -= input.mouseDy * kMouseSensitivity;
        cam.pitchDeg = std::clamp(cam.pitchDeg, -89.0f, 89.0f);
    }

    glm::vec3 move(0.0f);
    const glm::vec3 f = cam.forward();
    const glm::vec3 r = cam.right();
    if (input.forward) move += f;
    if (input.back) move -= f;
    if (input.right) move += r;
    if (input.left) move -= r;
    if (input.up) move += glm::vec3(0.0f, 1.0f, 0.0f);
    if (input.down) move -= glm::vec3(0.0f, 1.0f, 0.0f);

    if (glm::length(move) > 0.0f) {
        const float speed = kBaseSpeed * (input.boost ? kBoostMultiplier : 1.0f);
        cam.pos += glm::normalize(move) * speed * dt;
    }
}

}  // namespace aimc
