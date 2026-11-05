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
    // Bauteile mit Zustand im Block-Id (siehe Hilfsfunktionen unten):
    // Treppen: 5 Materialien x 4 Richtungen (Richtung der hohen Seite: +X, -X, +Z, -Z)
    STAIRS_BASE,
    STAIRS_END = STAIRS_BASE + 19,
    // Platten: 5 Materialien x (unten, oben)
    SLAB_BASE,
    SLAB_END = SLAB_BASE + 9,
    // Zaun, Mauern, Fensterscheibe, Gitter: verbinden sich mit Nachbarn
    FENCE,
    WALL_COBBLE,
    WALL_BRICKS,
    GLASS_PANE,
    IRON_BARS,
    // Laternen: auf dem Boden oder haengend
    LANTERN,
    LANTERN_HANG,
    // Tueren: 2 Materialien x (unten/oben) x (offen/zu) x 4 Richtungen
    DOOR_BASE,
    DOOR_END = DOOR_BASE + 31,
    // Tore: 2 Materialien x (offen/zu) x 2 Achsen; nebeneinander gesetzte Tore oeffnen gemeinsam
    GATE_BASE,
    GATE_END = GATE_BASE + 7,
    // Acker und Pflanzen
    FARMLAND,
    WHEAT_0,
    WHEAT_1,
    WHEAT_2,
    WHEAT_3,
    CARROT_0,
    CARROT_1,
    CARROT_2,
    CARROT_3,
    // Leitern: 4 Richtungen (Seite des Traegerblocks: +X, -X, +Z, -Z)
    LADDER_BASE,
    LADDER_END = LADDER_BASE + 3,
    // Betten aus zwei Bloecken: id = BED_BASE + (Kopf ? 4 : 0) + Richtung (vom Fussteil zum Kopfteil)
    BED_BASE,
    BED_END = BED_BASE + 7,
    // Umgedrehte Treppen (hohe Seite unten): gleiche Anordnung wie STAIRS_BASE. Am Ende, damit alte Spielstaende gueltig bleiben.
    STAIRS_TOP_BASE,
    STAIRS_TOP_END = STAIRS_TOP_BASE + 19,
    // Dachkeile: glatte Schraege ueber die ganze Zelle, 5 Materialien x 4 Richtungen (Richtung der hohen Seite: +X, -X, +Z, -Z)
    WEDGE_BASE,
    WEDGE_END = WEDGE_BASE + 19,
    // Schilder: id = SIGN_BASE + (Wand ? 4 : 0) + Richtung, in die die Vorderseite zeigt (+X, -X, +Z, -Z); Stehschilder haben einen Pfosten
    SIGN_BASE,
    SIGN_END = SIGN_BASE + 7,
    // Steampunk: Dampfkessel (aus/an), Dampfrohr, Antriebswelle, Dampfmotor, Destillierapparat
    BOILER,
    BOILER_LIT,
    PIPE,
    SHAFT,
    MOTOR,
    STILL,
    // Bohrmaschine, Richtung des Bohrers: 0 +X, 1 -X, 2 +Z, 3 -Z, 4 oben, 5 unten
    DRILL_BASE,
    DRILL_END = DRILL_BASE + 5,
    // Foerderband, Richtung: 0 +X, 1 -X, 2 +Z, 3 -Z
    CONVEYOR_BASE,
    CONVEYOR_END = CONVEYOR_BASE + 3,
    LIFT,
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
    TILE_IRON_BARS,
    TILE_LANTERN,
    TILE_DOOR_WOOD_LOWER,
    TILE_DOOR_WOOD_UPPER,
    TILE_DOOR_IRON_LOWER,
    TILE_DOOR_IRON_UPPER,
    TILE_GATE_WOOD,
    TILE_GATE_IRON,
    TILE_FARMLAND,
    TILE_WHEAT_0,
    TILE_WHEAT_1,
    TILE_WHEAT_2,
    TILE_WHEAT_3,
    TILE_CARROT_0,
    TILE_CARROT_1,
    TILE_CARROT_2,
    TILE_CARROT_3,
    TILE_SEEDS,
    TILE_WHEAT,
    TILE_BREAD,
    TILE_CARROT,
    TILE_HOE_WOOD,
    TILE_HOE_STONE,
    TILE_HOE_IRON,
    TILE_HOE_GOLD,
    TILE_HOE_DIAMOND,
    TILE_DOOR_WOOD_ITEM,
    TILE_DOOR_IRON_ITEM,
    TILE_LADDER,
    TILE_BED_FOOT_TOP,
    TILE_BED_HEAD_TOP,
    TILE_LEATHER,
    TILE_ARMOR_BASE,  // + material*4 + Teil (Helm, Brustpanzer, Hose, Stiefel); Material: Leder, Eisen, Gold, Diamant
    TILE_ARMOR_END = TILE_ARMOR_BASE + 15,
    TILE_BUCKET,
    TILE_BUCKET_WATER,
    TILE_BUCKET_LAVA,
    TILE_SIGN,
    TILE_BRASS,
    TILE_BOILER,
    TILE_BOILER_LIT,
    TILE_PIPE,
    TILE_SHAFT,
    TILE_MOTOR,
    TILE_STILL,
    TILE_DRILL,
    TILE_CONVEYOR_BASE,  // + Richtung 0..3
    TILE_CONVEYOR_END = TILE_CONVEYOR_BASE + 3,
    TILE_LIFT,
    TILE_TAR,
    TILE_FUEL_OIL,
    TILE_SALT,
    TILE_BUCKET_DISTILLED,
    TILE_COUNT
};

