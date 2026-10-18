#pragma once

#include <glm/glm.hpp>

namespace aimc {

// Procedurally generated 4x4 tile atlas (16 px per tile). No asset files needed.
class Atlas {
public:
    static constexpr int Grid = 4;
    static constexpr int TileSize = 16;

    Atlas() = default;
    ~Atlas();

    Atlas(const Atlas&) = delete;
    Atlas& operator=(const Atlas&) = delete;

    bool build();
    unsigned int texture() const { return m_texture; }

    // Returns (u0, v0, u1, v1) with a half-texel inset to avoid bleeding.
    glm::vec4 tileUV(int tile) const;

private:
    unsigned int m_texture = 0;
};

}  // namespace aimc
