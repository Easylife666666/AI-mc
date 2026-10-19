#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>

#include <glm/glm.hpp>

#include "game/creative.h"

namespace aimc {

struct SaveData {
    int seed = 0;
    glm::vec3 playerPos{0.5f, 70.0f, 0.5f};
    float yaw = -90.0f;
    float pitch = -18.0f;
    int selected = 0;
    float timeOfDay = 0.42f;
    std::array<BlockId, kHotbarSlots> hotbar = defaultHotbar();
    std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> edits;
};

// zlib-compressed binary save. Only player edits are stored; terrain is
// regenerated deterministically from the seed. Version 1 files (no hotbar) are
// still accepted.
bool saveWorld(const std::string& path, const SaveData& data);
bool loadWorld(const std::string& path, SaveData& data);

}  // namespace aimc
