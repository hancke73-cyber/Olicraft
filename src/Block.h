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
};

const BlockInfo& blockInfo(BlockId id);

inline bool isOpaque(BlockId id) { return blockInfo(id).opaque; }
inline bool isSolid(BlockId id) { return blockInfo(id).solid; }

// Auswahl in der Hotbar
constexpr int HOTBAR_SIZE = 9;
extern const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE];

}  // namespace oc
