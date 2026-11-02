#pragma once
#include <cstdint>

namespace oc {

// Weltdimensionen
constexpr int CHUNK_W = 16;
constexpr int CHUNK_H = 128;
constexpr int SEA_LEVEL = 62;
constexpr int LAVA_LEVEL = 10;  // Hoehlen bis zu dieser Hoehe sind mit Lava gefuellt

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
    // Lava: LAVA ist die Quelle, LAVA_1..3 Stroemungsstufen (kuerzere Reichweite als Wasser), LAVA_FALL faellt.
    LAVA,
    LAVA_1,
    LAVA_2,
    LAVA_3,
    LAVA_FALL,
    OBSIDIAN,
    CRAFTING_TABLE,
    COAL_ORE,
    IRON_ORE,
    DIAMOND_ORE,
    FURNACE,
    FURNACE_LIT,
    GOLD_ORE,
    // Fackeln: TORCH steht auf dem Boden, die vier anderen haengen an einer Wand (Suffix = Seite des Traegerblocks)
    TORCH,
    TORCH_PX,
    TORCH_NX,
    TORCH_PZ,
    TORCH_NZ,
    // Bausteine
    BRICKS,
    STONE_BRICKS,
    SANDSTONE,
    GRAVEL,
    CLAY,
    WOOL_WHITE,
    WOOL_RED,
    WOOL_BLUE,
    WOOL_GREEN,
    WOOL_YELLOW,
    WOOL_BLACK,
    BOOKSHELF,
    IRON_BLOCK,
    GOLD_BLOCK,
    DIAMOND_BLOCK,
    COAL_BLOCK,
    LAMP,
    PUMPKIN,
    CACTUS,
    BIRCH_LOG,
    BIRCH_LEAVES,
    SPRUCE_LOG,
    SPRUCE_LEAVES,
    CHEST,
    BED,
    // Pflanzen: kreuzfoermig, begehbar, brechen sofort und fallen ab, wenn der Boden fehlt
    TALL_GRASS,
    FERN,
    FLOWER_RED,
    FLOWER_YELLOW,
    FLOWER_BLUE,
    FLOWER_WHITE,
    MUSHROOM_RED,
    MUSHROOM_BROWN,
    DEAD_BUSH,
    SUGAR_CANE,
    BERRY_BUSH,
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
    TILE_MEAT,
    TILE_LAVA,
    TILE_OBSIDIAN,
    TILE_CRAFT_TOP,
    TILE_CRAFT_SIDE,
    TILE_STICK,
    TILE_PICKAXE_WOOD,
    TILE_AXE_WOOD,
    TILE_SHOVEL_WOOD,
    TILE_SWORD_WOOD,
    TILE_PICKAXE_STONE,
    TILE_AXE_STONE,
    TILE_SHOVEL_STONE,
    TILE_SWORD_STONE,
    TILE_COAL_ORE,
    TILE_IRON_ORE,
    TILE_DIAMOND_ORE,
    TILE_FURNACE,
    TILE_FURNACE_LIT,
    TILE_FURNACE_TOP,
    TILE_COAL,
    TILE_IRON_INGOT,
    TILE_DIAMOND,
    TILE_COOKED_MEAT,
    TILE_PICKAXE_IRON,
    TILE_AXE_IRON,
    TILE_SHOVEL_IRON,
    TILE_SWORD_IRON,
    TILE_PICKAXE_DIAMOND,
    TILE_AXE_DIAMOND,
    TILE_SHOVEL_DIAMOND,
    TILE_SWORD_DIAMOND,
    TILE_GOLD_ORE,
    TILE_GOLD_INGOT,
    TILE_PICKAXE_GOLD,
    TILE_AXE_GOLD,
    TILE_SHOVEL_GOLD,
    TILE_SWORD_GOLD,
    TILE_TORCH,
    TILE_BRICKS,
    TILE_STONE_BRICKS,
    TILE_SANDSTONE_TOP,
    TILE_SANDSTONE_SIDE,
    TILE_GRAVEL,
    TILE_CLAY,
    TILE_WOOL_WHITE,
    TILE_WOOL_RED,
    TILE_WOOL_BLUE,
    TILE_WOOL_GREEN,
    TILE_WOOL_YELLOW,
    TILE_WOOL_BLACK,
    TILE_BOOKSHELF,
    TILE_IRON_BLOCK,
    TILE_GOLD_BLOCK,
    TILE_DIAMOND_BLOCK,
    TILE_COAL_BLOCK,
    TILE_LAMP,
    TILE_PUMPKIN_TOP,
    TILE_PUMPKIN_SIDE,
    TILE_CACTUS_TOP,
    TILE_CACTUS_SIDE,
    TILE_BIRCH_LOG_SIDE,
    TILE_BIRCH_LOG_TOP,
    TILE_BIRCH_LEAVES,
    TILE_SPRUCE_LOG_SIDE,
    TILE_SPRUCE_LOG_TOP,
    TILE_SPRUCE_LEAVES,
    TILE_CHEST_TOP,
    TILE_CHEST_SIDE,
    TILE_BED_TOP,
    TILE_BED_SIDE,
    TILE_TALL_GRASS,
    TILE_FERN,
    TILE_FLOWER_RED,
    TILE_FLOWER_YELLOW,
    TILE_FLOWER_BLUE,
    TILE_FLOWER_WHITE,
    TILE_MUSHROOM_RED,
    TILE_MUSHROOM_BROWN,
    TILE_DEAD_BUSH,
    TILE_SUGAR_CANE,
    TILE_BERRY_BUSH,
    TILE_BRICK,
    TILE_CLAY_BALL,
    TILE_PAPER,
    TILE_BOOK,
    TILE_DYE_RED,
    TILE_DYE_YELLOW,
    TILE_DYE_BLUE,
    TILE_DYE_GREEN,
    TILE_DYE_BLACK,
    TILE_BERRIES,
    TILE_COUNT
};

