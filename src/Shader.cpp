#include "Shader.h"

#include <cstdio>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace oc {

static GLuint compile(GLenum type, const char* src, const char* name) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1, 0);
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::fprintf(stderr, "Shader-Fehler (%s, %s):\n%s\n", name, type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                     log.data());
        glDeleteShader(s);
        return 0;
    }
    return s;
}

Shader::~Shader() { release(); }

void Shader::release() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
    cache_.clear();
}

bool Shader::build(const char* vertexSrc, const char* fragmentSrc, const char* debugName) {
    GLuint vs = compile(GL_VERTEX_SHADER, vertexSrc, debugName);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc, debugName);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1, 0);
        glGetProgramInfoLog(program_, len, nullptr, log.data());
        std::fprintf(stderr, "Link-Fehler (%s):\n%s\n", debugName, log.data());
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    return true;
}

GLint Shader::loc(const char* name) const {
    auto it = cache_.find(name);
    if (it != cache_.end()) return it->second;
    GLint l = glGetUniformLocation(program_, name);
    cache_.emplace(name, l);
    return l;
}

void Shader::setInt(const char* n, int v) const { glUniform1i(loc(n), v); }
void Shader::setFloat(const char* n, float v) const { glUniform1f(loc(n), v); }
void Shader::setVec2(const char* n, const glm::vec2& v) const { glUniform2fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::setVec3(const char* n, const glm::vec3& v) const { glUniform3fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::setVec4(const char* n, const glm::vec4& v) const { glUniform4fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::setMat4(const char* n, const glm::mat4& m) const {
    glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(m));
}

}  // namespace oc
