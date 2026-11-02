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

Recipe makeList(std::vector<ItemId> list, ItemStack result) {
    Recipe r;
    r.shapeless = true;
    r.list = std::move(list);
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
    v.push_back(make({"CCC", "C C", "CCC"}, {{'C', COBBLE}}, {FURNACE, 1}));
    v.push_back(make({"C", "S"}, {{'C', ITEM_COAL}, {'S', ITEM_STICK}}, {TORCH, 4}));

    // Holzarten
    v.push_back(make({"L"}, {{'L', BIRCH_LOG}}, {PLANKS, 4}));
    v.push_back(make({"L"}, {{'L', SPRUCE_LOG}}, {PLANKS, 4}));
    // Stationen und Einrichtung
    v.push_back(make({"PPP", "P P", "PPP"}, {{'P', PLANKS}}, {CHEST, 1}));
    for (ItemId wool : {WOOL_WHITE, WOOL_RED, WOOL_BLUE, WOOL_GREEN, WOOL_YELLOW, WOOL_BLACK})
        v.push_back(make({"WWW", "PPP"}, {{'W', wool}, {'P', PLANKS}}, {BED, 1}));
    v.push_back(make({"PPP", "BBB", "PPP"}, {{'P', PLANKS}, {'B', ITEM_BOOK}}, {BOOKSHELF, 1}));
    v.push_back(make({"P", "P", "P"}, {{'P', ITEM_PAPER}}, {ITEM_BOOK, 1}));
    v.push_back(make({"SSS"}, {{'S', SUGAR_CANE}}, {ITEM_PAPER, 3}));
    v.push_back(make({" G ", "GTG", " G "}, {{'G', GLASS}, {'T', TORCH}}, {LAMP, 1}));
    // Stein, Ton, Sand
    v.push_back(make({"SS", "SS"}, {{'S', STONE}}, {STONE_BRICKS, 4}));
    v.push_back(make({"SS", "SS"}, {{'S', SAND}}, {SANDSTONE, 1}));
    v.push_back(make({"C"}, {{'C', CLAY}}, {ITEM_CLAY_BALL, 4}));
    v.push_back(make({"CC", "CC"}, {{'C', ITEM_CLAY_BALL}}, {CLAY, 1}));
    v.push_back(make({"BB", "BB"}, {{'B', ITEM_BRICK}}, {BRICKS, 1}));
    // Materialbloecke und zurueck
    struct Pack {
        ItemId item;
        BlockId block;
    };
    for (const Pack& p : {Pack{ITEM_IRON_INGOT, IRON_BLOCK}, Pack{ITEM_GOLD_INGOT, GOLD_BLOCK}, Pack{ITEM_DIAMOND, DIAMOND_BLOCK},
                          Pack{ITEM_COAL, COAL_BLOCK}}) {
        v.push_back(make({"III", "III", "III"}, {{'I', p.item}}, {p.block, 1}));
        v.push_back(make({"B"}, {{'B', p.block}}, {p.item, 9}));
    }
    // Farben und Wolle
    v.push_back(make({"F"}, {{'F', FLOWER_RED}}, {ITEM_DYE_RED, 1}));
    v.push_back(make({"F"}, {{'F', FLOWER_YELLOW}}, {ITEM_DYE_YELLOW, 1}));
    v.push_back(make({"F"}, {{'F', FLOWER_BLUE}}, {ITEM_DYE_BLUE, 1}));
    v.push_back(make({"C"}, {{'C', ITEM_COAL}}, {ITEM_DYE_BLACK, 1}));
    v.push_back(makeList({WOOL_WHITE, ITEM_DYE_RED}, {WOOL_RED, 1}));
    v.push_back(makeList({WOOL_WHITE, ITEM_DYE_BLUE}, {WOOL_BLUE, 1}));
    v.push_back(makeList({WOOL_WHITE, ITEM_DYE_YELLOW}, {WOOL_YELLOW, 1}));
    v.push_back(makeList({WOOL_WHITE, ITEM_DYE_GREEN}, {WOOL_GREEN, 1}));
    v.push_back(makeList({WOOL_WHITE, ITEM_DYE_BLACK}, {WOOL_BLACK, 1}));

    struct Mat {
        ItemId material;
        ItemId pick, axe, shovel, sword;
    };
    const Mat mats[5] = {{PLANKS, ITEM_WOODEN_PICKAXE, ITEM_WOODEN_AXE, ITEM_WOODEN_SHOVEL, ITEM_WOODEN_SWORD},
                         {COBBLE, ITEM_STONE_PICKAXE, ITEM_STONE_AXE, ITEM_STONE_SHOVEL, ITEM_STONE_SWORD},
                         {ITEM_IRON_INGOT, ITEM_IRON_PICKAXE, ITEM_IRON_AXE, ITEM_IRON_SHOVEL, ITEM_IRON_SWORD},
                         {ITEM_DIAMOND, ITEM_DIAMOND_PICKAXE, ITEM_DIAMOND_AXE, ITEM_DIAMOND_SHOVEL, ITEM_DIAMOND_SWORD},
                         {ITEM_GOLD_INGOT, ITEM_GOLD_PICKAXE, ITEM_GOLD_AXE, ITEM_GOLD_SHOVEL, ITEM_GOLD_SWORD}};
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

namespace {
bool matchesShapeless(const Recipe& rec, const ItemStack* cells, int n) {
    std::vector<ItemId> have;
    for (int i = 0; i < n * n; ++i)
        if (!cells[i].empty()) have.push_back(cells[i].id);
    if (have.size() != rec.list.size()) return false;
    std::vector<ItemId> want = rec.list;
    std::sort(have.begin(), have.end());
    std::sort(want.begin(), want.end());
    return have == want;
}
}  // namespace

std::vector<Ingredient> recipeIngredients(const Recipe& r) {
    std::vector<Ingredient> out;
    auto add = [&](ItemId id) {
        for (Ingredient& i : out)
            if (i.item == id) {
                ++i.count;
                return;
            }
        out.push_back({id, 1});
    };
    if (r.shapeless) {
        for (ItemId id : r.list) add(id);
    } else {
        for (const std::string& row : r.rows)
            for (char ch : row)
                if (ch != ' ') add(ingredient(r, ch));
    }
    return out;
}

std::vector<ItemId> recipeLayout(const Recipe& r, int n) {
    std::vector<ItemId> cells(static_cast<size_t>(n * n), ITEM_NONE);
    if (r.shapeless) {
        for (size_t i = 0; i < r.list.size() && i < cells.size(); ++i) cells[i] = r.list[i];
        return cells;
    }
    for (int row = 0; row < r.height() && row < n; ++row)
        for (int col = 0; col < r.width() && col < n; ++col) {
            const char ch = r.rows[static_cast<size_t>(row)][static_cast<size_t>(col)];
            if (ch != ' ') cells[static_cast<size_t>(row * n + col)] = ingredient(r, ch);
        }
    return cells;
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
        if (rec.shapeless) {
            if (matchesShapeless(rec, cells, n)) return rec.result;
            continue;
        }
        if (rec.height() != h || rec.width() != w) continue;
        if (matchesAt(rec, cells, n, minR, minC, false) || matchesAt(rec, cells, n, minR, minC, true)) return rec.result;
    }
    return ItemStack();
}