// Werkzeugart: passt das Werkzeug zum Block, baut es schneller ab
enum ToolClass : uint8_t { TOOL_NONE = 0, TOOL_PICKAXE, TOOL_AXE, TOOL_SHOVEL, TOOL_SWORD, TOOL_HOE };

// Form eines Blocks: Vollwuerfel oder Teilform (genaue Boxen siehe BlockShape.h)
enum Shape : uint8_t { SH_FULL = 0, SH_STAIRS, SH_SLAB, SH_FENCE, SH_WALL, SH_PANE, SH_DOOR, SH_GATE, SH_LANTERN, SH_FARMLAND, SH_LADDER, SH_BED, SH_WEDGE, SH_SIGN, SH_PIPE, SH_SHAFT, SH_DRILL, SH_CONVEYOR, SH_LIFT };

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
    uint16_t drop;   // Block oder Gegenstand, der beim Abbauen faellt (AIR = nichts)
    ToolClass tool = TOOL_NONE;  // passende Werkzeugart
    int minTier = 0;             // noetige Werkzeugstufe, damit der Block etwas fallen laesst (0 = keine)
    bool emissive = false;       // leuchtet (unabhaengig von Tageszeit und Schatten)
    Shape shape = SH_FULL;       // Teilform (Treppe, Platte, Zaun, Tuer ...)
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

