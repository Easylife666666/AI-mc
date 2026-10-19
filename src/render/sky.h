#pragma once

#include <glm/glm.hpp>

#include "render/shader.h"

namespace aimc {

struct Camera;

struct SkyState {
    glm::vec3 sunDir{0.0f, 1.0f, 0.0f};
    glm::vec3 zenith{0.2f, 0.45f, 0.86f};
    glm::vec3 horizon{0.68f, 0.82f, 0.96f};
    glm::vec3 fogColor{0.68f, 0.82f, 0.96f};
    glm::vec3 sunColor{1.0f, 0.96f, 0.86f};
    float light = 1.0f;
    float dayFactor = 1.0f;
};

SkyState computeSky(float timeOfDay);

// The nether has no day/night cycle: a warm, dim fog and no sun or stars.
SkyState computeNetherSky(float timeOfDay);

class Sky {
public:
    Sky() = default;
    ~Sky();

    Sky(const Sky&) = delete;
    Sky& operator=(const Sky&) = delete;

    bool init();
    void shutdown();
    void draw(const Camera& camera, const glm::mat4& invViewProj, const SkyState& state);

private:
    Shader m_shader;
    unsigned int m_vao = 0;
};

}  // namespace aimc
