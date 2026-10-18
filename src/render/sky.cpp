#include "render/sky.h"

#include <cmath>

#include <GL/glew.h>
#include <spdlog/spdlog.h>

#include "render/camera.h"

namespace aimc {
namespace {

const char* kVertexShader = R"GLSL(
#version 330 core
out vec2 vNdc;
void main() {
    // Fullscreen triangle, no vertex buffer needed.
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    vNdc = p;
    gl_Position = vec4(p, 1.0, 1.0);
}
)GLSL";

const char* kFragmentShader = R"GLSL(
#version 330 core
in vec2 vNdc;

uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uDayFactor;

out vec4 FragColor;

void main() {
    vec4 world = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(world.xyz / world.w - uCamPos);

    float h = clamp(dir.y, 0.0, 1.0);
    vec3 color = mix(uHorizon, uZenith, pow(h, 0.55));

    // Cheap sun disk + glow (avoids large pow() exponents on software GL).
    float sun = max(dot(dir, uSunDir), 0.0);
    float disk = smoothstep(0.9994, 0.99975, sun);
    float glow = sun * sun * sun * sun * sun * sun * sun * sun;
    color += uSunColor * (disk * 2.5 + glow * 0.18);

    if (uDayFactor < 0.35 && dir.y > 0.02) {
        vec3 cell = floor(dir * 420.0);
        float hsh = fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        float star = smoothstep(0.9975, 1.0, hsh) * (1.0 - uDayFactor / 0.35);
        color += vec3(star) * 1.2;
    }

    if (dir.y < 0.0) {
        color = mix(color, uHorizon * 0.65, clamp(-dir.y * 4.0, 0.0, 1.0));
    }

    FragColor = vec4(color, 1.0);
}
)GLSL";

constexpr float kPi = 3.14159265358979323846f;

}  // namespace

SkyState computeSky(float timeOfDay) {
    SkyState state;
    const float t = timeOfDay - std::floor(timeOfDay);
    const float angle = (t - 0.25f) * 2.0f * kPi;
    state.sunDir = glm::normalize(glm::vec3(std::cos(angle), std::sin(angle), 0.25f));

    const float elevation = state.sunDir.y;
    const float day = glm::smoothstep(-0.10f, 0.30f, elevation);
    const float dawn = std::exp(-std::pow((elevation - 0.02f) / 0.16f, 2.0f));

    const glm::vec3 nightZenith(0.012f, 0.016f, 0.045f);
    const glm::vec3 nightHorizon(0.04f, 0.055f, 0.10f);
    const glm::vec3 dayZenith(0.20f, 0.45f, 0.86f);
    const glm::vec3 dayHorizon(0.68f, 0.82f, 0.96f);
    const glm::vec3 sunsetHorizon(0.98f, 0.42f, 0.16f);
    const glm::vec3 sunsetZenith(0.22f, 0.22f, 0.42f);

    state.zenith = glm::mix(nightZenith, dayZenith, day);
    state.horizon = glm::mix(nightHorizon, dayHorizon, day);
    state.horizon = glm::mix(state.horizon, sunsetHorizon, dawn * 0.75f);
    state.zenith = glm::mix(state.zenith, sunsetZenith, dawn * 0.35f);
    state.sunColor = glm::mix(glm::vec3(1.0f, 0.45f, 0.20f), glm::vec3(1.0f, 0.96f, 0.86f), day);
    state.light = 0.28f + 0.72f * day;
    state.dayFactor = day;
    state.fogColor = state.horizon * (0.85f + 0.15f * day);
    return state;
}

Sky::~Sky() { shutdown(); }

bool Sky::init() {
    if (!m_shader.compile(kVertexShader, kFragmentShader)) {
        return false;
    }
    glGenVertexArrays(1, &m_vao);
    return true;
}

void Sky::shutdown() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_shader.destroy();
}

void Sky::draw(const Camera& camera, const glm::mat4& invViewProj, const SkyState& state) {
    if (m_vao == 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    m_shader.use();
    m_shader.setMat4("uInvViewProj", invViewProj);
    m_shader.setVec3("uCamPos", camera.pos);
    m_shader.setVec3("uZenith", state.zenith);
    m_shader.setVec3("uHorizon", state.horizon);
    m_shader.setVec3("uSunDir", state.sunDir);
    m_shader.setVec3("uSunColor", state.sunColor);
    m_shader.setFloat("uDayFactor", state.dayFactor);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

}  // namespace aimc
