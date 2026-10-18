#pragma once

#include <glm/glm.hpp>

namespace aimc {

struct Frustum {
    glm::vec4 planes[6];

    static Frustum fromViewProj(const glm::mat4& viewProj);
    bool intersectsAabb(const glm::vec3& min, const glm::vec3& max) const;
};

}  // namespace aimc
