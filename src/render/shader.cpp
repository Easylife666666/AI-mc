#include "render/shader.h"

#include <vector>

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

namespace aimc {
namespace {

GLuint compileStage(GLenum type, const char* src) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<std::size_t>(len > 1 ? len : 1), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        spdlog::error("{} shader compile failed:\n{}",
                      type == GL_VERTEX_SHADER ? "vertex" : "fragment", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

Shader::~Shader() { destroy(); }

bool Shader::compile(const char* vertexSrc, const char* fragmentSrc) {
    destroy();

    const GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc);
    if (vs == 0) {
        return false;
    }
    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fs == 0) {
        glDeleteShader(vs);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<std::size_t>(len > 1 ? len : 1), '\0');
        glGetProgramInfoLog(m_program, len, nullptr, log.data());
        spdlog::error("shader link failed:\n{}", log.data());
        destroy();
        return false;
    }
    return true;
}

void Shader::destroy() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void Shader::use() const { glUseProgram(m_program); }

void Shader::setMat4(const char* name, const glm::mat4& value) const {
    const GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }
}

void Shader::setVec3(const char* name, const glm::vec3& value) const {
    const GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glUniform3f(loc, value.x, value.y, value.z);
    }
}

void Shader::setVec2(const char* name, const glm::vec2& value) const {
    const GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glUniform2f(loc, value.x, value.y);
    }
}

void Shader::setFloat(const char* name, float value) const {
    const GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void Shader::setInt(const char* name, int value) const {
    const GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

}  // namespace aimc
