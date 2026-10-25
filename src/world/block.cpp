#include "world/block.h"

namespace aimc {
namespace {

const BlockDef kDefs[BLOCK_COUNT] = {
    // name             top bottom side  solid  opaque
    // Air has no texture: tile 15 is unused, so a missed AIR check shows up
    // as the magenta debug tile instead of silently drawing another block.
    {"Air",             15,  15,   15,  false, false},
    {"Grass Block",      0,   2,    1,  true,  true},   // tile 0 grass top, 1 side, 2 dirt
    {"Dirt",             2,   2,    2,  true,  true},
    {"Stone",            3,   3,    3,  true,  true},
    {"Sand",             4,   4,    4,  true,  true},
    {"Oak Log",          6,   6,    5,  true,  true},   // tile 5 log side, 6 log top
    {"Oak Leaves",       7,   7,    7,  true,  true},
    {"Oak Planks",       8,   8,    8,  true,  true},
    {"Cobblestone",      9,   9,    9,  true,  true},
    {"Water",           10,  10,   10,  false, false},
    {"Gravel",          11,  11,   11,  true,  true},
    {"Stone Bricks",    12,  12,   12,  true,  true},
    {"Bricks",          13,  13,   13,  true,  true},
    {"Snow Block",      14,  14,   14,  true,  true},
    {"Obsidian",        17,  17,   17,  true,  true},
    {"Nether Portal",   18,  18,   18,  false, false},
    {"Nether Portal",   18,  18,   18,  false, false},
    {"Netherrack",      19,  19,   19,  true,  true},
    {"Soul Sand",       20,  20,   20,  true,  true},
    {"Glowstone",       21,  21,   21,  true,  true},
    {"Bedrock",         22,  22,   22,  true,  true},
    {"Lava",            23,  23,   23,  false, false},
    {"Nether Bricks",   24,  24,   24,  true,  true},
    // Flowing water / lava share the source textures; the level lives in the id.
    {"Flowing Water",   10,  10,   10,  false, false},
    {"Flowing Water",   10,  10,   10,  false, false},
    {"Flowing Water",   10,  10,   10,  false, false},
    {"Flowing Water",   10,  10,   10,  false, false},
    {"Flowing Water",   10,  10,   10,  false, false},
    {"Flowing Water",   10,  10,   10,  false, false},
    {"Flowing Water",   10,  10,   10,  false, false},
    {"Falling Water",   10,  10,   10,  false, false},
    {"Flowing Lava",    23,  23,   23,  false, false},
    {"Flowing Lava",    23,  23,   23,  false, false},
    {"Flowing Lava",    23,  23,   23,  false, false},
    {"Flowing Lava",    23,  23,   23,  false, false},
    {"Flowing Lava",    23,  23,   23,  false, false},
    {"Flowing Lava",    23,  23,   23,  false, false},
    {"Flowing Lava",    23,  23,   23,  false, false},
    {"Falling Lava",    23,  23,   23,  false, false},
};

}  // namespace

const BlockDef& blockDef(BlockId id) {
    unsigned int i = static_cast<unsigned int>(id);
    if (i >= static_cast<unsigned int>(BLOCK_COUNT)) {
        i = 0;
    }
    return kDefs[i];
}

bool isOpaque(BlockId id) { return blockDef(id).opaque; }
bool isSolid(BlockId id) { return blockDef(id).solid; }

bool isWater(BlockId id) {
    return id == BLOCK_WATER || (id >= BLOCK_WATER_FLOW_1 && id <= BLOCK_WATER_FALLING);
}

bool isLava(BlockId id) {
    return id == BLOCK_LAVA || (id >= BLOCK_LAVA_FLOW_1 && id <= BLOCK_LAVA_FALLING);
}

FluidKind fluidKind(BlockId id) {
    if (isWater(id)) {
        return FluidKind::Water;
    }
    if (isLava(id)) {
        return FluidKind::Lava;
    }
    return FluidKind::None;
}

bool isLiquid(BlockId id) { return fluidKind(id) != FluidKind::None; }

int fluidLevel(BlockId id) {
    if (id == BLOCK_WATER || id == BLOCK_LAVA) {
        return 0;
    }
    if (id >= BLOCK_WATER_FLOW_1 && id <= BLOCK_WATER_FLOW_7) {
        return 1 + (id - BLOCK_WATER_FLOW_1);
    }
    if (id == BLOCK_WATER_FALLING) {
        return 8;
    }
    if (id >= BLOCK_LAVA_FLOW_1 && id <= BLOCK_LAVA_FLOW_7) {
        return 1 + (id - BLOCK_LAVA_FLOW_1);
    }
    if (id == BLOCK_LAVA_FALLING) {
        return 8;
    }
    return -1;
}

bool isFluidFlow(BlockId id) { return fluidLevel(id) > 0; }

BlockId fluidBlock(FluidKind kind, int level) {
    if (kind == FluidKind::Water) {
        if (level <= 0) {
            return BLOCK_WATER;
        }
        if (level <= 7) {
            return static_cast<BlockId>(static_cast<int>(BLOCK_WATER_FLOW_1) + level - 1);
        }
        return BLOCK_WATER_FALLING;
    }
    if (kind == FluidKind::Lava) {
        if (level <= 0) {
            return BLOCK_LAVA;
        }
        if (level <= 7) {
            return static_cast<BlockId>(static_cast<int>(BLOCK_LAVA_FLOW_1) + level - 1);
        }
        return BLOCK_LAVA_FALLING;
    }
    return BLOCK_AIR;
}

float fluidHeight(BlockId id) {
    const int level = fluidLevel(id);
    if (level <= 0 || level >= 8) {
        return 1.0f;
    }
    return 1.0f - static_cast<float>(level) * 0.125f;
}

}  // namespace aimc
