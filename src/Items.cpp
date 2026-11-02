#include "Items.h"

namespace oc {

static const ItemInfo BLOCK_ITEM_NONE = {"", 0, false, TILE_STONE, TILE_STONE, TOOL_NONE, 0, 0, 1.0f, 2};

static const ItemId CREATIVE_LIST[] = {GRASS, DIRT, STONE, COBBLE, SAND, SNOW, LOG, PLANKS, LEAVES, GLASS, WATER, LAVA, OBSIDIAN, BEDROCK,
                                       CRAFTING_TABLE, ITEM_STICK, ITEM_WOODEN_PICKAXE, ITEM_WOODEN_AXE, ITEM_WOODEN_SHOVEL,
                                       ITEM_WOODEN_SWORD, ITEM_STONE_PICKAXE, ITEM_STONE_AXE, ITEM_STONE_SHOVEL,
                                       ITEM_STONE_SWORD, ITEM_MEAT};

bool itemValid(ItemId id) {
    if (id == ITEM_MEAT || (id >= ITEM_STICK && id <= ITEM_STONE_SWORD)) return true;
    if (id > 0 && id < BLOCK_COUNT) {
        // Fliessendes Wasser gibt es nicht als Gegenstand
        return !((id >= WATER_1 && id <= WATER_FALL) || (id >= LAVA_1 && id <= LAVA_FALL));
    }
    return false;
}

const ItemInfo& itemInfo(ItemId id) {
    static ItemInfo cache[BLOCK_COUNT];
    static bool init = false;
    static const ItemInfo meat = {"Rohes Fleisch", 64, false, TILE_MEAT, TILE_MEAT, TOOL_NONE, 0, 0, 1.0f, 2};
    if (!init) {
        for (int i = 1; i < BLOCK_COUNT; ++i) {
            const BlockInfo& bi = blockInfo(static_cast<BlockId>(i));
            cache[i] = {bi.name, 64, true, bi.top, bi.side, TOOL_NONE, 0, 0, 1.0f, 2};
        }
        init = true;
    }
    if (id == ITEM_MEAT) return meat;
    static const ItemInfo tools[9] = {
        {"Stock", 64, false, TILE_STICK, TILE_STICK, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Holzspitzhacke", 1, false, TILE_PICKAXE_WOOD, TILE_PICKAXE_WOOD, TOOL_PICKAXE, 1, 60, 2.0f, 3},
        {"Holzaxt", 1, false, TILE_AXE_WOOD, TILE_AXE_WOOD, TOOL_AXE, 1, 60, 2.0f, 4},
        {"Holzschaufel", 1, false, TILE_SHOVEL_WOOD, TILE_SHOVEL_WOOD, TOOL_SHOVEL, 1, 60, 2.0f, 3},
        {"Holzschwert", 1, false, TILE_SWORD_WOOD, TILE_SWORD_WOOD, TOOL_SWORD, 1, 60, 1.5f, 5},
        {"Steinspitzhacke", 1, false, TILE_PICKAXE_STONE, TILE_PICKAXE_STONE, TOOL_PICKAXE, 2, 132, 3.5f, 4},
        {"Steinaxt", 1, false, TILE_AXE_STONE, TILE_AXE_STONE, TOOL_AXE, 2, 132, 3.5f, 5},
        {"Steinschaufel", 1, false, TILE_SHOVEL_STONE, TILE_SHOVEL_STONE, TOOL_SHOVEL, 2, 132, 3.5f, 3},
        {"Steinschwert", 1, false, TILE_SWORD_STONE, TILE_SWORD_STONE, TOOL_SWORD, 2, 132, 1.5f, 6},
    };
    if (id >= ITEM_STICK && id <= ITEM_STONE_SWORD) return tools[id - ITEM_STICK];
    if (id > 0 && id < BLOCK_COUNT) return cache[id];
    return BLOCK_ITEM_NONE;
}

ItemId blockDropItem(BlockId b) {
    const BlockInfo& bi = blockInfo(b);
    if (!bi.breakable || bi.drop == AIR) return ITEM_NONE;
    return static_cast<ItemId>(bi.drop);
}

bool canHarvest(BlockId b, const ItemStack& held) {
    const BlockInfo& bi = blockInfo(b);
    if (bi.minTier <= 0) return true;
    if (held.empty()) return false;
    const ItemInfo& ii = itemInfo(held.id);
    return ii.tool == bi.tool && ii.tier >= bi.minTier;
}

float miningSeconds(BlockId b, const ItemStack& held) {
    const BlockInfo& bi = blockInfo(b);
    float t = bi.hardness;
    if (!held.empty()) {
        const ItemInfo& ii = itemInfo(held.id);
        if (ii.tool != TOOL_NONE && ii.tool == bi.tool) t /= ii.speed;
        else if (ii.tool == TOOL_SWORD && b == LEAVES) t /= ii.speed;
    }
    if (!canHarvest(b, held)) t *= 4.0f;  // ohne passendes Werkzeug muehsam und ohne Beute
    return t;
}

int attackDamage(const ItemStack& held) { return held.empty() ? 2 : itemInfo(held.id).attack; }

int creativeItemCount() { return static_cast<int>(sizeof(CREATIVE_LIST) / sizeof(CREATIVE_LIST[0])); }
ItemId creativeItem(int index) { return CREATIVE_LIST[index]; }

}  // namespace oc
