#pragma once

#include <functional>

#include <glm/glm.hpp>

#include "world/block.h"

namespace aimc {

struct RayHit {
    bool hit = false;
    glm::ivec3 block{0};
    glm::ivec3 normal{0};
    float distance = 0.0f;
};

// Amanatides & Woo voxel traversal. The getter may filter blocks (e.g. treat
// water as air for interaction).
RayHit raycastVoxel(const std::function<BlockId(int, int, int)>& getBlock,
                    const glm::vec3& origin, const glm::vec3& direction, float maxDistance);

}  // namespace aimc
