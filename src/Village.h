#pragma once
#include <cstdint>
#include <map>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Items.h"

namespace oc {

class TerrainGenerator;
struct Chunk;

// Dorf: deterministisch aus Seed und Rasterzelle, so passen Gebaeude ueber Chunkgrenzen hinweg zusammen.
struct VillageBuilding {
    enum Kind : uint8_t { House, Library, Smithy, Well, Farm, Barn, Market, Tower, GateHouse, Church } kind = House;
    int x0 = 0, z0 = 0, x1 = 0, z1 = 0;  // Grundflaeche (einschliesslich)
    int groundY = 0;                     // Hoehe des Fussbodens
    int facing = 0;                      // Tuerseite: 0 +X, 1 -X, 2 +Z, 3 -Z
    glm::ivec2 door{0, 0};               // Zelle direkt vor der Tuer (draussen)

    // Zelle im Gebaeude: u = Tiefe von der Tuerwand aus (0 = Tuerwand), w = Abstand von der Mittellinie quer dazu
    glm::ivec2 cell(int u, int w) const {
        const int xc = (x0 + x1) / 2, zc = (z0 + z1) / 2;
        switch (facing) {
            case 0: return {x1 - u, zc + w};
            case 1: return {x0 + u, zc + w};
            case 2: return {xc + w, z1 - u};
            default: return {xc + w, z0 + u};
        }
    }
    int depth() const { return facing <= 1 ? x1 - x0 + 1 : z1 - z0 + 1; }
    int width() const { return facing <= 1 ? z1 - z0 + 1 : x1 - x0 + 1; }
};

struct Village {
    int id = 0;                          // eindeutig (aus der Zelle)
    glm::ivec2 center{0, 0};
    int groundY = 0;                     // Hoehe am Brunnen
    std::vector<VillageBuilding> buildings;  // erstes Element: der Brunnen
    std::vector<glm::ivec3> pathCells;   // x, Hoehe der Oberflaeche, z
    std::vector<glm::ivec3> chests;      // Truhen (Position)
    struct Bed {
        glm::ivec3 foot, head;  // Zellen der beiden Haelften
        int building = 0;       // Index in buildings (Haus)
    };
    std::vector<Bed> beds;
    std::vector<glm::ivec3> villagerSpawns;  // Fusspunkt, an dem Dorfbewohner erscheinen
    struct AnimalSpawn {
        glm::ivec3 pos;
        int type;  // 0 Schwein, 1 Kuh, 2 Schaf
    };
    std::vector<AnimalSpawn> animalSpawns;  // Tiere im Stall
    int minX = 0, maxX = 0, minZ = 0, maxZ = 0;  // Umriss inkl. Rand

    int houseCount() const;
    // Arbeitsplatz (Zelle vor der Tuer) fuer einen Beruf, sonst am Brunnen
    glm::ivec3 workSpot(int profession) const;
};

class VillageGenerator {
public:
    static constexpr int CELL = 160;     // Rasterweite in Bloecken
    static constexpr int RADIUS = 40;    // groesster Abstand eines Gebaeudes vom Zentrum

    VillageGenerator(const TerrainGenerator& terrain, int32_t seed) : terrain_(terrain), seed_(seed) {}

    // Dorf der Rasterzelle (nullptr, wenn dort keines liegt)
    const Village* villageInCell(int cellX, int cellZ) const;
    // Alle Doerfer, deren Umriss das Rechteck beruehrt
    std::vector<const Village*> villagesIn(int minX, int minZ, int maxX, int maxZ) const;
    // Naechstes Dorf zu (x, z) innerhalb der angegebenen Zellsuchweite, sonst nullptr
    const Village* nearest(int x, int z, int cellRadius) const;
    // Schreibt die Teile aller Doerfer, die in den Chunk fallen
    void placeInChunk(Chunk& chunk) const;
    // Inhalt einer Dorftruhe (deterministisch)
    struct Loot {
        ItemId item;
        int count;
    };
    std::vector<Loot> lootFor(const Village& v, const glm::ivec3& chest) const;
    bool isVillageChest(int x, int y, int z) const;
    // Liegt (x, z) im baumfreien Dorfgebiet?
    bool inArea(int x, int z) const;

private:
    Village build(int cellX, int cellZ, bool& exists) const;
    const TerrainGenerator& terrain_;
    int32_t seed_;
    mutable std::map<std::pair<int, int>, std::pair<bool, Village>> cache_;
};

}  // namespace oc
