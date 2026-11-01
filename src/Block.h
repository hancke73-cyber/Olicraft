#pragma once
#include <cstdint>

namespace oc {

// Weltdimensionen
constexpr int CHUNK_W = 16;
constexpr int CHUNK_H = 128;
constexpr int SEA_LEVEL = 62;

using BlockId = uint8_t;

enum Block : BlockId {
    AIR = 0,
    GRASS,
    DIRT,
    STONE,
    SAND,
    WATER,
    LOG,
    LEAVES,
    PLANKS,
    COBBLE,
    BEDROCK,
    GLASS,
    SNOW,
    // Fliessendes Wasser: WATER ist die Quelle (Stufe 0), WATER_1..7 sind Stroemungsstufen, WATER_FALL faellt.
    WATER_1,
    WATER_2,
    WATER_3,
    WATER_4,
    WATER_5,
    WATER_6,
    WATER_7,
    WATER_FALL,
    BLOCK_COUNT
};

// Kacheln im Texturatlas (16x16 Kacheln je 16x16 Pixel)
enum Tile : int {
    TILE_GRASS_TOP = 0,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_SAND,
    TILE_WATER,
    TILE_LOG_SIDE,
    TILE_LOG_TOP,
    TILE_LEAVES,
    TILE_PLANKS,
    TILE_COBBLE,
    TILE_BEDROCK,
    TILE_GLASS,
    TILE_SNOW,
    TILE_COUNT
};

struct BlockInfo {
    const char* name;
    bool solid;      // Kollision
    bool opaque;     // verdeckt Nachbarflaechen, wirft Ambient Occlusion
    bool liquid;
    bool blocksSky;  // blockiert Himmelslicht (fuer die einfache Beleuchtung)
    bool cullSame;   // Flaechen zwischen gleichen Bloecken weglassen
    bool breakable;
    Tile top, bottom, side;
    float hardness;  // Abbauzeit in Sekunden (Survival, mit blossen Haenden)
    BlockId drop;    // Block, der beim Abbauen faellt (AIR = nichts)
};

const BlockInfo& blockInfo(BlockId id);

inline bool isOpaque(BlockId id) { return blockInfo(id).opaque; }
inline bool isSolid(BlockId id) { return blockInfo(id).solid; }
inline bool isLiquid(BlockId id) { return id == WATER || (id >= WATER_1 && id <= WATER_FALL); }
// Stroemungsstufe 1..7 (WATER_1..WATER_7), sonst 0
inline int flowLevel(BlockId id) { return (id >= WATER_1 && id <= WATER_7) ? id - WATER_1 + 1 : 0; }
// Hoehe der Wasseroberflaeche in einem Block (0..1), ohne Beruecksichtigung von Wasser darueber
inline float liquidHeight(BlockId id) {
    if (id == WATER_FALL) return 1.0f;
    if (id == WATER) return 8.0f / 9.0f;
    int l = flowLevel(id);
    return l ? static_cast<float>(8 - l) / 9.0f : 0.0f;
}

// Auswahl in der Hotbar
constexpr int HOTBAR_SIZE = 9;
extern const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE];

}  // namespace oc
