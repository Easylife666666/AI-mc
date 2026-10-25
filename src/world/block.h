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
    BLOCK_NETHERRACK,
    BLOCK_SOUL_SAND,
    BLOCK_GLOWSTONE,
    BLOCK_BEDROCK,
    BLOCK_LAVA,
    BLOCK_NETHER_BRICKS,

    // Flowing fluid variants. Level 1..7 is the distance from the source;
    // level 8 is a falling column. Sources keep BLOCK_WATER / BLOCK_LAVA.
    BLOCK_WATER_FLOW_1,
    BLOCK_WATER_FLOW_2,
    BLOCK_WATER_FLOW_3,
    BLOCK_WATER_FLOW_4,
    BLOCK_WATER_FLOW_5,
    BLOCK_WATER_FLOW_6,
    BLOCK_WATER_FLOW_7,
    BLOCK_WATER_FALLING,
    BLOCK_LAVA_FLOW_1,
    BLOCK_LAVA_FLOW_2,
    BLOCK_LAVA_FLOW_3,
    BLOCK_LAVA_FLOW_4,
    BLOCK_LAVA_FLOW_5,
    BLOCK_LAVA_FLOW_6,
    BLOCK_LAVA_FLOW_7,
    BLOCK_LAVA_FALLING,

    BLOCK_COUNT
};

enum class FluidKind : std::uint8_t {
    None = 0,
    Water,
    Lava,
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
bool isLiquid(BlockId id);

FluidKind fluidKind(BlockId id);
bool isWater(BlockId id);
bool isLava(BlockId id);
bool isFluidFlow(BlockId id);  // flowing/falling, i.e. not a source
int fluidLevel(BlockId id);    // 0 source, 1..7 flow, 8 falling, -1 not a fluid
BlockId fluidBlock(FluidKind kind, int level);

// Visible surface height inside the block (1.0 for sources, falling columns and
// non-fluids; flowing levels step down by 1/8 each).
float fluidHeight(BlockId id);

}  // namespace aimc