// --- Bauteile
constexpr int STAIR_MATERIALS = 5;  // Bretter, Bruchstein, Steinziegel, Ziegel, Sandstein
inline bool stairIsTop(BlockId id) { return id >= STAIRS_TOP_BASE && id <= STAIRS_TOP_END; }  // umgedreht: Stufe unten, Platte oben
inline bool isStairs(BlockId id) { return (id >= STAIRS_BASE && id <= STAIRS_END) || stairIsTop(id); }
inline int stairIndex(BlockId id) { return stairIsTop(id) ? id - STAIRS_TOP_BASE : id - STAIRS_BASE; }
inline int stairMaterial(BlockId id) { return stairIndex(id) / 4; }
inline int stairFacing(BlockId id) { return stairIndex(id) % 4; }  // Richtung der hohen Seite: 0 +X, 1 -X, 2 +Z, 3 -Z
inline BlockId makeStairs(int material, int facing, bool top = false) {
    return static_cast<BlockId>((top ? STAIRS_TOP_BASE : STAIRS_BASE) + material * 4 + facing);
}
inline bool isSign(BlockId id) { return id >= SIGN_BASE && id <= SIGN_END; }
inline bool signIsWall(BlockId id) { return (id - SIGN_BASE) >= 4; }
inline int signFacing(BlockId id) { return (id - SIGN_BASE) % 4; }  // Richtung der Vorderseite
inline BlockId makeSign(bool wall, int facing) { return static_cast<BlockId>(SIGN_BASE + (wall ? 4 : 0) + facing); }
// --- Dampfmaschinen
inline bool isBoiler(BlockId id) { return id == BOILER || id == BOILER_LIT; }
inline bool isDrill(BlockId id) { return id >= DRILL_BASE && id <= DRILL_END; }
inline int drillFacing(BlockId id) { return id - DRILL_BASE; }  // 0 +X, 1 -X, 2 +Z, 3 -Z, 4 oben, 5 unten
inline BlockId makeDrill(int facing) { return static_cast<BlockId>(DRILL_BASE + facing); }
inline bool isConveyor(BlockId id) { return id >= CONVEYOR_BASE && id <= CONVEYOR_END; }
inline int conveyorFacing(BlockId id) { return id - CONVEYOR_BASE; }
inline BlockId makeConveyor(int facing) { return static_cast<BlockId>(CONVEYOR_BASE + facing); }
// Dampfanschluss: hier fliesst Dampf durch bzw. wird er erzeugt oder verbraucht
inline bool isSteamPart(BlockId id) { return id == PIPE || isBoiler(id) || id == MOTOR || id == STILL; }
// Mechanische Verbindung: Welle, Motor und alles, was Antrieb bekommt
inline bool isMechPart(BlockId id) { return id == SHAFT || id == MOTOR || isDrill(id) || isConveyor(id) || id == LIFT; }
// Maschinen mit eigenem Zustand (Inhalt, Fortschritt)
inline bool isMachineBlock(BlockId id) { return isBoiler(id) || id == STILL || isDrill(id) || id == MOTOR; }
inline bool isWedge(BlockId id) { return id >= WEDGE_BASE && id <= WEDGE_END; }
inline int wedgeMaterial(BlockId id) { return (id - WEDGE_BASE) / 4; }
inline int wedgeFacing(BlockId id) { return (id - WEDGE_BASE) % 4; }  // Richtung der hohen Seite
inline BlockId makeWedge(int material, int facing) { return static_cast<BlockId>(WEDGE_BASE + material * 4 + facing); }
inline bool isSlab(BlockId id) { return id >= SLAB_BASE && id <= SLAB_END; }
inline int slabMaterial(BlockId id) { return (id - SLAB_BASE) / 2; }
inline bool slabIsTop(BlockId id) { return (id - SLAB_BASE) % 2 == 1; }
inline BlockId makeSlab(int material, bool top) { return static_cast<BlockId>(SLAB_BASE + material * 2 + (top ? 1 : 0)); }
// Vollblock zum Material (Bretter, Bruchstein, Steinziegel, Ziegel, Sandstein)
inline BlockId stairMaterialBlock(int material) {
    static const BlockId m[STAIR_MATERIALS] = {PLANKS, COBBLE, STONE_BRICKS, BRICKS, SANDSTONE};
    return m[material < 0 || material >= STAIR_MATERIALS ? 0 : material];
}
inline bool isFence(BlockId id) { return id == FENCE; }
inline bool isWall(BlockId id) { return id == WALL_COBBLE || id == WALL_BRICKS; }
inline bool isPane(BlockId id) { return id == GLASS_PANE || id == IRON_BARS; }
inline bool isLantern(BlockId id) { return id == LANTERN || id == LANTERN_HANG; }
// Tueren: id = DOOR_BASE + material*16 + upper*8 + open*4 + facing
inline bool isDoor(BlockId id) { return id >= DOOR_BASE && id <= DOOR_END; }
inline int doorMaterial(BlockId id) { return (id - DOOR_BASE) / 16; }  // 0 Holz, 1 Eisen
inline bool doorUpper(BlockId id) { return ((id - DOOR_BASE) / 8) % 2 == 1; }
inline bool doorOpen(BlockId id) { return ((id - DOOR_BASE) / 4) % 2 == 1; }
inline int doorFacing(BlockId id) { return (id - DOOR_BASE) % 4; }  // Seite der geschlossenen Platte: 0 +X, 1 -X, 2 +Z, 3 -Z
inline BlockId makeDoor(int material, bool upper, bool open, int facing) {
    return static_cast<BlockId>(DOOR_BASE + material * 16 + (upper ? 8 : 0) + (open ? 4 : 0) + facing);
}
// Tore: id = GATE_BASE + material*4 + open*2 + axis (Achse 0: Tor spannt sich entlang X, 1: entlang Z)
inline bool isGate(BlockId id) { return id >= GATE_BASE && id <= GATE_END; }
inline int gateMaterial(BlockId id) { return (id - GATE_BASE) / 4; }
inline bool gateOpen(BlockId id) { return ((id - GATE_BASE) / 2) % 2 == 1; }
inline int gateAxis(BlockId id) { return (id - GATE_BASE) % 2; }
inline BlockId makeGate(int material, bool open, int axis) { return static_cast<BlockId>(GATE_BASE + material * 4 + (open ? 2 : 0) + axis); }
inline bool isLadder(BlockId id) { return id >= LADDER_BASE && id <= LADDER_END; }
inline int ladderFacing(BlockId id) { return id - LADDER_BASE; }  // Seite des Traegerblocks
inline BlockId makeLadder(int facing) { return static_cast<BlockId>(LADDER_BASE + facing); }
inline bool isBedPart(BlockId id) { return id >= BED_BASE && id <= BED_END; }
inline bool bedIsHead(BlockId id) { return (id - BED_BASE) >= 4; }
inline int bedFacing(BlockId id) { return (id - BED_BASE) % 4; }  // vom Fussteil zum Kopfteil: 0 +X, 1 -X, 2 +Z, 3 -Z
inline BlockId makeBedPart(bool head, int facing) { return static_cast<BlockId>(BED_BASE + (head ? 4 : 0) + facing); }
inline bool isAnyBed(BlockId id) { return id == BED || isBedPart(id); }
inline bool isCrop(BlockId id) { return (id >= WHEAT_0 && id <= WHEAT_3) || (id >= CARROT_0 && id <= CARROT_3); }
inline int cropStage(BlockId id) { return id >= CARROT_0 ? id - CARROT_0 : id - WHEAT_0; }
inline bool cropFullyGrown(BlockId id) { return isCrop(id) && cropStage(id) == 3; }

inline bool isTorch(BlockId id) { return id >= TORCH && id <= TORCH_NZ; }
inline bool isPlant(BlockId id) { return (id >= TALL_GRASS && id <= BERRY_BUSH) || isCrop(id); }
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
        default: return isCrop(plant) ? below == FARMLAND : soil;
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
    if (id == BOILER_LIT) return 12;
    if (id == LAMP) return 15;
    if (id == LANTERN || id == LANTERN_HANG) return 15;
    return 0;
}
constexpr int MAX_LIGHT = 15;

// Auswahl in der Hotbar
constexpr int HOTBAR_SIZE = 9;
extern const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE];

}  // namespace oc
