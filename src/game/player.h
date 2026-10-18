#pragma once

#include <glm/glm.hpp>

namespace aimc {

class World;

struct PlayerInput {
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool jump = false;
    bool up = false;
    bool down = false;
    bool sprint = false;
};

class Player {
public:
    static constexpr float Width = 0.6f;
    static constexpr float Height = 1.8f;
    static constexpr float EyeHeight = 1.62f;

    void setPosition(const glm::vec3& feetPosition);
    glm::vec3 position() const { return m_pos; }
    glm::vec3 eyePosition() const { return m_pos + glm::vec3(0.0f, EyeHeight, 0.0f); }
    glm::vec3 velocity() const { return m_velocity; }

    bool onGround() const { return m_onGround; }
    bool flying() const { return m_flying; }
    bool inWater() const { return m_inWater; }
    void toggleFlying();

    void update(const World& world, float yawDeg, const PlayerInput& input, float dt);

    bool consumeStep();
    bool consumeJump();
    bool consumeLand();

private:
    bool collides(const World& world, const glm::vec3& feet) const;
    void moveAxis(const World& world, int axis, float amount);

    glm::vec3 m_pos{0.5f, 64.0f, 0.5f};
    glm::vec3 m_velocity{0.0f};
    bool m_onGround = false;
    bool m_flying = false;
    bool m_inWater = false;

    bool m_stepEvent = false;
    bool m_jumpEvent = false;
    bool m_landEvent = false;
    float m_stepAccumulator = 0.0f;
};

}  // namespace aimc
