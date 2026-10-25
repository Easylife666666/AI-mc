#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include <glm/glm.hpp>

namespace aimc {

enum class ChunkState : std::uint8_t {
    Empty = 0,
    Generating = 1,
    Generated = 2,  // block data valid, waiting for / needs a mesh
    Meshing = 3,
    Meshed = 4,  // mesh queued for GPU upload
    Ready = 5
};

struct GpuMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ibo = 0;
    int indexCount = 0;
};

// A full-height 16 x 64 x 16 column. Blocks are stored y-major so horizontal
// slices stay contiguous. Block data is immutable while meshing; edits take the
// write lock.
class Chunk {
public:
    static constexpr int SX = 16;
    static constexpr int SY = 64;
    static constexpr int SZ = 16;
    static constexpr int VOLUME = SX * SY * SZ;

    explicit Chunk(glm::ivec2 coord);

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

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

    // Raw access is only for generation (state == Generating) and edit application.
    std::vector<std::uint8_t>& rawBlocks() { return m_blocks; }
    const std::vector<std::uint8_t>& rawBlocks() const { return m_blocks; }

    std::shared_lock<std::shared_mutex> readLock() const {
        return std::shared_lock<std::shared_mutex>(m_mutex);
    }
    std::unique_lock<std::shared_mutex> writeLock() {
        return std::unique_lock<std::shared_mutex>(m_mutex);
    }

    std::atomic<ChunkState> state{ChunkState::Empty};
    std::atomic<std::uint32_t> version{0};

    GpuMesh opaqueMesh;
    GpuMesh waterMesh;
    GpuMesh lavaMesh;
    GpuMesh portalMesh;

    // Main-thread flag: fluids in this chunk have been scanned after load.
    bool fluidWoken = false;

private:
    glm::ivec2 m_coord;
    std::vector<std::uint8_t> m_blocks;
    mutable std::shared_mutex m_mutex;
};

}  // namespace aimc
