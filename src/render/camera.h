#pragma once

#include <glm/glm.hpp>

namespace aimc {

struct Camera {
    glm::vec3 pos{0.0f, 48.0f, 0.0f};
    float yawDeg = -90.0f;
    float pitchDeg = -18.0f;
    float fovDeg = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 400.0f;

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::mat4 view() const;
    glm::mat4 proj(float aspect) const;
    glm::mat4 viewProj(float aspect) const;
};

struct CameraInput {
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool boost = false;
    float mouseDx = 0.0f;
    float mouseDy = 0.0f;
    bool mouseLook = false;
};

void updateFreeCamera(Camera& cam, const CameraInput& input, float dt);

}  // namespace aimc
