#include "world/terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "world/block.h"
#include "world/chunk.h"

namespace aimc {
namespace {

std::uint32_t hash2(std::uint32_t a, std::uint32_t b, std::uint32_t seed) {
    std::uint32_t h = seed * 0x9E3779B9u + a * 0x85EBCA6Bu + b * 0xC2B2AE35u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

struct Rng {
    std::uint32_t state;
    explicit Rng(std::uint32_t s) : state(s ? s : 0x9E3779B9u) {}
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float unit() { return static_cast<float>(next() & 0xFFFFFFu) / 16777216.0f; }
};

struct TreePlacement {
    int worldX;
    int worldZ;
    int trunkHeight;
};

// Deterministic per-chunk tree candidate list. A chunk asks its 3x3 neighbours
// and clips their trees into itself, so trees may cross chunk borders.
std::vector<TreePlacement> treesForChunk(int seed, int cx, int cz) {
    std::vector<TreePlacement> trees;
    Rng rng(hash2(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz),
                  static_cast<std::uint32_t>(seed) ^ 0x5F3759DFu));

    int count = rng.unit() < 0.55f ? 1 : 0;
    if (rng.unit() < 0.15f) {
        count = 2;
    }
    for (int i = 0; i < count; ++i) {
        const int lx = 1 + static_cast<int>(rng.unit() * 14.0f);
        const int lz = 1 + static_cast<int>(rng.unit() * 14.0f);
        const int trunk = 4 + static_cast<int>(rng.unit() * 3.0f);
        trees.push_back({cx * Chunk::SX + lx, cz * Chunk::SZ + lz, trunk});
    }
    return trees;
}

void placeTree(Chunk& chunk, const TreePlacement& tree, int groundY) {
    const glm::ivec3 origin = chunk.origin();

    auto put = [&](int wx, int wy, int wz, BlockId id, bool onlyIntoAir) {
        const int lx = wx - origin.x;
        const int lz = wz - origin.z;
        if (lx < 0 || lx >= Chunk::SX || lz < 0 || lz >= Chunk::SZ) {
            return;
        }
        if (wy < 0 || wy >= Chunk::SY) {
            return;
        }
        if (onlyIntoAir && chunk.get(lx, wy, lz) != static_cast<std::uint8_t>(BLOCK_AIR)) {
            return;
        }
        chunk.set(lx, wy, lz, static_cast<std::uint8_t>(id));
    };

    const int baseY = groundY + 1;

    // Leaf blob around the top of the trunk.
    for (int ly = -2; ly <= 2; ++ly) {
        const int radius = ly <= 0 ? 2 : 1;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                const bool corner = std::abs(dx) == radius && std::abs(dz) == radius;
                if (corner && (ly == 2 || (ly == 1 && ((dx + dz) & 1) != 0))) {
                    continue;
                }
                if (ly == 2 && std::abs(dx) + std::abs(dz) > 1) {
                    continue;
                }
                put(tree.worldX + dx, baseY + tree.trunkHeight - 1 + ly, tree.worldZ + dz,
                    BLOCK_LEAVES, true);
            }
        }
    }

    // Trunk overwrites leaves so the top cap never turns into a leaf block.
    for (int y = 0; y < tree.trunkHeight; ++y) {
        put(tree.worldX, baseY + y, tree.worldZ, BLOCK_LOG, false);
    }
}

}  // namespace

Terrain::Terrain(int seed) : m_seed(seed) {
    m_height.SetSeed(seed);
    m_height.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_height.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_height.SetFractalOctaves(4);
    m_height.SetFractalLacunarity(2.0f);
    m_height.SetFractalGain(0.5f);
    m_height.SetFrequency(0.012f);
}

int Terrain::heightAt(int worldX, int worldZ) const {
    const float n = m_height.GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    const int h = SeaLevel + 4 + static_cast<int>(std::lround(n * 14.0f));
    return std::clamp(h, 2, Chunk::SY - 6);
}

