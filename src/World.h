#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "BlockShape.h"
#include "Chunk.h"
#include "Chest.h"
#include "Furnace.h"
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
    float dist = 0.0f;     // Abstand vom Strahlursprung bis zum Eintritt in den Block
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

    const TerrainGenerator& terrain() const { return terrain_; }

    // Zugriff
    BlockId getBlock(int x, int y, int z) const;  // AIR ausserhalb / ungeladen
    // Kollisionsboxen der Zelle (0..1 relativ zur Zelle); ungeladene Chunks und y < 0 zaehlen als fest
    int collisionBoxes(int x, int y, int z, Box* out) const;
    bool isSolidAt(int x, int y, int z) const;    // ungeladene Chunks gelten als fest (Spieler faellt nicht durch)
    bool isChunkLoaded(int cx, int cz) const { return find(cx, cz) != nullptr; }
    bool setBlock(int x, int y, int z, BlockId b);

    // Blocklicht (Fackeln, Lava, brennende Oefen) 0..15
    int lightAt(int x, int y, int z) const;
    // Fackeln, deren Traeger entfernt wurde (die Fackel ist schon weg, der Spieler holt die Gegenstaende ab)
    std::vector<glm::ivec3> takeTorchDrops() {
        std::vector<glm::ivec3> out;
        out.swap(torchDrops_);
        return out;
    }

    // Tuer/Tor und alle gleichartigen Nachbarn oeffnen bzw. schliessen (grosse Tore). Rueckgabe: Anzahl.
    int toggleOpenGroup(int x, int y, int z);

    // Pflanzen, deren Boden entfernt wurde (die Pflanze ist schon weg; Ort und Blockart zum Abholen der Beute)
    struct PlantDrop {
        glm::ivec3 pos;
        BlockId block;
    };
    std::vector<PlantDrop> takePlantDrops() {
        std::vector<PlantDrop> out;
        out.swap(plantDrops_);
        return out;
    }

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

    // Oefen (Zustand je Block, wird beim Speichern mitgeschrieben)
    Furnace& furnaceAt(int x, int y, int z);  // legt bei Bedarf an
    Furnace* findFurnace(int x, int y, int z);
    bool removeFurnace(int x, int y, int z, Furnace* out);
    size_t furnaceCount() const { return furnaces_.size(); }
    void exportFurnaces(std::vector<FurnaceSave>& out) const;
    void importFurnaces(const std::vector<FurnaceSave>& in);
    // Truhen
    Chest& chestAt(int x, int y, int z);  // legt bei Bedarf an
    Chest* findChest(int x, int y, int z);
    bool removeChest(int x, int y, int z, Chest* out);
    size_t chestCount() const { return chests_.size(); }
    // Schilder: Text je Schild (leerer Text entfernt den Eintrag, der Eintrag verschwindet auch mit dem Block)
    const SignText* findSign(int x, int y, int z) const;
    void setSign(int x, int y, int z, const SignText& t);
    void exportSigns(std::vector<SignSave>& out) const;
    void importSigns(const std::vector<SignSave>& in);
    // Dampfmaschinen: Zustand je Block, Netze werden aus den Nachbarbloecken berechnet (siehe Machines.cpp)
    Machine* findMachine(int x, int y, int z);
    Machine& machineAt(int x, int y, int z);  // legt bei Bedarf an (der Block muss eine Maschine sein)
    bool removeMachine(int x, int y, int z, Machine* out);
    size_t machineCount() const { return machines_.size(); }
    // Mechanische Versorgung 0..1 am Block (Welle, Motor, Bohrer, Foerderband, Lift); 0 ohne laufenden Motor
    float powerAt(int x, int y, int z) const;
    // Pumpe: Quellblock (Wasser) in der Naehe, aus dem sie saugt (Pipeline.cpp)
    bool pumpIntake(const glm::ivec3& pump, glm::ivec3& out) const;
    // Naechster abbaubarer Block vor einem Bohrer an pos in Richtung facing (0 +X, 1 -X, 2 +Z, 3 -Z, 4 oben, 5 unten), sonst false
    // Foerderband unter den Fuessen und Dampflift am Koerper: Bandgeschwindigkeit (x, z) und Liftgeschwindigkeit (y, 0 = kein Lift)
    struct MechFlow {
        glm::vec3 belt{0.0f};
        float lift = 0.0f;
        bool inLift = false;  // im Liftschacht (auch ohne Strom)
    };
    MechFlow mechFlow(const glm::vec3& feet) const;
    bool drillTarget(const glm::ivec3& pos, int facing, glm::ivec3& out) const;
    std::vector<MachineDrop> takeMachineDrops() {
        std::vector<MachineDrop> out;
        out.swap(machineDrops_);
        return out;
    }
    void exportMachines(std::vector<MachineSave>& out) const;
    void importMachines(const std::vector<MachineSave>& in);
    void exportChests(std::vector<ChestSave>& out) const;
    void importChests(const std::vector<ChestSave>& in);

    // liquids = true: auch Wasser und Lava werden getroffen (fuer Eimer)
    RayHit raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist, bool liquids = false) const;

    const std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash>& chunks() const { return chunks_; }
    const Chunk* findChunk(int cx, int cz) const { return find(cx, cz); }

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
    // Licht
    void setLight(int x, int y, int z, uint8_t v);
    void propagateLight(std::vector<glm::ivec3>& queue);
    void removeLight(int x, int y, int z);
    void relightAfterChange(int x, int y, int z, BlockId oldB, BlockId newB);
    void lightChunkInit(Chunk& c, bool fullScan);
    void checkTorchSupports(int x, int y, int z);
    void scheduleFluid(int x, int y, int z);
    void updateFluidCell(int kind, int x, int y, int z);
    BlockId computeFluid(int kind, int x, int y, int z) const;
    void reactFluids(int x, int y, int z);
    void tickCrops(float dt);
    float cropTimer_ = 0.0f;
    uint32_t cropRng_ = 0x2545F491u;
    void tickFurnaces(float dt);  // Wasser trifft Lava: Obsidian bzw. Bruchstein
    bool inFluidAt(const glm::vec3& p, bool lava) const;
    BlockId blockOrSolid(int x, int y, int z) const;  // ungeladen/unten = Stein

    TerrainGenerator terrain_;
    std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash> chunks_;
    std::unordered_map<ChunkKey, std::vector<BlockId>, ChunkKeyHash> stored_;  // entladene, veraenderte Chunks
    std::vector<BlockId> padded_;
    std::vector<uint8_t> paddedLight_;
    std::vector<glm::ivec3> torchDrops_;
    std::vector<PlantDrop> plantDrops_;
    bool lightUrgent_ = false;
    bool meshing_ = true;
    struct FluidQueue {
        std::vector<glm::ivec3> pending;
        std::unordered_set<uint64_t> set;
        float accum = 0.0f;
    };
    FluidQueue fluid_[2];  // 0 = Wasser, 1 = Lava
    struct FurnaceEntry {
        glm::ivec3 pos;
        Furnace f;
    };
    std::unordered_map<uint64_t, FurnaceEntry> furnaces_;
    struct ChestEntry {
        glm::ivec3 pos;
        Chest c;
    };
    std::unordered_map<uint64_t, ChestEntry> chests_;
    struct SignEntry {
        glm::ivec3 pos;
        SignText text;
    };
    std::unordered_map<uint64_t, SignEntry> signs_;
    struct MachineEntry {
        glm::ivec3 pos;
        Machine m;
    };
    std::unordered_map<uint64_t, MachineEntry> machines_;
    std::unordered_map<uint64_t, float> power_;  // mechanische Versorgung je Block des Antriebsnetzes
    std::vector<MachineDrop> machineDrops_;
    float machineAccum_ = 0.0f;
    void tickMachines(float dt);
    void stepMachines(float dt);
    // Wasserleitung (Pipeline.cpp)
    struct WaterNet {
        std::vector<MachineEntry*> pumps;
        std::vector<glm::ivec3> boilers, outlets;
    };
    std::vector<WaterNet> waterNets_;
    bool isInfiniteSource(const glm::ivec3& p) const;
    void planPipelines(const std::vector<MachineEntry*>& list);
    void runPipelines(float dt);
};

}  // namespace oc
