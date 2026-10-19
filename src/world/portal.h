#pragma once

#include <functional>

#include <glm/glm.hpp>

#include "world/block.h"

namespace aimc {

class World;

struct PortalFrame {
    glm::ivec3 min{0};  // interior air cells, inclusive
    glm::ivec3 max{0};
    bool normalZ = true;  // interior spans X/Y (plane normal Z) when true

    int width() const;
    int height() const;
};

using BlockGetter = std::function<BlockId(int, int, int)>;
using BlockSetter = std::function<bool(int, int, int, BlockId)>;

// Walks the air pocket starting at `start` and checks that it is a rectangle
// surrounded by an obsidian ring, like the vanilla 4x5..22x22 nether portal.
bool findPortalFrame(const BlockGetter& get, const glm::ivec3& start, PortalFrame& out);

// Fills the interior of the frame with portal blocks. Returns false when there
// is no valid frame.
bool fillPortal(const BlockGetter& get, const BlockSetter& set, const glm::ivec3& start,
                PortalFrame& out);

bool lightPortal(World& world, const glm::ivec3& start);

inline bool isPortalBlock(BlockId id) {
    return id == BLOCK_PORTAL_X || id == BLOCK_PORTAL_Z;
}

// Finds the lowest portal block within a square radius, used when linking two
// dimensions.
bool findPortalNear(const World& world, const glm::ivec3& target, int radius,
                    glm::ivec3& out);

// Builds a 4x5 portal on the ground at the target column, carving a small room
// if needed. Returns the feet position inside the new portal.
bool buildPortalAt(World& world, const glm::ivec3& target, glm::vec3& spawn);

}  // namespace aimc
