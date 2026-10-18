#include "render/selection.h"

#include <array>

#include <GL/glew.h>
#include <spdlog/spdlog.h>

namespace aimc {
namespace {

const char* kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
uniform vec3 uOffset;
void main() {
    gl_Position = uViewProj * vec4(aPos + uOffset, 1.0);
}
)GLSL";

const char* kFragmentShader = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)GLSL";

constexpr float kInflate = 0.004f;

}  // namespace

SelectionBox::~SelectionBox() { shutdown(); }

bool SelectionBox::init() {
    if (!m_shader.compile(kVertexShader, kFragmentShader)) {
        return false;
    }

    const float lo = -kInflate;
    const float hi = 1.0f + kInflate;
    const std::array<glm::vec3, 8> corners = {{
        {lo, lo, lo}, {hi, lo, lo}, {hi, lo, hi}, {lo, lo, hi},
        {lo, hi, lo}, {hi, hi, lo}, {hi, hi, hi}, {lo, hi, hi},
    }};
    const std::array<std::array<int, 2>, 12> edges = {{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
    }};

    std::array<glm::vec3, 24> vertices{};
    for (std::size_t i = 0; i < edges.size(); ++i) {
        vertices[i * 2 + 0] = corners[static_cast<std::size_t>(edges[i][0])];
        vertices[i * 2 + 1] = corners[static_cast<std::size_t>(edges[i][1])];
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(vertices)), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
    return true;
}

void SelectionBox::shutdown() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    m_shader.destroy();
}

void SelectionBox::draw(const glm::mat4& viewProj, const glm::ivec3& block) {
    if (m_vao == 0) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    m_shader.use();
    m_shader.setMat4("uViewProj", viewProj);
    m_shader.setVec3("uOffset", glm::vec3(block));
    m_shader.setVec4("uColor", glm::vec4(0.05f, 0.05f, 0.05f, 0.85f));

    glBindVertexArray(m_vao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 24);
    glLineWidth(1.0f);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

}  // namespace aimc
