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

}  // namespace aimc
