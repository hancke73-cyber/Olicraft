#include "Trades.h"

#include "Noise.h"

namespace oc {

namespace {
Trade T(ItemId ci, int cn, ItemId ri, int rn) { return Trade{ItemStack{ci, cn, 0}, ItemStack{ri, rn, 0}}; }
}  // namespace

const char* professionName(int p) {
    static const char* names[PROF_COUNT] = {"Bauer", "Bibliothekar", "Schmied", "Steinmetz", "Schaefer"};
    return names[p < 0 || p >= PROF_COUNT ? 0 : p];
}

const std::vector<Trade>& tradesFor(int p) {
    static const std::vector<Trade> all[PROF_COUNT] = {
        // Bauer
        {T(ITEM_WHEAT, 12, ITEM_GOLD_INGOT, 1), T(ITEM_BERRIES, 10, ITEM_GOLD_INGOT, 1), T(ITEM_GOLD_INGOT, 1, ITEM_BREAD, 4),
         T(ITEM_GOLD_INGOT, 1, ITEM_CARROT, 6), T(ITEM_GOLD_INGOT, 3, ITEM_HOE_IRON, 1), T(PUMPKIN, 2, ITEM_GOLD_INGOT, 1)},
        // Bibliothekar
        {T(ITEM_PAPER, 12, ITEM_GOLD_INGOT, 1), T(ITEM_GOLD_INGOT, 2, ITEM_BOOK, 2), T(ITEM_GOLD_INGOT, 4, BOOKSHELF, 1),
         T(ITEM_GOLD_INGOT, 1, ITEM_PAPER, 6), T(ITEM_GOLD_INGOT, 3, LAMP, 1), T(ITEM_GOLD_INGOT, 2, LANTERN, 2)},
        // Schmied
        {T(ITEM_COAL, 15, ITEM_GOLD_INGOT, 1), T(ITEM_IRON_INGOT, 4, ITEM_GOLD_INGOT, 1), T(ITEM_GOLD_INGOT, 3, ITEM_IRON_PICKAXE, 1),
         T(ITEM_GOLD_INGOT, 2, ITEM_IRON_SWORD, 1), T(ITEM_GOLD_INGOT, 9, ITEM_DIAMOND, 1)},
        // Steinmetz
        {T(COBBLE, 32, ITEM_GOLD_INGOT, 1), T(ITEM_CLAY_BALL, 10, ITEM_GOLD_INGOT, 1), T(ITEM_GOLD_INGOT, 1, BRICKS, 4),
         T(ITEM_GOLD_INGOT, 1, STONE_BRICKS, 6), T(ITEM_GOLD_INGOT, 1, makeStairs(2, 0), 4), T(ITEM_GOLD_INGOT, 1, WALL_COBBLE, 6)},
        // Schaefer
        {T(WOOL_WHITE, 8, ITEM_GOLD_INGOT, 1), T(ITEM_GOLD_INGOT, 1, WOOL_RED, 2), T(ITEM_GOLD_INGOT, 1, WOOL_BLUE, 2),
         T(ITEM_GOLD_INGOT, 1, WOOL_GREEN, 2), T(ITEM_GOLD_INGOT, 4, BED, 1)},
    };
    return all[p < 0 || p >= PROF_COUNT ? 0 : p];
}

int professionFor(int villageId, int index) {
    return static_cast<int>(hash3(villageId, index, 0x7F0, 0xBEEFu) % static_cast<uint32_t>(PROF_COUNT));
}

}  // namespace oc
