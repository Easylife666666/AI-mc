#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include "world/block.h"

namespace aimc {

class Chunk;
class Terrain;
class Atlas;

struct DrawStats {
    int drawCalls = 0;
    long long triangles = 0;
};

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void init(int seed, int radius);
    void buildAllMeshes(const Atlas& atlas);
    void destroyGpu();
    DrawStats draw() const;

    BlockId getBlock(int worldX, int worldY, int worldZ) const;
    Chunk* getChunk(int cx, int cz) const;

    int seed() const { return m_seed; }
    int radius() const { return m_radius; }
    std::size_t chunkCount() const { return m_chunks.size(); }

private:
    static std::uint64_t key(int cx, int cz);

    std::unordered_map<std::uint64_t, std::unique_ptr<Chunk>> m_chunks;
    std::unique_ptr<Terrain> m_terrain;
    int m_seed = 0;
    int m_radius = 0;
};

}  // namespace aimc
