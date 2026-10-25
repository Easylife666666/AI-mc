#include "game/player.h"

#include <algorithm>
#include <cmath>

#include "world/block.h"
#include "world/world.h"

namespace aimc {
namespace {

constexpr float kGravity = 26.0f;
constexpr float kJumpSpeed = 8.4f;
constexpr float kTerminalVelocity = 55.0f;

bool isLiquidBlock(const World& world, float x, float y, float z) {
    return isLiquid(world.getBlock(static_cast<int>(std::floor(x)),
                                   static_cast<int>(std::floor(y)),
                                   static_cast<int>(std::floor(z))));
}

}  // namespace

void Player::setPosition(const glm::vec3& feetPosition) {
    m_pos = feetPosition;
    m_velocity = glm::vec3(0.0f);
    m_onGround = false;
}

void Player::toggleFlying() {
    m_flying = !m_flying;
    m_velocity = glm::vec3(0.0f);
}

bool Player::collides(const World& world, const glm::vec3& feet) const {
    const float half = Width * 0.5f;
    const float eps = 0.001f;
    const int x0 = static_cast<int>(std::floor(feet.x - half + eps));
    const int x1 = static_cast<int>(std::floor(feet.x + half - eps));
    const int y0 = static_cast<int>(std::floor(feet.y + eps));
    const int y1 = static_cast<int>(std::floor(feet.y + Height - eps));
    const int z0 = static_cast<int>(std::floor(feet.z - half + eps));
    const int z1 = static_cast<int>(std::floor(feet.z + half - eps));

    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (isSolid(world.getBlock(x, y, z))) {
                    return true;
                }
            }
        }
    }
    return false;
}

void Player::moveAxis(const World& world, int axis, float amount) {
    if (std::abs(amount) < 1.0e-6f) {
        return;
    }

    const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(amount) / 0.05f)));
    const float stepAmount = amount / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        m_pos[axis] += stepAmount;
        if (!collides(world, m_pos)) {
            continue;
        }

        m_pos[axis] -= stepAmount;
        if (axis == 1) {
            if (stepAmount < 0.0f) {
                m_onGround = true;
            }
            m_velocity.y = 0.0f;
        } else {
            m_velocity[axis] = 0.0f;
        }
        break;
    }
}

void Player::update(const World& world, float yawDeg, const PlayerInput& input, float dt) {
    if (dt <= 0.0f) {
        return;
    }

    const float yaw = glm::radians(yawDeg);
    const glm::vec3 forward(std::cos(yaw), 0.0f, std::sin(yaw));
    const glm::vec3 right(-std::sin(yaw), 0.0f, std::cos(yaw));

    glm::vec3 wish(0.0f);
    if (input.forward) wish += forward;
    if (input.back) wish -= forward;
    if (input.right) wish += right;
    if (input.left) wish -= right;
    if (glm::length(wish) > 0.0f) {
        wish = glm::normalize(wish);
    }

    const glm::vec3 previous = m_pos;
    const bool wasOnGround = m_onGround;

    m_jumpEvent = false;
    m_landEvent = false;
    m_stepEvent = false;

    // Sample the block at the feet: chest-height sampling stops the buoyancy
    // forces as soon as the chest breaks the surface, leaving the player stuck
    // one block below the shore.
    m_inWater = isLiquidBlock(world, m_pos.x, m_pos.y + 0.1f, m_pos.z);

    if (m_flying) {
        const float speed = input.boost ? 24.0f : 12.0f;
        m_velocity = wish * speed;
        m_velocity.y = (input.up ? speed : 0.0f) - (input.down ? speed : 0.0f);
    } else {
        float speed = input.sprint ? 5.9f : 4.3f;
        if (m_inWater) {
            speed = 3.0f;
        }
        const glm::vec3 target(wish.x * speed, 0.0f, wish.z * speed);
        const float accel = m_inWater ? 5.0f : (wasOnGround ? 22.0f : 6.0f);
        const float blend = 1.0f - std::exp(-accel * dt);
        m_velocity.x += (target.x - m_velocity.x) * blend;
        m_velocity.z += (target.z - m_velocity.z) * blend;

        if (m_inWater) {
            m_velocity.y -= 8.0f * dt;
            if (input.jump) {
                // Gentle swim-up by default. Only grant a real jump when the
                // player is pushing into a one-block ledge: in open water this
                // keeps you bobbing at the surface instead of leaping out.
                float up = 3.4f;
                const bool topLayer =
                    !isLiquidBlock(world, m_pos.x, m_pos.y + 1.0f, m_pos.z);
                if (topLayer && glm::length(glm::vec2(wish.x, wish.z)) > 1.0e-4f) {
                    const float ahead = Width * 0.5f + 0.25f;
                    const int px = static_cast<int>(std::floor(m_pos.x + wish.x * ahead));
                    const int pz = static_cast<int>(std::floor(m_pos.z + wish.z * ahead));
                    const int footY = static_cast<int>(std::floor(m_pos.y));
                    if (isSolid(world.getBlock(px, footY + 1, pz)) &&
                        !isSolid(world.getBlock(px, footY + 2, pz))) {
                        up = kJumpSpeed;
                    }
                }
                m_velocity.y = std::max(m_velocity.y, up);
            }
            m_velocity.y *= (1.0f - 2.5f * dt);
        } else {
            m_velocity.y -= kGravity * dt;
            if (input.jump && wasOnGround) {
                m_velocity.y = kJumpSpeed;
                m_jumpEvent = true;
            }
            m_velocity.y = std::max(m_velocity.y, -kTerminalVelocity);
        }
    }

    m_onGround = false;
    moveAxis(world, 0, m_velocity.x * dt);
    moveAxis(world, 2, m_velocity.z * dt);
    moveAxis(world, 1, m_velocity.y * dt);

    if (!m_onGround && m_velocity.y <= 0.001f &&
        collides(world, m_pos - glm::vec3(0.0f, 0.02f, 0.0f))) {
        m_onGround = true;
    }

    if (m_onGround && !wasOnGround && !m_flying) {
        m_landEvent = true;
    }

    const float horizontal = glm::length(glm::vec2(m_pos.x - previous.x, m_pos.z - previous.z));
    if (m_onGround && horizontal > 1.0e-4f) {
        m_stepAccumulator += horizontal;
        if (m_stepAccumulator >= 1.3f) {
            m_stepAccumulator = 0.0f;
            m_stepEvent = true;
        }
    }
}

bool Player::consumeStep() {
    const bool value = m_stepEvent;
    m_stepEvent = false;
    return value;
}

bool Player::consumeJump() {
    const bool value = m_jumpEvent;
    m_jumpEvent = false;
    return value;
}

bool Player::consumeLand() {
    const bool value = m_landEvent;
    m_landEvent = false;
    return value;
}

}  // namespace aimc
