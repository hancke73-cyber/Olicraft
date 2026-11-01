#pragma once
#include <glad/gl.h>

#include <array>
#include <cstdint>
#include <vector>

#include "Block.h"

namespace oc {

struct Vertex {
    float x, y, z;
    float u, v;
    float shade;  // Flaechenhelligkeit * Ambient Occlusion
    float sky;    // Himmelslicht-Anteil 0..1
};

// Maximale Quads pro Mesh (gemeinsamer Index-Puffer)
constexpr uint32_t MAX_QUADS_PER_MESH = 131072;

GLuint sharedQuadIndexBuffer();       // legt den Index-Puffer bei Bedarf an
void destroySharedQuadIndexBuffer();  // vor dem Zerstoeren des GL-Kontexts aufrufen

class GpuMesh {
public:
    GpuMesh() = default;
    ~GpuMesh() { release(); }
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    void upload(const std::vector<Vertex>& verts);
    void release();
    void draw() const;
    bool empty() const { return indexCount_ == 0; }
    uint32_t quadCount() const { return indexCount_ / 6; }

private:
    GLuint vao_ = 0, vbo_ = 0;
    GLsizei indexCount_ = 0;
};

struct Chunk {
    static constexpr int VOLUME = CHUNK_W * CHUNK_W * CHUNK_H;

    Chunk(int cx_, int cz_) : cx(cx_), cz(cz_), blocks(VOLUME, AIR) {}

    static int index(int x, int y, int z) { return x + z * CHUNK_W + y * CHUNK_W * CHUNK_W; }
    BlockId get(int x, int y, int z) const { return blocks[index(x, y, z)]; }
    void set(int x, int y, int z, BlockId b) { blocks[index(x, y, z)] = b; }

    int cx, cz;
    std::vector<BlockId> blocks;

    bool modified = false;  // vom Spieler veraendert -> wird gespeichert
    bool dirty = true;      // Mesh muss neu gebaut werden
    bool urgent = false;    // sofort neu bauen (Spieleraktion)
    bool meshed = false;

    GpuMesh opaque;
    GpuMesh water;
};

}  // namespace oc
