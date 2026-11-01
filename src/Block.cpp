#include "Block.h"

namespace oc {

static const BlockInfo INFOS[BLOCK_COUNT] = {
    //  name       solid  opaque liquid sky    same   brk    top            bottom         side            hard  drop
    {"Luft",       false, false, false, false, false, false, TILE_STONE,     TILE_STONE,     TILE_STONE,     0.0f, AIR},
    {"Gras",       true,  true,  false, true,  false, true,  TILE_GRASS_TOP, TILE_DIRT,      TILE_GRASS_SIDE, 0.35f, DIRT},
    {"Erde",       true,  true,  false, true,  false, true,  TILE_DIRT,      TILE_DIRT,      TILE_DIRT,      0.3f, DIRT},
    {"Stein",      true,  true,  false, true,  false, true,  TILE_STONE,     TILE_STONE,     TILE_STONE,     1.0f, COBBLE},
    {"Sand",       true,  true,  false, true,  false, true,  TILE_SAND,      TILE_SAND,      TILE_SAND,      0.3f, SAND},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Holz",       true,  true,  false, true,  false, true,  TILE_LOG_TOP,   TILE_LOG_TOP,   TILE_LOG_SIDE,  1.0f, LOG},
    {"Laub",       true,  false, false, true,  true,  true,  TILE_LEAVES,    TILE_LEAVES,    TILE_LEAVES,    0.15f, LEAVES},
    {"Bretter",    true,  true,  false, true,  false, true,  TILE_PLANKS,    TILE_PLANKS,    TILE_PLANKS,    0.8f, PLANKS},
    {"Bruchstein", true,  true,  false, true,  false, true,  TILE_COBBLE,    TILE_COBBLE,    TILE_COBBLE,    1.2f, COBBLE},
    {"Grundgestein", true, true, false, true,  false, false, TILE_BEDROCK,   TILE_BEDROCK,   TILE_BEDROCK,   0.0f, AIR},
    {"Glas",       true,  false, false, false, true,  true,  TILE_GLASS,     TILE_GLASS,     TILE_GLASS,     0.2f, AIR},
    {"Schnee",     true,  true,  false, true,  false, true,  TILE_SNOW,      TILE_SNOW,      TILE_SNOW,      0.15f, SNOW},
    // fliessendes Wasser (nicht abbaubar, nicht auswaehlbar)
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER,     0.0f, AIR},
};

const BlockInfo& blockInfo(BlockId id) {
    return INFOS[id < BLOCK_COUNT ? id : 0];
}

const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE] = {GRASS, DIRT, STONE, COBBLE, PLANKS, LOG, LEAVES, SAND, GLASS};

}  // namespace oc
