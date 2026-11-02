#pragma once
#include <vector>

#include "Block.h"
#include "Chunk.h"

namespace oc {

// Kopie des Chunks mit 1 Block Rand (Nachbarchunks): 18 x CHUNK_H x 18
constexpr int PAD_W = CHUNK_W + 2;
constexpr int PAD_VOLUME = PAD_W * PAD_W * CHUNK_H;
inline int padIndex(int px, int y, int pz) { return px + pz * PAD_W + y * PAD_W * PAD_W; }

class World;

struct MeshOutput {
    std::vector<Vertex> opaque;  // feste Bloecke + Laub/Glas (Cutout)
    std::vector<Vertex> water;   // transparent
    void clear() {
        opaque.clear();
        water.clear();
    }
};

// Erzeugt die Geometrie fuer einen Chunk (Positionen relativ zur Chunk-Ecke).
// light: Blocklicht im selben Layout wie padded (0..15), nullptr = dunkel.
void buildChunkMesh(const BlockId* padded, const uint8_t* light, MeshOutput& out);

}  // namespace oc
