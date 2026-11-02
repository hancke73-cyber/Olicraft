#pragma once
#include <cstdint>

#include "Block.h"

namespace oc {

// Gegenstands-IDs: 1..BLOCK_COUNT-1 sind Bloecke (gleiche Zahl wie die BlockId), darueber folgen Gegenstaende.
using ItemId = uint16_t;
constexpr ItemId ITEM_NONE = 0;
constexpr ItemId ITEM_MEAT = 200;  // Rohes Fleisch (heilt)
constexpr ItemId ITEM_STICK = 201;
constexpr ItemId ITEM_WOODEN_PICKAXE = 202;
constexpr ItemId ITEM_WOODEN_AXE = 203;
constexpr ItemId ITEM_WOODEN_SHOVEL = 204;
constexpr ItemId ITEM_WOODEN_SWORD = 205;
constexpr ItemId ITEM_STONE_PICKAXE = 206;
constexpr ItemId ITEM_STONE_AXE = 207;
constexpr ItemId ITEM_STONE_SHOVEL = 208;
constexpr ItemId ITEM_STONE_SWORD = 209;
constexpr ItemId ITEM_COAL = 210;
constexpr ItemId ITEM_IRON_INGOT = 211;
constexpr ItemId ITEM_DIAMOND = 212;
constexpr ItemId ITEM_COOKED_MEAT = 213;  // Gebratenes Fleisch (heilt mehr)
constexpr ItemId ITEM_IRON_PICKAXE = 214;
constexpr ItemId ITEM_IRON_AXE = 215;
constexpr ItemId ITEM_IRON_SHOVEL = 216;
constexpr ItemId ITEM_IRON_SWORD = 217;
constexpr ItemId ITEM_DIAMOND_PICKAXE = 218;
constexpr ItemId ITEM_DIAMOND_AXE = 219;
constexpr ItemId ITEM_DIAMOND_SHOVEL = 220;
constexpr ItemId ITEM_DIAMOND_SWORD = 221;
constexpr ItemId ITEM_GOLD_INGOT = 222;
constexpr ItemId ITEM_GOLD_PICKAXE = 223;
constexpr ItemId ITEM_GOLD_AXE = 224;
constexpr ItemId ITEM_GOLD_SHOVEL = 225;
constexpr ItemId ITEM_GOLD_SWORD = 226;
constexpr ItemId ITEM_BRICK = 227;
constexpr ItemId ITEM_CLAY_BALL = 228;
constexpr ItemId ITEM_PAPER = 229;
constexpr ItemId ITEM_BOOK = 230;
constexpr ItemId ITEM_DYE_RED = 231;
constexpr ItemId ITEM_DYE_YELLOW = 232;
constexpr ItemId ITEM_DYE_BLUE = 233;
constexpr ItemId ITEM_DYE_GREEN = 234;
constexpr ItemId ITEM_DYE_BLACK = 235;
constexpr ItemId ITEM_BERRIES = 236;  // Beeren (heilen etwas)
constexpr ItemId ITEM_LAST = 236;

struct ItemStack {
    ItemId id = ITEM_NONE;
    int count = 0;
    int damage = 0;  // Abnutzung bei Werkzeugen (0 = neu)

    bool empty() const { return id == ITEM_NONE || count <= 0; }
    void clear() {
        id = ITEM_NONE;
        count = 0;
        damage = 0;
    }
    bool operator==(const ItemStack& o) const { return id == o.id && count == o.count && damage == o.damage; }
    // Gehoeren beide Stapel zusammen (gleiche Art, gleiche Abnutzung)?
    bool sameKind(const ItemStack& o) const { return id == o.id && damage == o.damage; }
};

struct ItemInfo {
    const char* name;
    int maxStack;
    bool isBlock;
    Tile iconTop;   // Symbol: Oberseite (Bloecke) bzw. Flachsymbol
    Tile iconSide;  // Symbol: Seite (Bloecke)
    ToolClass tool = TOOL_NONE;
    int tier = 0;            // 1 = Holz, 2 = Stein, 3 = Eisen, 4 = Diamant
    int maxDurability = 0;   // 0 = kein Werkzeug
    float speed = 1.0f;      // Abbaugeschwindigkeit bei passendem Block
    int attack = 2;          // Schaden gegen Mobs
    bool flatIcon = false;   // Blockgegenstand mit flachem Symbol (Fackel)
};

bool itemValid(ItemId id);
const ItemInfo& itemInfo(ItemId id);
inline bool itemIsBlock(ItemId id) { return id > 0 && id < BLOCK_COUNT; }
inline BlockId itemBlock(ItemId id) { return static_cast<BlockId>(id); }

// Abbau mit einem gehaltenen Gegenstand (leer = Hand)
bool canHarvest(BlockId b, const ItemStack& held);   // faellt etwas, wenn der Block so abgebaut wird?
float miningSeconds(BlockId b, const ItemStack& held);
int attackDamage(const ItemStack& held);

// Gegenstand, der beim Abbauen eines Blocks faellt (ITEM_NONE = nichts)
ItemId blockDropItem(BlockId b);

// Auswahlliste fuer das Kreativ-Inventar
int creativeItemCount();
ItemId creativeItem(int index);

}  // namespace oc
