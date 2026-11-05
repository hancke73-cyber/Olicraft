#include "Block.h"
#include "Items.h"

namespace oc {

static const BlockInfo BASE_INFOS[BERRY_BUSH + 1] = {
    //  name       solid  opaque liquid sky    same   brk    top            bottom         side            hard  drop
    {"Luft",       false, false, false, false, false, false, TILE_STONE,     TILE_STONE,     TILE_STONE,     0.0f, AIR},
    {"Gras",       true,  true,  false, true,  false, true,  TILE_GRASS_TOP, TILE_DIRT,      TILE_GRASS_SIDE, 0.35f, DIRT, TOOL_SHOVEL, 0},
    {"Erde",       true,  true,  false, true,  false, true,  TILE_DIRT,      TILE_DIRT,      TILE_DIRT,      0.3f, DIRT, TOOL_SHOVEL, 0},
    {"Stein",      true,  true,  false, true,  false, true,  TILE_STONE,     TILE_STONE,     TILE_STONE,     1.0f, COBBLE, TOOL_PICKAXE, 1},
    {"Sand",       true,  true,  false, true,  false, true,  TILE_SAND,      TILE_SAND,      TILE_SAND,      0.3f, SAND, TOOL_SHOVEL, 0},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Holz",       true,  true,  false, true,  false, true,  TILE_LOG_TOP,   TILE_LOG_TOP,   TILE_LOG_SIDE,  1.0f, LOG, TOOL_AXE, 0},
    {"Laub",       true,  false, false, true,  true,  true,  TILE_LEAVES,    TILE_LEAVES,    TILE_LEAVES,    0.15f, LEAVES},
    {"Bretter",    true,  true,  false, true,  false, true,  TILE_PLANKS,    TILE_PLANKS,    TILE_PLANKS,    0.8f, PLANKS, TOOL_AXE, 0},
    {"Bruchstein", true,  true,  false, true,  false, true,  TILE_COBBLE,    TILE_COBBLE,    TILE_COBBLE,    1.2f, COBBLE, TOOL_PICKAXE, 1},
    {"Grundgestein", true, true, false, true,  false, false, TILE_BEDROCK,   TILE_BEDROCK,   TILE_BEDROCK,   0.0f, AIR},
    {"Glas",       true,  false, false, false, true,  true,  TILE_GLASS,     TILE_GLASS,     TILE_GLASS,     0.2f, AIR},
    {"Schnee",     true,  true,  false, true,  false, true,  TILE_SNOW,      TILE_SNOW,      TILE_SNOW,      0.15f, SNOW, TOOL_SHOVEL, 0},
    // fliessendes Wasser (nicht abbaubar, nicht auswaehlbar)
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Lava",       false, false, true,  false, true,  false, TILE_LAVA,      TILE_LAVA,      TILE_LAVA,      0.0f, AIR},
    {"Lava",       false, false, true,  false, true,  false, TILE_LAVA,      TILE_LAVA,      TILE_LAVA,      0.0f, AIR},
    {"Lava",       false, false, true,  false, true,  false, TILE_LAVA,      TILE_LAVA,      TILE_LAVA,      0.0f, AIR},
    {"Lava",       false, false, true,  false, true,  false, TILE_LAVA,      TILE_LAVA,      TILE_LAVA,      0.0f, AIR},
    {"Lava",       false, false, true,  false, true,  false, TILE_LAVA,      TILE_LAVA,      TILE_LAVA,      0.0f, AIR},
    {"Obsidian",   true,  true,  false, true,  false, true,  TILE_OBSIDIAN,  TILE_OBSIDIAN,  TILE_OBSIDIAN,  6.0f, OBSIDIAN, TOOL_PICKAXE, 4},
    {"Werkbank",   true,  true,  false, true,  false, true,  TILE_CRAFT_TOP,   TILE_PLANKS,    TILE_CRAFT_SIDE, 1.0f, CRAFTING_TABLE, TOOL_AXE, 0},
    {"Kohleerz",    true,  true,  false, true,  false, true,  TILE_COAL_ORE,    TILE_COAL_ORE,    TILE_COAL_ORE,    1.5f, COAL_ORE, TOOL_PICKAXE, 1},
    {"Eisenerz",    true,  true,  false, true,  false, true,  TILE_IRON_ORE,    TILE_IRON_ORE,    TILE_IRON_ORE,    2.0f, IRON_ORE, TOOL_PICKAXE, 2},
    {"Diamanterz",  true,  true,  false, true,  false, true,  TILE_DIAMOND_ORE, TILE_DIAMOND_ORE, TILE_DIAMOND_ORE, 2.5f, DIAMOND_ORE, TOOL_PICKAXE, 3},
    {"Ofen",        true,  true,  false, true,  false, true,  TILE_FURNACE_TOP, TILE_FURNACE_TOP, TILE_FURNACE,     1.5f, FURNACE, TOOL_PICKAXE, 1},
    {"Ofen",        true,  true,  false, true,  false, true,  TILE_FURNACE_TOP, TILE_FURNACE_TOP, TILE_FURNACE_LIT, 1.5f, FURNACE, TOOL_PICKAXE, 1, true},
    {"Golderz",     true,  true,  false, true,  false, true,  TILE_GOLD_ORE,    TILE_GOLD_ORE,    TILE_GOLD_ORE,    2.5f, GOLD_ORE, TOOL_PICKAXE, 3},
    {"Fackel",     false, false, false, false, false, true,  TILE_TORCH,     TILE_TORCH,     TILE_TORCH,     0.0f, TORCH, TOOL_NONE, 0, true},
    {"Fackel",     false, false, false, false, false, true,  TILE_TORCH,     TILE_TORCH,     TILE_TORCH,     0.0f, TORCH, TOOL_NONE, 0, true},
    {"Fackel",     false, false, false, false, false, true,  TILE_TORCH,     TILE_TORCH,     TILE_TORCH,     0.0f, TORCH, TOOL_NONE, 0, true},
    {"Fackel",     false, false, false, false, false, true,  TILE_TORCH,     TILE_TORCH,     TILE_TORCH,     0.0f, TORCH, TOOL_NONE, 0, true},
    {"Fackel",     false, false, false, false, false, true,  TILE_TORCH,     TILE_TORCH,     TILE_TORCH,     0.0f, TORCH, TOOL_NONE, 0, true},
    //  name            solid  opaque liquid sky    same   brk    top                  bottom               side                  hard  drop
    {"Ziegel",         true,  true,  false, true,  false, true,  TILE_BRICKS,         TILE_BRICKS,         TILE_BRICKS,          2.0f, BRICKS, TOOL_PICKAXE, 1},
    {"Steinziegel",    true,  true,  false, true,  false, true,  TILE_STONE_BRICKS,   TILE_STONE_BRICKS,   TILE_STONE_BRICKS,    1.5f, STONE_BRICKS, TOOL_PICKAXE, 1},
    {"Sandstein",      true,  true,  false, true,  false, true,  TILE_SANDSTONE_TOP,  TILE_SANDSTONE_TOP,  TILE_SANDSTONE_SIDE,  0.8f, SANDSTONE, TOOL_PICKAXE, 1},
    {"Kies",           true,  true,  false, true,  false, true,  TILE_GRAVEL,         TILE_GRAVEL,         TILE_GRAVEL,          0.6f, GRAVEL, TOOL_SHOVEL, 0},
    {"Ton",            true,  true,  false, true,  false, true,  TILE_CLAY,           TILE_CLAY,           TILE_CLAY,            0.6f, CLAY, TOOL_SHOVEL, 0},
    {"Weisse Wolle",   true,  true,  false, true,  false, true,  TILE_WOOL_WHITE,     TILE_WOOL_WHITE,     TILE_WOOL_WHITE,      0.8f, WOOL_WHITE},
    {"Rote Wolle",     true,  true,  false, true,  false, true,  TILE_WOOL_RED,       TILE_WOOL_RED,       TILE_WOOL_RED,        0.8f, WOOL_RED},
    {"Blaue Wolle",    true,  true,  false, true,  false, true,  TILE_WOOL_BLUE,      TILE_WOOL_BLUE,      TILE_WOOL_BLUE,       0.8f, WOOL_BLUE},
    {"Gruene Wolle",   true,  true,  false, true,  false, true,  TILE_WOOL_GREEN,     TILE_WOOL_GREEN,     TILE_WOOL_GREEN,      0.8f, WOOL_GREEN},
    {"Gelbe Wolle",    true,  true,  false, true,  false, true,  TILE_WOOL_YELLOW,    TILE_WOOL_YELLOW,    TILE_WOOL_YELLOW,     0.8f, WOOL_YELLOW},
    {"Schwarze Wolle", true,  true,  false, true,  false, true,  TILE_WOOL_BLACK,     TILE_WOOL_BLACK,     TILE_WOOL_BLACK,      0.8f, WOOL_BLACK},
    {"Buecherregal",    true,  true,  false, true,  false, true,  TILE_PLANKS,         TILE_PLANKS,         TILE_BOOKSHELF,       1.5f, BOOKSHELF, TOOL_AXE, 0},
    {"Eisenblock",     true,  true,  false, true,  false, true,  TILE_IRON_BLOCK,     TILE_IRON_BLOCK,     TILE_IRON_BLOCK,      5.0f, IRON_BLOCK, TOOL_PICKAXE, 3},
    {"Goldblock",      true,  true,  false, true,  false, true,  TILE_GOLD_BLOCK,     TILE_GOLD_BLOCK,     TILE_GOLD_BLOCK,      3.0f, GOLD_BLOCK, TOOL_PICKAXE, 3},
    {"Diamantblock",   true,  true,  false, true,  false, true,  TILE_DIAMOND_BLOCK,  TILE_DIAMOND_BLOCK,  TILE_DIAMOND_BLOCK,   5.0f, DIAMOND_BLOCK, TOOL_PICKAXE, 3},
    {"Kohleblock",     true,  true,  false, true,  false, true,  TILE_COAL_BLOCK,     TILE_COAL_BLOCK,     TILE_COAL_BLOCK,      3.0f, COAL_BLOCK, TOOL_PICKAXE, 1},
    {"Lampe",          true,  true,  false, true,  false, true,  TILE_LAMP,           TILE_LAMP,           TILE_LAMP,            0.5f, LAMP, TOOL_NONE, 0, true},
    {"Kuerbis",        true,  true,  false, true,  false, true,  TILE_PUMPKIN_TOP,    TILE_PUMPKIN_TOP,    TILE_PUMPKIN_SIDE,    1.0f, PUMPKIN, TOOL_AXE, 0},
    {"Kaktus",         true,  true,  false, true,  false, true,  TILE_CACTUS_TOP,     TILE_CACTUS_TOP,     TILE_CACTUS_SIDE,     0.4f, CACTUS},
    {"Birkenholz",     true,  true,  false, true,  false, true,  TILE_BIRCH_LOG_TOP,  TILE_BIRCH_LOG_TOP,  TILE_BIRCH_LOG_SIDE,  1.0f, BIRCH_LOG, TOOL_AXE, 0},
    {"Birkenlaub",     true,  false, false, true,  true,  true,  TILE_BIRCH_LEAVES,   TILE_BIRCH_LEAVES,   TILE_BIRCH_LEAVES,    0.15f, BIRCH_LEAVES},
    {"Fichtenholz",    true,  true,  false, true,  false, true,  TILE_SPRUCE_LOG_TOP, TILE_SPRUCE_LOG_TOP, TILE_SPRUCE_LOG_SIDE, 1.0f, SPRUCE_LOG, TOOL_AXE, 0},
    {"Fichtenlaub",    true,  false, false, true,  true,  true,  TILE_SPRUCE_LEAVES,  TILE_SPRUCE_LEAVES,  TILE_SPRUCE_LEAVES,   0.15f, SPRUCE_LEAVES},
    {"Truhe",          true,  true,  false, true,  false, true,  TILE_CHEST_TOP,      TILE_CHEST_TOP,      TILE_CHEST_SIDE,      2.5f, CHEST, TOOL_AXE, 0},
    {"Bett",           true,  true,  false, true,  false, true,  TILE_BED_TOP,        TILE_PLANKS,         TILE_BED_SIDE,        0.4f, BED},
    // Pflanzen (nicht fest, nicht lichtblockierend)
    {"Hohes Gras",     false, false, false, false, false, true,  TILE_TALL_GRASS,     TILE_TALL_GRASS,     TILE_TALL_GRASS,      0.0f, AIR},
    {"Farn",           false, false, false, false, false, true,  TILE_FERN,           TILE_FERN,           TILE_FERN,            0.0f, FERN},
    {"Mohn",           false, false, false, false, false, true,  TILE_FLOWER_RED,     TILE_FLOWER_RED,     TILE_FLOWER_RED,      0.0f, FLOWER_RED},
    {"Loewenzahn",     false, false, false, false, false, true,  TILE_FLOWER_YELLOW,  TILE_FLOWER_YELLOW,  TILE_FLOWER_YELLOW,   0.0f, FLOWER_YELLOW},
    {"Kornblume",      false, false, false, false, false, true,  TILE_FLOWER_BLUE,    TILE_FLOWER_BLUE,    TILE_FLOWER_BLUE,     0.0f, FLOWER_BLUE},
    {"Gaensebluemchen", false, false, false, false, false, true, TILE_FLOWER_WHITE,   TILE_FLOWER_WHITE,   TILE_FLOWER_WHITE,    0.0f, FLOWER_WHITE},
    {"Roter Pilz",     false, false, false, false, false, true,  TILE_MUSHROOM_RED,   TILE_MUSHROOM_RED,   TILE_MUSHROOM_RED,    0.0f, MUSHROOM_RED},
    {"Brauner Pilz",   false, false, false, false, false, true,  TILE_MUSHROOM_BROWN, TILE_MUSHROOM_BROWN, TILE_MUSHROOM_BROWN,  0.0f, MUSHROOM_BROWN},
    {"Toter Busch",    false, false, false, false, false, true,  TILE_DEAD_BUSH,      TILE_DEAD_BUSH,      TILE_DEAD_BUSH,       0.0f, ITEM_STICK},
    {"Zuckerrohr",     false, false, false, false, false, true,  TILE_SUGAR_CANE,     TILE_SUGAR_CANE,     TILE_SUGAR_CANE,      0.0f, SUGAR_CANE},
    {"Beerenstrauch",  false, false, false, false, false, true,  TILE_BERRY_BUSH,     TILE_BERRY_BUSH,     TILE_BERRY_BUSH,      0.0f, ITEM_BERRIES},
};

namespace {

struct InfoTable {
    BlockInfo infos[BLOCK_COUNT];
    InfoTable() {
        for (int i = 0; i <= BERRY_BUSH; ++i) infos[i] = BASE_INFOS[i];
        // Treppen und Platten
        static const char* STAIR_NAMES[STAIR_MATERIALS] = {"Brettertreppe", "Bruchsteintreppe", "Steinziegeltreppe", "Ziegeltreppe", "Sandsteintreppe"};
        static const char* SLAB_NAMES[STAIR_MATERIALS] = {"Brettplatte", "Bruchsteinplatte", "Steinziegelplatte", "Ziegelplatte", "Sandsteinplatte"};
        for (int m = 0; m < STAIR_MATERIALS; ++m) {
            const BlockInfo& mb = BASE_INFOS[stairMaterialBlock(m)];
            for (int f = 0; f < 8; ++f) {  // 4 Richtungen, normal und umgedreht
                BlockInfo& i = infos[makeStairs(m, f & 3, f >= 4)];
                i = mb;
                i.name = STAIR_NAMES[m];
                i.opaque = false;
                i.shape = SH_STAIRS;
                i.drop = makeStairs(m, 0);
            }
            for (int t = 0; t < 2; ++t) {
                BlockInfo& i = infos[makeSlab(m, t == 1)];
                i = mb;
                i.name = SLAB_NAMES[m];
                i.opaque = false;
                i.shape = SH_SLAB;
                i.hardness = mb.hardness * 0.6f;
                i.drop = makeSlab(m, false);
            }
        }
        auto thin = [&](BlockId id, const char* name, Tile tile, float hardness, uint16_t drop, ToolClass tool, int tier, Shape shape, bool solid = true) {
            BlockInfo& i = infos[id];
            i = {name, solid, false, false, false, false, true, tile, tile, tile, hardness, drop, tool, tier, false, shape};
        };
        thin(FENCE, "Zaun", TILE_PLANKS, 1.0f, FENCE, TOOL_AXE, 0, SH_FENCE);
        thin(WALL_COBBLE, "Bruchsteinmauer", TILE_COBBLE, 1.5f, WALL_COBBLE, TOOL_PICKAXE, 1, SH_WALL);
        thin(WALL_BRICKS, "Ziegelmauer", TILE_BRICKS, 2.0f, WALL_BRICKS, TOOL_PICKAXE, 1, SH_WALL);
        thin(GLASS_PANE, "Glasscheibe", TILE_GLASS, 0.2f, AIR, TOOL_NONE, 0, SH_PANE);
        thin(IRON_BARS, "Eisengitter", TILE_IRON_BARS, 3.0f, IRON_BARS, TOOL_PICKAXE, 1, SH_PANE);
        thin(LANTERN, "Laterne", TILE_LANTERN, 0.5f, LANTERN, TOOL_NONE, 0, SH_LANTERN, false);
        thin(LANTERN_HANG, "Laterne", TILE_LANTERN, 0.5f, LANTERN, TOOL_NONE, 0, SH_LANTERN, false);
        infos[LANTERN].emissive = infos[LANTERN_HANG].emissive = true;
        // Tueren
        for (int mat = 0; mat < 2; ++mat)
            for (int up = 0; up < 2; ++up)
                for (int open = 0; open < 2; ++open)
                    for (int f = 0; f < 4; ++f) {
                        const Tile tile = mat == 0 ? (up ? TILE_DOOR_WOOD_UPPER : TILE_DOOR_WOOD_LOWER) : (up ? TILE_DOOR_IRON_UPPER : TILE_DOOR_IRON_LOWER);
                        thin(makeDoor(mat, up == 1, open == 1, f), mat == 0 ? "Holztuer" : "Eisentuer", tile, mat == 0 ? 1.5f : 4.0f,
                             static_cast<uint16_t>(up ? 0 : (mat == 0 ? ITEM_DOOR_WOOD : ITEM_DOOR_IRON)), mat == 0 ? TOOL_AXE : TOOL_PICKAXE, mat == 0 ? 0 : 1, SH_DOOR, open == 0);
                    }
        // Tore
        for (int mat = 0; mat < 2; ++mat)
            for (int open = 0; open < 2; ++open)
                for (int axis = 0; axis < 2; ++axis)
                    thin(makeGate(mat, open == 1, axis), mat == 0 ? "Holztor" : "Eisentor", mat == 0 ? TILE_GATE_WOOD : TILE_GATE_IRON,
                         mat == 0 ? 1.5f : 4.0f, makeGate(mat, false, 0), mat == 0 ? TOOL_AXE : TOOL_PICKAXE, mat == 0 ? 0 : 1, SH_GATE, open == 0);
        // Leitern und Betten
        for (int f = 0; f < 4; ++f) {
            thin(makeLadder(f), "Leiter", TILE_LADDER, 0.4f, LADDER_BASE, TOOL_AXE, 0, SH_LADDER, false);
            for (int hd = 0; hd < 2; ++hd) {
                BlockInfo& i = infos[makeBedPart(hd == 1, f)];
                i = {"Bett", true, false, false, true, false, true, hd ? TILE_BED_HEAD_TOP : TILE_BED_FOOT_TOP, TILE_PLANKS, TILE_BED_SIDE, 0.4f, BED, TOOL_NONE, 0, false, SH_BED};
            }
        }
        // Acker und Feldfruechte
        infos[FARMLAND] = {"Acker", true, false, false, true, false, true, TILE_FARMLAND, TILE_DIRT, TILE_DIRT, 0.3f, DIRT, TOOL_SHOVEL, 0, false, SH_FARMLAND};
        for (int s = 0; s < 4; ++s) {
            thin(static_cast<BlockId>(WHEAT_0 + s), "Weizen", static_cast<Tile>(TILE_WHEAT_0 + s), 0.0f, static_cast<uint16_t>(s == 3 ? ITEM_WHEAT : ITEM_SEEDS), TOOL_NONE, 0, SH_FULL, false);
            thin(static_cast<BlockId>(CARROT_0 + s), "Karotten", static_cast<Tile>(TILE_CARROT_0 + s), 0.0f, static_cast<uint16_t>(ITEM_CARROT), TOOL_NONE, 0, SH_FULL, false);
        }
    }
};

}  // namespace

const BlockInfo& blockInfo(BlockId id) {
    static const InfoTable table;
    return table.infos[id < BLOCK_COUNT ? id : 0];
}

const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE] = {GRASS, DIRT, STONE, COBBLE, PLANKS, LOG, LEAVES, SAND, GLASS};

}  // namespace oc
