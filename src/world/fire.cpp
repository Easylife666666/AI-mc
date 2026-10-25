#include "world/fire.h"

#include "world/block.h"
#include "world/world.h"

namespace aimc {

bool placeFire(World& world, const glm::ivec3& pos) {
    if (world.getBlock(pos.x, pos.y, pos.z) != BLOCK_AIR) {
        return false;
    }
    if (!isSolid(world.getBlock(pos.x, pos.y - 1, pos.z))) {
        return false;
    }
    if (!world.setBlock(pos.x, pos.y, pos.z, BLOCK_FIRE)) {
        return false;
    }
    world.scheduleFire(pos.x, pos.y, pos.z);
    return true;
}

}  // namespace aimc