const SmeltRecipe* smeltRecipe(ItemId input) {
    static const SmeltRecipe table[] = {
        {IRON_ORE, {ITEM_IRON_INGOT, 1}},
        {GOLD_ORE, {ITEM_GOLD_INGOT, 1}},
        {SAND, {GLASS, 1}},
        {COBBLE, {STONE, 1}},
        {ITEM_MEAT, {ITEM_COOKED_MEAT, 1}},
        {ITEM_CLAY_BALL, {ITEM_BRICK, 1}},
        {CACTUS, {ITEM_DYE_GREEN, 1}},
        {LOG, {ITEM_COAL, 1}},
        {BIRCH_LOG, {ITEM_COAL, 1}},
        {SPRUCE_LOG, {ITEM_COAL, 1}},
    };
    for (const SmeltRecipe& r : table)
        if (r.input == input) return &r;
    return nullptr;
}

float fuelSeconds(ItemId item) {
    switch (item) {
        case ITEM_COAL: return 80.0f;
        case COAL_BLOCK: return 800.0f;
        case LOG:
        case BIRCH_LOG:
        case SPRUCE_LOG:
        case PLANKS:
        case CHEST:
        case BOOKSHELF:
        case CRAFTING_TABLE: return 15.0f;
        case DEAD_BUSH: return 5.0f;
        case ITEM_STICK: return 5.0f;
        case ITEM_WOODEN_PICKAXE:
        case ITEM_WOODEN_AXE:
        case ITEM_WOODEN_SHOVEL:
        case ITEM_WOODEN_SWORD: return 10.0f;
        default: return 0.0f;
    }
}

void consumeIngredients(ItemStack* cells, int n) {
    for (int i = 0; i < n * n; ++i) {
        if (cells[i].empty()) continue;
        if (--cells[i].count <= 0) cells[i].clear();
    }
}

}  // namespace oc
