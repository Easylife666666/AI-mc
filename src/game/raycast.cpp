#include "game/raycast.h"

#include <cmath>
#include <limits>

namespace aimc {

RayHit raycastVoxel(const std::function<BlockId(int, int, int)>& getBlock,
                    const glm::vec3& origin, const glm::vec3& direction, float maxDistance) {
    RayHit result;
    if (glm::length(direction) < 1.0e-6f) {
        return result;
    }

    const glm::vec3 dir = glm::normalize(direction);
    glm::ivec3 cell(static_cast<int>(std::floor(origin.x)), static_cast<int>(std::floor(origin.y)),
                    static_cast<int>(std::floor(origin.z)));

    glm::ivec3 step(0);
    glm::vec3 tMax(std::numeric_limits<float>::infinity());
    glm::vec3 tDelta(std::numeric_limits<float>::infinity());

    for (int axis = 0; axis < 3; ++axis) {
        if (dir[axis] > 1.0e-6f) {
            step[axis] = 1;
            tDelta[axis] = 1.0f / dir[axis];
            tMax[axis] = (static_cast<float>(cell[axis] + 1) - origin[axis]) / dir[axis];
        } else if (dir[axis] < -1.0e-6f) {
            step[axis] = -1;
            tDelta[axis] = -1.0f / dir[axis];
            tMax[axis] = (static_cast<float>(cell[axis]) - origin[axis]) / dir[axis];
        }
    }

    glm::ivec3 normal(0);
    float distance = 0.0f;

    for (int guard = 0; guard < 1024; ++guard) {
        if (getBlock(cell.x, cell.y, cell.z) != BLOCK_AIR) {
            result.hit = true;
            result.block = cell;
            result.normal = normal;
            result.distance = distance;
            return result;
        }

        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            cell.x += step.x;
            distance = tMax.x;
            tMax.x += tDelta.x;
            normal = glm::ivec3(-step.x, 0, 0);
        } else if (tMax.y < tMax.z) {
            cell.y += step.y;
            distance = tMax.y;
            tMax.y += tDelta.y;
            normal = glm::ivec3(0, -step.y, 0);
        } else {
            cell.z += step.z;
            distance = tMax.z;
            tMax.z += tDelta.z;
            normal = glm::ivec3(0, 0, -step.z);
        }

        if (distance > maxDistance) {
            break;
        }
    }
    return result;
}

}  // namespace aimc
