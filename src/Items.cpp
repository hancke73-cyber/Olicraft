#include "Items.h"

namespace oc {

static const ItemInfo BLOCK_ITEM_NONE = {"", 0, false, TILE_STONE, TILE_STONE};

static const ItemId CREATIVE_LIST[] = {GRASS, DIRT, STONE, COBBLE, SAND, SNOW, LOG, PLANKS, LEAVES, GLASS, WATER, BEDROCK,
                                       ITEM_MEAT};

bool itemValid(ItemId id) {
    if (id == ITEM_MEAT) return true;
    if (id > 0 && id < BLOCK_COUNT) {
        // Fliessendes Wasser gibt es nicht als Gegenstand
        return !(id >= WATER_1 && id <= WATER_FALL);
    }
    return false;
}

const ItemInfo& itemInfo(ItemId id) {
    static ItemInfo cache[BLOCK_COUNT];
    static bool init = false;
    static const ItemInfo meat = {"Rohes Fleisch", 64, false, TILE_MEAT, TILE_MEAT};
    if (!init) {
        for (int i = 1; i < BLOCK_COUNT; ++i) {
            const BlockInfo& bi = blockInfo(static_cast<BlockId>(i));
            cache[i] = {bi.name, 64, true, bi.top, bi.side};
        }
        init = true;
    }
    if (id == ITEM_MEAT) return meat;
    if (id > 0 && id < BLOCK_COUNT) return cache[id];
    return BLOCK_ITEM_NONE;
}

ItemId blockDropItem(BlockId b) {
    const BlockInfo& bi = blockInfo(b);
    if (!bi.breakable || bi.drop == AIR) return ITEM_NONE;
    return static_cast<ItemId>(bi.drop);
}

int creativeItemCount() { return static_cast<int>(sizeof(CREATIVE_LIST) / sizeof(CREATIVE_LIST[0])); }
ItemId creativeItem(int index) { return CREATIVE_LIST[index]; }

}  // namespace oc
