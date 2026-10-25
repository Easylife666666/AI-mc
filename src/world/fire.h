#pragma once

#include <glm/glm.hpp>

namespace aimc {

class World;

// Places fire at pos when the cell is air and the block below is solid, and
// schedules it to burn out. Returns false when the spot is not valid.
bool placeFire(World& world, const glm::ivec3& pos);

}  // namespace aimc
