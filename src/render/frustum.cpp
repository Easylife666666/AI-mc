#include "render/frustum.h"

#include <cmath>

namespace aimc {
namespace {

glm::vec4 normalizePlane(glm::vec4 plane) {
    const float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
    if (length > 0.0f) {
        plane /= length;
    }
    return plane;
}

}  // namespace

Frustum Frustum::fromViewProj(const glm::mat4& m) {
    // glm is column-major: m[column][row].
    const glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
    const glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
    const glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
    const glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

    Frustum frustum;
    frustum.planes[0] = normalizePlane(row3 + row0);  // left
    frustum.planes[1] = normalizePlane(row3 - row0);  // right
    frustum.planes[2] = normalizePlane(row3 + row1);  // bottom
    frustum.planes[3] = normalizePlane(row3 - row1);  // top
    frustum.planes[4] = normalizePlane(row3 + row2);  // near
    frustum.planes[5] = normalizePlane(row3 - row2);  // far
    return frustum;
}

bool Frustum::intersectsAabb(const glm::vec3& min, const glm::vec3& max) const {
    for (const glm::vec4& plane : planes) {
        const glm::vec3 positive(plane.x >= 0.0f ? max.x : min.x,
                                 plane.y >= 0.0f ? max.y : min.y,
                                 plane.z >= 0.0f ? max.z : min.z);
        if (plane.x * positive.x + plane.y * positive.y + plane.z * positive.z + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

}  // namespace aimc
