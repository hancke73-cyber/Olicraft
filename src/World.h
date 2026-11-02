#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Chunk.h"
#include "Save.h"
#include "TerrainGen.h"

namespace oc {

struct ChunkKey {
    int32_t cx, cz;
    bool operator==(const ChunkKey& o) const { return cx == o.cx && cz == o.cz; }
};
struct ChunkKeyHash {
    size_t operator()(const ChunkKey& k) const {
        return static_cast<size_t>(static_cast<uint64_t>(static_cast<uint32_t>(k.cx)) * 73856093ull ^
                                   static_cast<uint64_t>(static_cast<uint32_t>(k.cz)) * 19349663ull);
    }
};

struct RayHit {
    bool hit = false;
    glm::ivec3 block{0};
    glm::ivec3 normal{0};  // zeigt aus dem getroffenen Block heraus
};

inline int floorDiv(int a, int b) {
    int q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

class World {
public:
    explicit World(int32_t seed);
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    int32_t seed() const { return terrain_.seed(); }

    // Zugriff
    BlockId getBlock(int x, int y, int z) const;  // AIR ausserhalb / ungeladen
    bool isSolidAt(int x, int y, int z) const;    // ungeladene Chunks gelten als fest (Spieler faellt nicht durch)
    bool isChunkLoaded(int cx, int cz) const { return find(cx, cz) != nullptr; }
    bool setBlock(int x, int y, int z, BlockId b);

    // Fluessigkeiten: Wasser fliesst in Schritten von 0.25 s, Lava deutlich langsamer (1.5 s)
    void tick(float dt);
    void scheduleFluidAround(int x, int y, int z);
    size_t pendingFluidCount() const { return fluid_[0].pending.size() + fluid_[1].pending.size(); }
    // Liegt der Punkt (Weltkoordinaten) unter der Wasser- bzw. Lavaoberflaeche?
    bool inWaterAt(const glm::vec3& p) const;
    bool inLavaAt(const glm::vec3& p) const;
    const Chunk* chunkAt(int cx, int cz) const { return find(cx, cz); }

    // Chunk-Streaming; budgetMs begrenzt die Arbeit pro Frame
    void update(const glm::vec3& center, int renderDist, double budgetMs);
    // Laedt und meshed synchron alles im Radius (Start)
    void loadAround(const glm::vec3& center, int radius);
    // Sucht einen Startpunkt an Land nahe (0,0)
    glm::vec3 findSpawn();

    RayHit raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist) const;

    const std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash>& chunks() const { return chunks_; }

    // Speichern / Laden (nur vom Spieler veraenderte Chunks)
    void exportModified(std::vector<SavedChunk>& out) const;
    void importChunks(const std::vector<SavedChunk>& in);

    // Meshing abschaltbar (fuer Tests ohne OpenGL-Kontext)
    void setMeshingEnabled(bool on) { meshing_ = on; }

    size_t loadedChunkCount() const { return chunks_.size(); }
    size_t pendingMeshCount() const;

private:
    Chunk* find(int cx, int cz) const;
    Chunk& ensureChunk(int cx, int cz);
    void meshChunk(Chunk& c);
    bool neighborsLoaded(int cx, int cz) const;
    void markDirty(int cx, int cz, bool urgent);
    bool setBlockImpl(int x, int y, int z, BlockId b, bool urgent);
    void scheduleFluid(int x, int y, int z);
    void updateFluidCell(int kind, int x, int y, int z);
    BlockId computeFluid(int kind, int x, int y, int z) const;
    void reactFluids(int x, int y, int z);  // Wasser trifft Lava: Obsidian bzw. Bruchstein
    bool inFluidAt(const glm::vec3& p, bool lava) const;
    BlockId blockOrSolid(int x, int y, int z) const;  // ungeladen/unten = Stein

    TerrainGenerator terrain_;
    std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash> chunks_;
    std::unordered_map<ChunkKey, std::vector<BlockId>, ChunkKeyHash> stored_;  // entladene, veraenderte Chunks
    std::vector<BlockId> padded_;
    bool meshing_ = true;
    struct FluidQueue {
        std::vector<glm::ivec3> pending;
        std::unordered_set<uint64_t> set;
        float accum = 0.0f;
    };
    FluidQueue fluid_[2];  // 0 = Wasser, 1 = Lava
};

}  // namespace oc
