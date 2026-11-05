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

    // Bauteile
    for (int m = 0; m < STAIR_MATERIALS; ++m) {
        const ItemId mat = stairMaterialBlock(m);
        v.push_back(make({"M  ", "MM ", "MMM"}, {{'M', mat}}, {makeStairs(m, 0), 4}));
        v.push_back(make({"MMM"}, {{'M', mat}}, {makeSlab(m, false), 6}));
        v.push_back(make({"M ", "MM"}, {{'M', mat}}, {makeWedge(m, 0), 4}));  // Dachkeile
    }
    v.push_back(make({"PSP", "PSP"}, {{'P', PLANKS}, {'S', ITEM_STICK}}, {FENCE, 3}));
    v.push_back(make({"SPS", "SPS"}, {{'P', PLANKS}, {'S', ITEM_STICK}}, {makeGate(0, false, 0), 2}));
    v.push_back(make({"CCC", "CCC"}, {{'C', COBBLE}}, {WALL_COBBLE, 6}));
    v.push_back(make({"BBB", "BBB"}, {{'B', BRICKS}}, {WALL_BRICKS, 6}));
    v.push_back(make({"GGG", "GGG"}, {{'G', GLASS}}, {GLASS_PANE, 16}));
    v.push_back(make({"III", "III"}, {{'I', ITEM_IRON_INGOT}}, {IRON_BARS, 16}));
    v.push_back(make({"IBI", "ISI"}, {{'I', ITEM_IRON_INGOT}, {'B', IRON_BARS}, {'S', ITEM_STICK}}, {makeGate(1, false, 0), 2}));
    v.push_back(make({" I ", "GTG", " I "}, {{'I', ITEM_IRON_INGOT}, {'G', GLASS}, {'T', TORCH}}, {LANTERN, 1}));
    v.push_back(make({"PP", "PP", "PP"}, {{'P', PLANKS}}, {ITEM_DOOR_WOOD, 3}));
    v.push_back(make({"II", "II", "II"}, {{'I', ITEM_IRON_INGOT}}, {ITEM_DOOR_IRON, 3}));
    // Ruestungen
    {
        const ItemId mats[4] = {ITEM_LEATHER, ITEM_IRON_INGOT, ITEM_GOLD_INGOT, ITEM_DIAMOND};
        for (int m = 0; m < 4; ++m) {
            v.push_back(make({"MMM", "M M"}, {{'M', mats[m]}}, {makeArmorItem(m, 0), 1}));
            v.push_back(make({"M M", "MMM", "MMM"}, {{'M', mats[m]}}, {makeArmorItem(m, 1), 1}));
            v.push_back(make({"MMM", "M M", "M M"}, {{'M', mats[m]}}, {makeArmorItem(m, 2), 1}));
            v.push_back(make({"M M", "M M"}, {{'M', mats[m]}}, {makeArmorItem(m, 3), 1}));
        }
    }
    v.push_back(make({"S S", "SSS", "S S"}, {{'S', ITEM_STICK}}, {LADDER_BASE, 3}));
    v.push_back(make({"I I", " I "}, {{'I', ITEM_IRON_INGOT}}, {ITEM_BUCKET, 1}));
    // Steampunk
    v.push_back(make({"III"}, {{'I', ITEM_IRON_INGOT}}, {PIPE, 6}));
    v.push_back(make({"I", "I", "I"}, {{'I', ITEM_IRON_INGOT}}, {SHAFT, 6}));
    v.push_back(make({"III", "IFI", "III"}, {{'I', ITEM_IRON_INGOT}, {'F', FURNACE}}, {BOILER, 1}));
    v.push_back(make({"IPI", "IGI", "III"}, {{'I', ITEM_IRON_INGOT}, {'P', PIPE}, {'G', ITEM_GOLD_INGOT}}, {MOTOR, 1}));
    v.push_back(make({"GIG", "PBP", "III"}, {{'G', GLASS}, {'I', ITEM_IRON_INGOT}, {'P', PIPE}, {'B', ITEM_BUCKET}}, {STILL, 1}));
    v.push_back(make({"III", "IMI", "IKI"}, {{'I', ITEM_IRON_INGOT}, {'M', MOTOR}, {'K', ITEM_IRON_PICKAXE}}, {DRILL_BASE, 1}));
    v.push_back(make({"III", "SSS"}, {{'I', ITEM_IRON_INGOT}, {'S', ITEM_STICK}}, {CONVEYOR_BASE, 6}));
    v.push_back(make({"I I", "GIG", "I I"}, {{'I', ITEM_IRON_INGOT}, {'G', GLASS}}, {LIFT, 2}));
    v.push_back(make({"I I", "I I", "I I"}, {{'I', ITEM_IRON_INGOT}}, {WATER_PIPE, 6}));
    v.push_back(make({"IWI", "IBI", "III"}, {{'I', ITEM_IRON_INGOT}, {'W', WATER_PIPE}, {'B', ITEM_BUCKET}}, {PUMP, 1}));
    v.push_back(make({"III", " W "}, {{'I', ITEM_IRON_INGOT}, {'W', WATER_PIPE}}, {OUTLET, 1}));
    // Fahrzeuge
    v.push_back(make({"P P", "IBI", "PPP"}, {{'P', PLANKS}, {'I', ITEM_IRON_INGOT}, {'B', BOILER}}, {ITEM_STEAMBOAT, 1}));
    v.push_back(make({"PGP", "P P", "III"}, {{'P', PLANKS}, {'G', GLASS}, {'I', ITEM_IRON_INGOT}}, {ITEM_GONDOLA, 1}));
    v.push_back(make({" I ", "IBI", "IMI"}, {{'I', ITEM_IRON_INGOT}, {'B', BOILER}, {'M', MOTOR}}, {ITEM_PROPELLER, 1}));
    v.push_back(make({"WWW", "WWW", "S S"}, {{'W', WOOL_WHITE}, {'S', ITEM_STICK}}, {ITEM_BALLOON_S, 1}));
    v.push_back(makeList({ITEM_BALLOON_S, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE}, {ITEM_BALLOON_M, 1}));
    v.push_back(makeList({ITEM_BALLOON_M, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE, WOOL_WHITE}, {ITEM_BALLOON_L, 1}));
    v.push_back(make({"PPP", "PPP", " S "}, {{'P', PLANKS}, {'S', ITEM_STICK}}, {SIGN_BASE, 3}));
    // Landwirtschaft
    v.push_back(make({"WWW"}, {{'W', ITEM_WHEAT}}, {ITEM_BREAD, 1}));
    v.push_back(make({"W"}, {{'W', ITEM_WHEAT}}, {ITEM_SEEDS, 1}));

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
    const ItemId hoes[5] = {ITEM_HOE_WOOD, ITEM_HOE_STONE, ITEM_HOE_IRON, ITEM_HOE_DIAMOND, ITEM_HOE_GOLD};
    for (int i = 0; i < 5; ++i)
        v.push_back(make({"MM", " S", " S"}, {{'M', mats[i].material}, {'S', ITEM_STICK}}, {hoes[i], 1}));
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
    static const std::vector<Recipe> all = buildRecipes();
    static const std::vector<Recipe> plain = [] {
        std::vector<Recipe> v;
        for (const Recipe& r : all)
            if (!isSteampunkItem(r.result.id)) v.push_back(r);
        return v;
    }();
    return steampunkEnabled() ? all : plain;
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
        case FENCE:
        case ITEM_DOOR_WOOD: return 10.0f;
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
