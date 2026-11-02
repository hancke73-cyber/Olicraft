#include "Chunk.h"

#include <cstddef>
#include <vector>

namespace oc {

static GLuint g_quadEbo = 0;

GLuint sharedQuadIndexBuffer() {
    if (g_quadEbo) return g_quadEbo;
    std::vector<uint32_t> idx(static_cast<size_t>(MAX_QUADS_PER_MESH) * 6);
    for (uint32_t q = 0; q < MAX_QUADS_PER_MESH; ++q) {
        uint32_t b = q * 4, i = q * 6;
        idx[i + 0] = b + 0;
        idx[i + 1] = b + 1;
        idx[i + 2] = b + 2;
        idx[i + 3] = b + 0;
        idx[i + 4] = b + 2;
        idx[i + 5] = b + 3;
    }
    glGenBuffers(1, &g_quadEbo);
    // Die Bindung des EBO gehoert zum VAO; hier nur ueber einen kurzlebigen Binding-Punkt befuellen.
    glBindBuffer(GL_COPY_WRITE_BUFFER, g_quadEbo);
    glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(uint32_t)), idx.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    return g_quadEbo;
}

void destroySharedQuadIndexBuffer() {
    if (g_quadEbo) {
        glDeleteBuffers(1, &g_quadEbo);
        g_quadEbo = 0;
    }
}

void GpuMesh::upload(const std::vector<Vertex>& verts) {
    size_t quads = verts.size() / 4;
    if (quads > MAX_QUADS_PER_MESH) quads = MAX_QUADS_PER_MESH;
    if (quads == 0) {
        release();
        return;
    }
    if (!vao_) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedQuadIndexBuffer());
        const GLsizei stride = sizeof(Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, u)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, shade)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, sky)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, light)));
    } else {
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    }
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(quads * 4 * sizeof(Vertex)), verts.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    indexCount_ = static_cast<GLsizei>(quads * 6);
}

void GpuMesh::release() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    vao_ = vbo_ = 0;
    indexCount_ = 0;
}

void GpuMesh::draw() const {
    if (!vao_ || indexCount_ == 0) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

}  // namespace oc
