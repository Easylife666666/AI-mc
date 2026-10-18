#pragma once

#include <glm/glm.hpp>

namespace aimc {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool compile(const char* vertexSrc, const char* fragmentSrc);
    void destroy();
    void use() const;

    unsigned int id() const { return m_program; }

    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec4(const char* name, const glm::vec4& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setVec2(const char* name, const glm::vec2& value) const;
    void setFloat(const char* name, float value) const;
    void setInt(const char* name, int value) const;

private:
    unsigned int m_program = 0;
};

}  // namespace aimc
