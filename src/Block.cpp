#include "Block.h"

namespace oc {

static const BlockInfo INFOS[BLOCK_COUNT] = {
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
};

const BlockInfo& blockInfo(BlockId id) {
    return INFOS[id < BLOCK_COUNT ? id : 0];
}

const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE] = {GRASS, DIRT, STONE, COBBLE, PLANKS, LOG, LEAVES, SAND, GLASS};

}  // namespace oc
