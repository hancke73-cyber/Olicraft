#include "Crafting.h"

#include <algorithm>

namespace oc {

namespace {

Recipe make(std::vector<std::string> rows, std::initializer_list<std::pair<char, ItemId>> syms, ItemStack result) {
    Recipe r;
    r.rows = std::move(rows);
    for (auto& s : syms) {
        r.symbols[r.symbolCount] = s.first;
        r.items[r.symbolCount] = s.second;
        ++r.symbolCount;
    }
    r.result = result;
    return r;
}

ItemId ingredient(const Recipe& r, char c) {
    for (int i = 0; i < r.symbolCount; ++i)
        if (r.symbols[i] == c) return r.items[i];
    return ITEM_NONE;
}

std::vector<Recipe> buildRecipes() {
    std::vector<Recipe> v;
    v.push_back(make({"L"}, {{'L', LOG}}, {PLANKS, 4}));
    v.push_back(make({"P", "P"}, {{'P', PLANKS}}, {ITEM_STICK, 4}));
    v.push_back(make({"PP", "PP"}, {{'P', PLANKS}}, {CRAFTING_TABLE, 1}));
    v.push_back(make({"SS", "SS"}, {{'S', SAND}}, {GLASS, 2}));  // vereinfacht, es gibt noch keinen Ofen
    v.push_back(make({"C"}, {{'C', COBBLE}}, {STONE, 1}));       // vereinfacht, es gibt noch keinen Ofen

    struct Mat {
        ItemId material;
        ItemId pick, axe, shovel, sword;
    };
    const Mat mats[2] = {{PLANKS, ITEM_WOODEN_PICKAXE, ITEM_WOODEN_AXE, ITEM_WOODEN_SHOVEL, ITEM_WOODEN_SWORD},
                         {COBBLE, ITEM_STONE_PICKAXE, ITEM_STONE_AXE, ITEM_STONE_SHOVEL, ITEM_STONE_SWORD}};
    for (const Mat& m : mats) {
        v.push_back(make({"MMM", " S ", " S "}, {{'M', m.material}, {'S', ITEM_STICK}}, {m.pick, 1}));
        v.push_back(make({"MM", "MS", " S"}, {{'M', m.material}, {'S', ITEM_STICK}}, {m.axe, 1}));
        v.push_back(make({"M", "S", "S"}, {{'M', m.material}, {'S', ITEM_STICK}}, {m.shovel, 1}));
        v.push_back(make({"M", "M", "S"}, {{'M', m.material}, {'S', ITEM_STICK}}, {m.sword, 1}));
    }
    return v;
}

// Passt das Muster (evtl. gespiegelt) an Position (r0, c0)?
bool matchesAt(const Recipe& rec, const ItemStack* cells, int n, int r0, int c0, bool mirror) {
    for (int r = 0; r < rec.height(); ++r)
        for (int c = 0; c < rec.width(); ++c) {
            const char sym = rec.rows[r][mirror ? rec.width() - 1 - c : c];
            const ItemStack& cell = cells[(r0 + r) * n + (c0 + c)];
            if (sym == ' ') {
                if (!cell.empty()) return false;
            } else {
                if (cell.empty() || cell.id != ingredient(rec, sym)) return false;
            }
        }
    return true;
}

}  // namespace

const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> r = buildRecipes();
    return r;
}

ItemStack matchRecipe(const ItemStack* cells, int n) {
    // Begrenzungsrahmen der belegten Felder
    int minR = n, maxR = -1, minC = n, maxC = -1;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            if (!cells[r * n + c].empty()) {
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
                minC = std::min(minC, c);
                maxC = std::max(maxC, c);
            }
    if (maxR < 0) return ItemStack();
    const int h = maxR - minR + 1, w = maxC - minC + 1;
    for (const Recipe& rec : recipes()) {
        if (rec.height() != h || rec.width() != w) continue;
        if (matchesAt(rec, cells, n, minR, minC, false) || matchesAt(rec, cells, n, minR, minC, true)) return rec.result;
    }
    return ItemStack();
}

void consumeIngredients(ItemStack* cells, int n) {
    for (int i = 0; i < n * n; ++i) {
        if (cells[i].empty()) continue;
        if (--cells[i].count <= 0) cells[i].clear();
    }
}

}  // namespace oc
