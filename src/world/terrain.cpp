#include "world/terrain.h"

#include <algorithm>
#include <cmath>

#include "world/block.h"
#include "world/chunk.h"

namespace aimc {

Terrain::Terrain(int seed) {
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
            const int h = heightAt(origin.x + x, origin.z + z);
            const bool beach = h <= SeaLevel + 1;
            for (int y = 0; y < Chunk::SY; ++y) {
                BlockId block = BLOCK_AIR;
                if (y > h) {
                    if (y <= SeaLevel) {
                        block = BLOCK_WATER;
                    }
                } else if (y == h) {
                    block = beach ? BLOCK_SAND : BLOCK_GRASS;
                } else if (y >= h - 3) {
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
}

}  // namespace aimc
