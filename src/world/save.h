#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>

#include <glm/glm.hpp>

#include "game/creative.h"
#include "world/dimension.h"

namespace aimc {

struct DimensionSave {
    glm::vec3 playerPos{0.5f, 70.0f, 0.5f};
    float yaw = -90.0f;
    float pitch = -18.0f;
    std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> edits;
};

struct SaveData {
    int seed = 0;
    float timeOfDay = 0.42f;
    int selected = 0;
    std::array<ItemRef, kHotbarSlots> hotbar = defaultHotbar();
    Dimension current = Dimension::Overworld;
    std::array<DimensionSave, kDimensionCount> dimensions;
};

// zlib-compressed binary save. Only player edits are stored; terrain is
// regenerated deterministically from the seed. Files from v1 (no hotbar),
// v2 (blocks only) and v3 (items, single dimension) still load into the
// overworld.
bool saveWorld(const std::string& path, const SaveData& data);
bool loadWorld(const std::string& path, SaveData& data);

}  // namespace aimc
