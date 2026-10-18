#pragma once

#include <FastNoiseLite.h>

namespace aimc {

class Chunk;

class Terrain {
public:
    static constexpr int SeaLevel = 20;

    explicit Terrain(int seed);

    void generate(Chunk& chunk) const;
    int heightAt(int worldX, int worldZ) const;

private:
    FastNoiseLite m_height;
};

}  // namespace aimc