// Werkzeugart: passt das Werkzeug zum Block, baut es schneller ab
enum ToolClass : uint8_t { TOOL_NONE = 0, TOOL_PICKAXE, TOOL_AXE, TOOL_SHOVEL, TOOL_SWORD };

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
    ToolClass tool = TOOL_NONE;  // passende Werkzeugart
    int minTier = 0;             // noetige Werkzeugstufe, damit der Block etwas fallen laesst (0 = keine)
    bool emissive = false;       // leuchtet (unabhaengig von Tageszeit und Schatten)
};

const BlockInfo& blockInfo(BlockId id);

inline bool isOpaque(BlockId id) { return blockInfo(id).opaque; }
inline bool isSolid(BlockId id) { return blockInfo(id).solid; }
inline bool isWater(BlockId id) { return id == WATER || (id >= WATER_1 && id <= WATER_FALL); }
inline bool isLava(BlockId id) { return id >= LAVA && id <= LAVA_FALL; }
inline bool isLiquid(BlockId id) { return isWater(id) || isLava(id); }
constexpr int MAX_WATER_LEVEL = 7;
constexpr int MAX_LAVA_LEVEL = 3;
// Stroemungsstufe 1..7 (Wasser) bzw. 1..3 (Lava), sonst 0 (Quelle, fallend oder keine Fluessigkeit)
inline int flowLevel(BlockId id) {
    if (id >= WATER_1 && id <= WATER_7) return id - WATER_1 + 1;
    if (id >= LAVA_1 && id <= LAVA_3) return id - LAVA_1 + 1;
    return 0;
}
// Hoehe der Oberflaeche in einem Block (0..1), ohne Beruecksichtigung von Fluessigkeit darueber
inline float liquidHeight(BlockId id) {
    if (id == WATER_FALL || id == LAVA_FALL) return 1.0f;
    if (id == WATER || id == LAVA) return 8.0f / 9.0f;
    int l = flowLevel(id);
    if (l == 0) return 0.0f;
    return isLava(id) ? static_cast<float>(8 - 2 * l) / 9.0f : static_cast<float>(8 - l) / 9.0f;
}

inline bool isTorch(BlockId id) { return id >= TORCH && id <= TORCH_NZ; }
inline bool isPlant(BlockId id) { return id >= TALL_GRASS && id <= BERRY_BUSH; }
inline bool isLeaves(BlockId id) { return id == LEAVES || id == BIRCH_LEAVES || id == SPRUCE_LEAVES; }
inline bool isLogBlock(BlockId id) { return id == LOG || id == BIRCH_LOG || id == SPRUCE_LOG; }
// Darf die Pflanze auf dem Block darunter stehen?
inline bool plantCanStandOn(BlockId plant, BlockId below) {
    const bool soil = below == GRASS || below == DIRT;
    switch (plant) {
        case DEAD_BUSH: return below == SAND || soil || below == SANDSTONE || below == GRAVEL;
        case SUGAR_CANE: return below == SAND || soil || below == SUGAR_CANE;
        case MUSHROOM_RED:
        case MUSHROOM_BROWN: return soil || below == STONE || isLogBlock(below);
        default: return soil;
    }
}
// Lage des Traegerblocks einer Fackel relativ zu ihr
inline void torchSupportOffset(BlockId id, int& dx, int& dy, int& dz) {
    dx = dy = dz = 0;
    switch (id) {
        case TORCH_PX: dx = 1; break;
        case TORCH_NX: dx = -1; break;
        case TORCH_PZ: dz = 1; break;
        case TORCH_NZ: dz = -1; break;
        default: dy = -1; break;
    }
}
// Lichtstaerke, die ein Block aussendet (0..15)
inline int lightEmission(BlockId id) {
    if (isTorch(id)) return 14;
    if (id >= LAVA && id <= LAVA_FALL) return 15;
    if (id == FURNACE_LIT) return 13;
    if (id == LAMP) return 15;
    return 0;
}
constexpr int MAX_LIGHT = 15;

// Auswahl in der Hotbar
constexpr int HOTBAR_SIZE = 9;
extern const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE];

}  // namespace oc
