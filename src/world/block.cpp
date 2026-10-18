#include "world/block.h"

namespace aimc {
namespace {

const BlockDef kDefs[BLOCK_COUNT] = {
    // name       top bottom side  solid  opaque
    {"air",         0,   0,    0,  false, false},
    {"grass",       0,   2,    1,  true,  true},   // tile 0 grass_top, 1 grass_side, 2 dirt
    {"dirt",        2,   2,    2,  true,  true},
    {"stone",       3,   3,    3,  true,  true},
    {"sand",        4,   4,    4,  true,  true},
    {"log",         6,   6,    5,  true,  true},   // tile 5 log_side, 6 log_top
    {"leaves",      7,   7,    7,  true,  true},
    {"planks",      8,   8,    8,  true,  true},
    {"water",      10,  10,   10,  false, false},
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
