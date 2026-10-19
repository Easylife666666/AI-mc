#include "world/dimension.h"

#include "core/math.h"

namespace aimc {

const char* dimensionName(Dimension dim) {
    switch (dim) {
    case Dimension::Overworld:
        return "Overworld";
    case Dimension::Nether:
        return "Nether";
    case Dimension::Count:
        break;
    }
    return "Overworld";
}

glm::ivec3 scaleBetweenDimensions(const glm::ivec3& pos, Dimension from, Dimension to) {
    if (from == to) {
        return pos;
    }
    if (to == Dimension::Nether) {
        return {floorDiv(pos.x, 8), pos.y, floorDiv(pos.z, 8)};
    }
    return {pos.x * 8, pos.y, pos.z * 8};
}

}  // namespace aimc
