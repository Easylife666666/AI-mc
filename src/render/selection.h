#pragma once

#include <glm/glm.hpp>

#include "render/shader.h"

namespace aimc {

// Wireframe box around the block under the crosshair.
class SelectionBox {
public:
    SelectionBox() = default;
    ~SelectionBox();

    SelectionBox(const SelectionBox&) = delete;
    SelectionBox& operator=(const SelectionBox&) = delete;

    bool init();
    void shutdown();
    void draw(const glm::mat4& viewProj, const glm::ivec3& block);

private:
    Shader m_shader;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
};

}  // namespace aimc
