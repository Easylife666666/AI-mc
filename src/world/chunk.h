#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace aimc {

// A chunk is a full-height 16 x 64 x 16 column. Blocks are stored y-major so
// horizontal slices stay contiguous.
class Chunk {
public:
    static constexpr int SX = 16;
    static constexpr int SY = 64;
    static constexpr int SZ = 16;
    static constexpr int VOLUME = SX * SY * SZ;

    explicit Chunk(glm::ivec2 coord);

    glm::ivec2 coord() const { return m_coord; }
    glm::ivec3 origin() const { return glm::ivec3(m_coord.x * SX, 0, m_coord.y * SZ); }

    bool contains(int x, int y, int z) const {
        return x >= 0 && x < SX && y >= 0 && y < SY && z >= 0 && z < SZ;
    }

    static constexpr int index(int x, int y, int z) { return (y * SZ + z) * SX + x; }

    std::uint8_t get(int x, int y, int z) const {
        return m_blocks[static_cast<std::size_t>(index(x, y, z))];
    }
    void set(int x, int y, int z, std::uint8_t id) {
        m_blocks[static_cast<std::size_t>(index(x, y, z))] = id;
    }

    // GPU handles, owned by the chunk but only touched with a live GL context.
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ibo = 0;
    int indexCount = 0;

private:
    glm::ivec2 m_coord;
    std::vector<std::uint8_t> m_blocks;
};

}  // namespace aimc
