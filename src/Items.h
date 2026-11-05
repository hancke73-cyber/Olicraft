#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include "Block.h"

namespace oc {

// Gegenstands-IDs: 1..BLOCK_COUNT-1 sind Bloecke (gleiche Zahl wie die BlockId), ab ITEM_BASE folgen Gegenstaende.
// Bloecke sind 8 Bit gross (hoechstens 255), daher beginnen die Gegenstaende erst bei 256. Spielstaende bis Version 6 hatten sie ab 200.
using ItemId = uint16_t;
constexpr ItemId ITEM_BASE = 256;
constexpr ItemId OLD_ITEM_BASE = 200;  // fuer alte Spielstaende
constexpr ItemId ITEM_NONE = 0;
constexpr ItemId ITEM_MEAT = ITEM_BASE + 0;  // Rohes Fleisch (heilt)
constexpr ItemId ITEM_STICK = ITEM_BASE + 1;
constexpr ItemId ITEM_WOODEN_PICKAXE = ITEM_BASE + 2;
constexpr ItemId ITEM_WOODEN_AXE = ITEM_BASE + 3;
constexpr ItemId ITEM_WOODEN_SHOVEL = ITEM_BASE + 4;
constexpr ItemId ITEM_WOODEN_SWORD = ITEM_BASE + 5;
constexpr ItemId ITEM_STONE_PICKAXE = ITEM_BASE + 6;
constexpr ItemId ITEM_STONE_AXE = ITEM_BASE + 7;
constexpr ItemId ITEM_STONE_SHOVEL = ITEM_BASE + 8;
constexpr ItemId ITEM_STONE_SWORD = ITEM_BASE + 9;
constexpr ItemId ITEM_COAL = ITEM_BASE + 10;
constexpr ItemId ITEM_IRON_INGOT = ITEM_BASE + 11;
constexpr ItemId ITEM_DIAMOND = ITEM_BASE + 12;
constexpr ItemId ITEM_COOKED_MEAT = ITEM_BASE + 13;  // Gebratenes Fleisch (heilt mehr)
constexpr ItemId ITEM_IRON_PICKAXE = ITEM_BASE + 14;
constexpr ItemId ITEM_IRON_AXE = ITEM_BASE + 15;
constexpr ItemId ITEM_IRON_SHOVEL = ITEM_BASE + 16;
constexpr ItemId ITEM_IRON_SWORD = ITEM_BASE + 17;
constexpr ItemId ITEM_DIAMOND_PICKAXE = ITEM_BASE + 18;
constexpr ItemId ITEM_DIAMOND_AXE = ITEM_BASE + 19;
constexpr ItemId ITEM_DIAMOND_SHOVEL = ITEM_BASE + 20;
constexpr ItemId ITEM_DIAMOND_SWORD = ITEM_BASE + 21;
constexpr ItemId ITEM_GOLD_INGOT = ITEM_BASE + 22;
constexpr ItemId ITEM_GOLD_PICKAXE = ITEM_BASE + 23;
constexpr ItemId ITEM_GOLD_AXE = ITEM_BASE + 24;
constexpr ItemId ITEM_GOLD_SHOVEL = ITEM_BASE + 25;
constexpr ItemId ITEM_GOLD_SWORD = ITEM_BASE + 26;
constexpr ItemId ITEM_BRICK = ITEM_BASE + 27;
constexpr ItemId ITEM_CLAY_BALL = ITEM_BASE + 28;
constexpr ItemId ITEM_PAPER = ITEM_BASE + 29;
constexpr ItemId ITEM_BOOK = ITEM_BASE + 30;
constexpr ItemId ITEM_DYE_RED = ITEM_BASE + 31;
constexpr ItemId ITEM_DYE_YELLOW = ITEM_BASE + 32;
constexpr ItemId ITEM_DYE_BLUE = ITEM_BASE + 33;
constexpr ItemId ITEM_DYE_GREEN = ITEM_BASE + 34;
constexpr ItemId ITEM_DYE_BLACK = ITEM_BASE + 35;
constexpr ItemId ITEM_BERRIES = ITEM_BASE + 36;  // Beeren (heilen etwas)
constexpr ItemId ITEM_SEEDS = ITEM_BASE + 37;
constexpr ItemId ITEM_WHEAT = ITEM_BASE + 38;
constexpr ItemId ITEM_BREAD = ITEM_BASE + 39;  // heilt 5
constexpr ItemId ITEM_CARROT = ITEM_BASE + 40;  // heilt 3, laesst sich pflanzen
constexpr ItemId ITEM_HOE_WOOD = ITEM_BASE + 41;
constexpr ItemId ITEM_HOE_STONE = ITEM_BASE + 42;
constexpr ItemId ITEM_HOE_IRON = ITEM_BASE + 43;
constexpr ItemId ITEM_HOE_GOLD = ITEM_BASE + 44;
constexpr ItemId ITEM_HOE_DIAMOND = ITEM_BASE + 45;
constexpr ItemId ITEM_DOOR_WOOD = ITEM_BASE + 46;
constexpr ItemId ITEM_DOOR_IRON = ITEM_BASE + 47;
constexpr ItemId ITEM_LEATHER = ITEM_BASE + 48;
// Ruestungen: ITEM_ARMOR_BASE + material*4 + Teil; Material 0 Leder, 1 Eisen, 2 Gold, 3 Diamant; Teil 0 Helm, 1 Brustpanzer, 2 Hose, 3 Stiefel
constexpr ItemId ITEM_ARMOR_BASE = ITEM_BASE + 49;
constexpr ItemId ITEM_LAST = ITEM_BASE + 64;
inline bool isArmorItem(ItemId id) { return id >= ITEM_ARMOR_BASE && id < ITEM_ARMOR_BASE + 16; }
inline int armorSlotOf(ItemId id) { return (id - ITEM_ARMOR_BASE) % 4; }
inline int armorMaterialOf(ItemId id) { return (id - ITEM_ARMOR_BASE) / 4; }
inline ItemId makeArmorItem(int material, int slot) { return static_cast<ItemId>(ITEM_ARMOR_BASE + material * 4 + slot); }

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
    int armorSlot = -1;      // Ruestung: 0 Helm, 1 Brustpanzer, 2 Hose, 3 Stiefel
    int armorPoints = 0;     // Schutz (20 = volle Diamantruestung)
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

// Kreativ-Inventar mit Reitern nach Themen. Reiter 0..CREATIVE_TAB_COUNT-1 sind Themen, CREATIVE_SEARCH_TAB ist die Suche.
constexpr int CREATIVE_TAB_COUNT = 8;
constexpr int CREATIVE_SEARCH_TAB = CREATIVE_TAB_COUNT;
const char* creativeTabName(int tab);                    // Gelaende, Baumaterial, Dekoration, Pflanzen, Stationen, Werkzeuge, Rohstoffe, Essen, Suche
ItemId creativeTabIcon(int tab);                         // Symbol des Reiters (ITEM_NONE bei der Suche)
const std::vector<ItemId>& creativeTabItems(int tab);    // jeder Gegenstand steht in genau einem Themenreiter
// Alle Gegenstaende, deren Name den Suchtext enthaelt (ohne Gross/Klein, Umlaute als ae, oe, ue); leerer Text: alle
std::vector<ItemId> creativeSearch(const std::string& query);

}  // namespace oc
