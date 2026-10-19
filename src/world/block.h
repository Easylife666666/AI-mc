#pragma once

#include <cstdint>

namespace aimc {

enum BlockId : std::uint8_t {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_PLANKS,
    BLOCK_COBBLE,
    BLOCK_WATER,
    BLOCK_GRAVEL,
    BLOCK_STONE_BRICKS,
    BLOCK_BRICKS,
    BLOCK_SNOW,
    BLOCK_OBSIDIAN,
    BLOCK_PORTAL_X,   // portal plane normal along X (spans Y/Z)
    BLOCK_PORTAL_Z,   // portal plane normal along Z (spans X/Y)
    BLOCK_COUNT
};

// Tile indices refer to the procedural 4x4 texture atlas (see render/atlas.cpp).
struct BlockDef {
    const char* name;
    int tileTop;
    int tileBottom;
    int tileSide;
    bool solid;
    bool opaque;
};

const BlockDef& blockDef(BlockId id);
bool isOpaque(BlockId id);
bool isSolid(BlockId id);

}  // namespace aimc
