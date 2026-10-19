#pragma once

#include <glm/glm.hpp>

namespace aimc {

// Procedurally generated 8x8 tile atlas. Each 16 px tile gets a 2 px replicated
// border so mipmaps do not bleed neighbouring tiles. No asset files needed.
class Atlas {
public:
    static constexpr int Grid = 8;
    static constexpr int TileSize = 16;
    static constexpr int Padding = 2;
    static constexpr int Pitch = TileSize + Padding * 2;
    static constexpr int Size = Grid * Pitch;

    Atlas() = default;
    ~Atlas();

    Atlas(const Atlas&) = delete;
    Atlas& operator=(const Atlas&) = delete;

    bool build();
    unsigned int texture() const { return m_texture; }

    // Returns (u0, v0, u1, v1) of the tile content in texture space.
    glm::vec4 tileUV(int tile) const;

private:
    unsigned int m_texture = 0;
};

}  // namespace aimc
