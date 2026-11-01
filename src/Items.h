#pragma once
#include <cstdint>

#include "Block.h"

namespace oc {

// Gegenstands-IDs: 1..BLOCK_COUNT-1 sind Bloecke (gleiche Zahl wie die BlockId), darueber folgen Gegenstaende.
using ItemId = uint16_t;
constexpr ItemId ITEM_NONE = 0;
constexpr ItemId ITEM_MEAT = 200;  // Rohes Fleisch (heilt)

struct ItemStack {
    ItemId id = ITEM_NONE;
    int count = 0;

    bool empty() const { return id == ITEM_NONE || count <= 0; }
    void clear() {
        id = ITEM_NONE;
        count = 0;
    }
    bool operator==(const ItemStack& o) const { return id == o.id && count == o.count; }
};

struct ItemInfo {
    const char* name;
    int maxStack;
    bool isBlock;
    Tile iconTop;   // Symbol: Oberseite (Bloecke) bzw. Flachsymbol
    Tile iconSide;  // Symbol: Seite (Bloecke)
};

bool itemValid(ItemId id);
const ItemInfo& itemInfo(ItemId id);
inline bool itemIsBlock(ItemId id) { return id > 0 && id < BLOCK_COUNT; }
inline BlockId itemBlock(ItemId id) { return static_cast<BlockId>(id); }

// Gegenstand, der beim Abbauen eines Blocks faellt (ITEM_NONE = nichts)
ItemId blockDropItem(BlockId b);

// Auswahlliste fuer das Kreativ-Inventar
int creativeItemCount();
ItemId creativeItem(int index);

}  // namespace oc
