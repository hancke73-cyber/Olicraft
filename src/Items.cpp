#include "Items.h"

namespace oc {

static const ItemInfo BLOCK_ITEM_NONE = {"", 0, false, TILE_STONE, TILE_STONE, TOOL_NONE, 0, 0, 1.0f, 2};

static const ItemId CREATIVE_LIST[] = {GRASS, DIRT, STONE, COBBLE, SAND, SNOW, LOG, PLANKS, LEAVES, GLASS, WATER, LAVA, OBSIDIAN, BEDROCK, TORCH,
                                       CRAFTING_TABLE, ITEM_STICK, ITEM_WOODEN_PICKAXE, ITEM_WOODEN_AXE, ITEM_WOODEN_SHOVEL,
                                       ITEM_WOODEN_SWORD, ITEM_STONE_PICKAXE, ITEM_STONE_AXE, ITEM_STONE_SHOVEL,
                                       ITEM_STONE_SWORD, ITEM_IRON_PICKAXE, ITEM_IRON_AXE, ITEM_IRON_SHOVEL, ITEM_IRON_SWORD,
                                       ITEM_DIAMOND_PICKAXE, ITEM_DIAMOND_AXE, ITEM_DIAMOND_SHOVEL, ITEM_DIAMOND_SWORD,
                                       ITEM_GOLD_PICKAXE, ITEM_GOLD_AXE, ITEM_GOLD_SHOVEL, ITEM_GOLD_SWORD,
                                       COAL_ORE, IRON_ORE, DIAMOND_ORE, GOLD_ORE, FURNACE, ITEM_COAL, ITEM_IRON_INGOT,
                                       ITEM_GOLD_INGOT, ITEM_DIAMOND,
                                       ITEM_MEAT, ITEM_COOKED_MEAT};

bool itemValid(ItemId id) {
    if (id == ITEM_MEAT || (id >= ITEM_STICK && id <= ITEM_LAST)) return true;
    if (id > 0 && id < BLOCK_COUNT) {
        // Fliessendes Wasser gibt es nicht als Gegenstand
        return !((id >= WATER_1 && id <= WATER_FALL) || (id >= LAVA_1 && id <= LAVA_FALL) || id == FURNACE_LIT ||
                 (id > TORCH && id <= TORCH_NZ));
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
            cache[i] = {bi.name, 64, true, bi.top, bi.side, TOOL_NONE, 0, 0, 1.0f, 2, i == TORCH};
        }
        init = true;
    }
    if (id == ITEM_MEAT) return meat;
    static const ItemInfo items[ITEM_LAST - ITEM_STICK + 1] = {
        {"Stock", 64, false, TILE_STICK, TILE_STICK, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Holzspitzhacke", 1, false, TILE_PICKAXE_WOOD, TILE_PICKAXE_WOOD, TOOL_PICKAXE, 1, 60, 2.0f, 3},
        {"Holzaxt", 1, false, TILE_AXE_WOOD, TILE_AXE_WOOD, TOOL_AXE, 1, 60, 2.0f, 4},
        {"Holzschaufel", 1, false, TILE_SHOVEL_WOOD, TILE_SHOVEL_WOOD, TOOL_SHOVEL, 1, 60, 2.0f, 3},
        {"Holzschwert", 1, false, TILE_SWORD_WOOD, TILE_SWORD_WOOD, TOOL_SWORD, 1, 60, 1.5f, 5},
        {"Steinspitzhacke", 1, false, TILE_PICKAXE_STONE, TILE_PICKAXE_STONE, TOOL_PICKAXE, 2, 132, 3.5f, 4},
        {"Steinaxt", 1, false, TILE_AXE_STONE, TILE_AXE_STONE, TOOL_AXE, 2, 132, 3.5f, 5},
        {"Steinschaufel", 1, false, TILE_SHOVEL_STONE, TILE_SHOVEL_STONE, TOOL_SHOVEL, 2, 132, 3.5f, 3},
        {"Steinschwert", 1, false, TILE_SWORD_STONE, TILE_SWORD_STONE, TOOL_SWORD, 2, 132, 1.5f, 6},
        {"Kohle", 64, false, TILE_COAL, TILE_COAL, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Eisenbarren", 64, false, TILE_IRON_INGOT, TILE_IRON_INGOT, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Diamant", 64, false, TILE_DIAMOND, TILE_DIAMOND, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Gebratenes Fleisch", 64, false, TILE_COOKED_MEAT, TILE_COOKED_MEAT, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Eisenspitzhacke", 1, false, TILE_PICKAXE_IRON, TILE_PICKAXE_IRON, TOOL_PICKAXE, 3, 250, 5.5f, 5},
        {"Eisenaxt", 1, false, TILE_AXE_IRON, TILE_AXE_IRON, TOOL_AXE, 3, 250, 5.5f, 6},
        {"Eisenschaufel", 1, false, TILE_SHOVEL_IRON, TILE_SHOVEL_IRON, TOOL_SHOVEL, 3, 250, 5.5f, 4},
        {"Eisenschwert", 1, false, TILE_SWORD_IRON, TILE_SWORD_IRON, TOOL_SWORD, 3, 250, 1.5f, 7},
        {"Diamantspitzhacke", 1, false, TILE_PICKAXE_DIAMOND, TILE_PICKAXE_DIAMOND, TOOL_PICKAXE, 4, 1560, 8.0f, 6},
        {"Diamantaxt", 1, false, TILE_AXE_DIAMOND, TILE_AXE_DIAMOND, TOOL_AXE, 4, 1560, 8.0f, 7},
        {"Diamantschaufel", 1, false, TILE_SHOVEL_DIAMOND, TILE_SHOVEL_DIAMOND, TOOL_SHOVEL, 4, 1560, 8.0f, 5},
        {"Diamantschwert", 1, false, TILE_SWORD_DIAMOND, TILE_SWORD_DIAMOND, TOOL_SWORD, 4, 1560, 1.5f, 8},
        {"Goldbarren", 64, false, TILE_GOLD_INGOT, TILE_GOLD_INGOT, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Goldspitzhacke", 1, false, TILE_PICKAXE_GOLD, TILE_PICKAXE_GOLD, TOOL_PICKAXE, 1, 33, 7.0f, 3},
        {"Goldaxt", 1, false, TILE_AXE_GOLD, TILE_AXE_GOLD, TOOL_AXE, 1, 33, 7.0f, 4},
        {"Goldschaufel", 1, false, TILE_SHOVEL_GOLD, TILE_SHOVEL_GOLD, TOOL_SHOVEL, 1, 33, 7.0f, 3},
        {"Goldschwert", 1, false, TILE_SWORD_GOLD, TILE_SWORD_GOLD, TOOL_SWORD, 1, 33, 1.5f, 5},
    };
    if (id >= ITEM_STICK && id <= ITEM_LAST) return items[id - ITEM_STICK];
    if (id > 0 && id < BLOCK_COUNT) return cache[id];
    return BLOCK_ITEM_NONE;
}

ItemId blockDropItem(BlockId b) {
    if (b == COAL_ORE) return ITEM_COAL;
    if (b == DIAMOND_ORE) return ITEM_DIAMOND;
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