void Terrain::generate(Chunk& chunk) const {
    const glm::ivec3 origin = chunk.origin();

    for (int z = 0; z < Chunk::SZ; ++z) {
        for (int x = 0; x < Chunk::SX; ++x) {
            const int height = heightAt(origin.x + x, origin.z + z);
            const bool beach = height <= SeaLevel + 1;
            for (int y = 0; y < Chunk::SY; ++y) {
                BlockId block = BLOCK_AIR;
                if (y > height) {
                    if (y <= SeaLevel) {
                        block = BLOCK_WATER;
                    }
                } else if (y == height) {
                    block = beach ? BLOCK_SAND : BLOCK_GRASS;
                } else if (y >= height - 3) {
                    block = beach ? BLOCK_SAND : BLOCK_DIRT;
                } else {
                    block = BLOCK_STONE;
                }
                if (y == 0) {
                    block = BLOCK_STONE;
                }
                chunk.set(x, y, z, static_cast<std::uint8_t>(block));
            }
        }
    }

    // Trees from the 3x3 neighbourhood, clipped into this chunk.
    const int cx = chunk.coord().x;
    const int cz = chunk.coord().y;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (const TreePlacement& tree : treesForChunk(m_seed, cx + dx, cz + dz)) {
                const int ground = heightAt(tree.worldX, tree.worldZ);
                if (ground <= SeaLevel + 1) {
                    continue;  // no trees on beaches or underwater
                }
                placeTree(chunk, tree, ground);
            }
        }
    }
}

NetherTerrain::NetherTerrain(int seed) : m_seed(seed) {
    m_height.SetSeed(seed ^ 0x1F123BB5);
    m_height.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_height.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_height.SetFractalOctaves(3);
    m_height.SetFractalLacunarity(2.0f);
    m_height.SetFractalGain(0.5f);
    m_height.SetFrequency(0.02f);

    m_soil.SetSeed(seed ^ 0x2545F491);
    m_soil.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_soil.SetFrequency(0.05f);
}

int NetherTerrain::heightAt(int worldX, int worldZ) const {
    const float n = m_height.GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    const int h = 26 + static_cast<int>(std::lround(n * 22.0f));
    return std::clamp(h, LavaLevel - 4, Chunk::SY - 14);
}

void NetherTerrain::generate(Chunk& chunk) const {
    const glm::ivec3 origin = chunk.origin();

    for (int z = 0; z < Chunk::SZ; ++z) {
        for (int x = 0; x < Chunk::SX; ++x) {
            const int worldX = origin.x + x;
            const int worldZ = origin.z + z;
            const int height = heightAt(worldX, worldZ);
            const float soil = m_soil.GetNoise(static_cast<float>(worldX),
                                               static_cast<float>(worldZ));
            for (int y = 0; y < Chunk::SY; ++y) {
                BlockId block = BLOCK_AIR;
                if (y == 0 || y == Chunk::SY - 1) {
                    block = BLOCK_BEDROCK;
                } else if (y <= height) {
                    block = (y == height && soil > 0.42f) ? BLOCK_SOUL_SAND : BLOCK_NETHERRACK;
                } else if (y <= LavaLevel) {
                    block = BLOCK_LAVA;
                }
                chunk.set(x, y, z, static_cast<std::uint8_t>(block));
            }
        }
    }

    // Glowstone clusters hanging from the bedrock roof.
    const int cx = chunk.coord().x;
    const int cz = chunk.coord().y;
    Rng rng(hash2(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz),
                  static_cast<std::uint32_t>(m_seed) ^ 0x6A09E667u));
    const int clusters = 1 + (rng.unit() < 0.55f ? 1 : 0);
    for (int i = 0; i < clusters; ++i) {
        const int lx = 2 + static_cast<int>(rng.unit() * 12.0f);
        const int lz = 2 + static_cast<int>(rng.unit() * 12.0f);
        const int count = 2 + static_cast<int>(rng.unit() * 4.0f);
        for (int k = 0; k < count; ++k) {
            const int gx = lx + static_cast<int>(rng.unit() * 3.0f) - 1;
            const int gz = lz + static_cast<int>(rng.unit() * 3.0f) - 1;
            if (gx < 0 || gx >= Chunk::SX || gz < 0 || gz >= Chunk::SZ) {
                continue;
            }
            if (chunk.get(gx, Chunk::SY - 2, gz) == static_cast<std::uint8_t>(BLOCK_AIR)) {
                chunk.set(gx, Chunk::SY - 2, gz,
                          static_cast<std::uint8_t>(BLOCK_GLOWSTONE));
            }
        }
    }
}
}  // namespace aimc
