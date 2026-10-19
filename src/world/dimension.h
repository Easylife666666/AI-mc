#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace aimc {

enum class Dimension : std::uint8_t {
    Overworld = 0,
    Nether = 1,
    Count,
};

constexpr int kDimensionCount = 2;

const char* dimensionName(Dimension dim);

// Vanilla link ratio: one nether block equals eight overworld blocks on X/Z.
glm::ivec3 scaleBetweenDimensions(const glm::ivec3& pos, Dimension from, Dimension to);

}  // namespace aimc
