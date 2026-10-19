#pragma once

#include <FastNoiseLite.h>

namespace aimc {

class Chunk;

class Terrain {
public:
    static constexpr int SeaLevel = 20;

    explicit Terrain(int seed);

    // Fills the chunk with terrain and trees. Safe to call concurrently from
    // multiple threads (each thread should own a Terrain instance).
    void generate(Chunk& chunk) const;
    int heightAt(int worldX, int worldZ) const;

    int seed() const { return m_seed; }

private:
    int m_seed = 0;
    FastNoiseLite m_height;
};

// A compressed nether: netherrack shelves, a lava sea, soul sand patches and a
// bedrock roof. No 3D cave carving yet, so the space between the shelves and
// the ceiling stays open.
class NetherTerrain {
public:
    static constexpr int LavaLevel = 12;

    explicit NetherTerrain(int seed);

    void generate(Chunk& chunk) const;
    int heightAt(int worldX, int worldZ) const;

    int seed() const { return m_seed; }

private:
    int m_seed = 0;
    FastNoiseLite m_height;
    FastNoiseLite m_soil;
};

}  // namespace aimc
