#include "Block.h"

namespace oc {

static const BlockInfo INFOS[BLOCK_COUNT] = {
    //  name       solid  opaque liquid sky    same   brk    top            bottom         side
    {"Luft",       false, false, false, false, false, false, TILE_STONE,     TILE_STONE,     TILE_STONE},
    {"Gras",       true,  true,  false, true,  false, true,  TILE_GRASS_TOP, TILE_DIRT,      TILE_GRASS_SIDE},
    {"Erde",       true,  true,  false, true,  false, true,  TILE_DIRT,      TILE_DIRT,      TILE_DIRT},
    {"Stein",      true,  true,  false, true,  false, true,  TILE_STONE,     TILE_STONE,     TILE_STONE},
    {"Sand",       true,  true,  false, true,  false, true,  TILE_SAND,      TILE_SAND,      TILE_SAND},
    {"Wasser",     false, false, true,  false, true,  false, TILE_WATER,     TILE_WATER,     TILE_WATER},
    {"Holz",       true,  true,  false, true,  false, true,  TILE_LOG_TOP,   TILE_LOG_TOP,   TILE_LOG_SIDE},
    {"Laub",       true,  false, false, true,  true,  true,  TILE_LEAVES,    TILE_LEAVES,    TILE_LEAVES},
    {"Bretter",    true,  true,  false, true,  false, true,  TILE_PLANKS,    TILE_PLANKS,    TILE_PLANKS},
    {"Bruchstein", true,  true,  false, true,  false, true,  TILE_COBBLE,    TILE_COBBLE,    TILE_COBBLE},
    {"Grundgestein", true, true, false, true,  false, false, TILE_BEDROCK,   TILE_BEDROCK,   TILE_BEDROCK},
    {"Glas",       true,  false, false, false, true,  true,  TILE_GLASS,     TILE_GLASS,     TILE_GLASS},
    {"Schnee",     true,  true,  false, true,  false, true,  TILE_SNOW,      TILE_SNOW,      TILE_SNOW},
};

const BlockInfo& blockInfo(BlockId id) {
    return INFOS[id < BLOCK_COUNT ? id : 0];
}

const BlockId DEFAULT_HOTBAR[HOTBAR_SIZE] = {GRASS, DIRT, STONE, COBBLE, PLANKS, LOG, LEAVES, SAND, GLASS};

}  // namespace oc
