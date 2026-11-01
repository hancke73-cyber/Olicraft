#pragma once
#include <glad/gl.h>

#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace oc {

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void release();
    bool build(const char* vertexSrc, const char* fragmentSrc, const char* debugName);
    void use() const { glUseProgram(program_); }

    void setInt(const char* name, int v) const;
    void setFloat(const char* name, float v) const;
    void setVec2(const char* name, const glm::vec2& v) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setVec4(const char* name, const glm::vec4& v) const;
    void setMat4(const char* name, const glm::mat4& m) const;

private:
    GLint loc(const char* name) const;
    GLuint program_ = 0;
    mutable std::unordered_map<std::string, GLint> cache_;
};

}  // namespace oc
