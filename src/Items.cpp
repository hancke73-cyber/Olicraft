#include "Items.h"

#include <array>

#include <cctype>

namespace oc {

static const ItemInfo BLOCK_ITEM_NONE = {"", 0, false, TILE_STONE, TILE_STONE, TOOL_NONE, 0, 0, 1.0f, 2};

// Reihenfolge der Reiter: Gelaende, Baumaterial, Dekoration, Pflanzen, Stationen, Werkzeuge, Rohstoffe, Essen
static const std::vector<ItemId> TAB_ITEMS[CREATIVE_TAB_COUNT] = {
    // Gelaende
    {GRASS, DIRT, FARMLAND, STONE, COBBLE, SAND, GRAVEL, CLAY, SNOW, SANDSTONE, OBSIDIAN, BEDROCK,
     COAL_ORE, IRON_ORE, GOLD_ORE, DIAMOND_ORE, LOG, BIRCH_LOG, SPRUCE_LOG, LEAVES, BIRCH_LEAVES, SPRUCE_LEAVES, WATER, LAVA},
    // Baumaterial
    {PLANKS, BRICKS, STONE_BRICKS, GLASS, IRON_BLOCK, GOLD_BLOCK, DIAMOND_BLOCK, COAL_BLOCK,
     STAIRS_BASE, STAIRS_BASE + 4, STAIRS_BASE + 8, STAIRS_BASE + 12, STAIRS_BASE + 16,
     SLAB_BASE, SLAB_BASE + 2, SLAB_BASE + 4, SLAB_BASE + 6, SLAB_BASE + 8,
     WEDGE_BASE, WEDGE_BASE + 4, WEDGE_BASE + 8, WEDGE_BASE + 12, WEDGE_BASE + 16,
     FENCE, WALL_COBBLE, WALL_BRICKS, GATE_BASE, GATE_BASE + 4, ITEM_DOOR_WOOD, ITEM_DOOR_IRON},
    // Dekoration
    {WOOL_WHITE, WOOL_RED, WOOL_BLUE, WOOL_GREEN, WOOL_YELLOW, WOOL_BLACK, GLASS_PANE, IRON_BARS, LANTERN, TORCH, LAMP,
     BOOKSHELF, LADDER_BASE, PUMPKIN, SIGN_BASE},
    // Pflanzen
    {TALL_GRASS, FERN, FLOWER_RED, FLOWER_YELLOW, FLOWER_BLUE, FLOWER_WHITE, MUSHROOM_RED, MUSHROOM_BROWN, DEAD_BUSH,
     SUGAR_CANE, BERRY_BUSH, CACTUS, ITEM_SEEDS},
    // Stationen
    {CRAFTING_TABLE, FURNACE, CHEST, BED, BOILER, PIPE, SHAFT, MOTOR, STILL, DRILL_BASE, CONVEYOR_BASE, LIFT, PUMP, WATER_PIPE, OUTLET},
    // Werkzeuge und Ruestung
    {ITEM_WOODEN_PICKAXE, ITEM_WOODEN_AXE, ITEM_WOODEN_SHOVEL, ITEM_WOODEN_SWORD, ITEM_HOE_WOOD,
     ITEM_STONE_PICKAXE, ITEM_STONE_AXE, ITEM_STONE_SHOVEL, ITEM_STONE_SWORD, ITEM_HOE_STONE,
     ITEM_IRON_PICKAXE, ITEM_IRON_AXE, ITEM_IRON_SHOVEL, ITEM_IRON_SWORD, ITEM_HOE_IRON,
     ITEM_GOLD_PICKAXE, ITEM_GOLD_AXE, ITEM_GOLD_SHOVEL, ITEM_GOLD_SWORD, ITEM_HOE_GOLD,
     ITEM_DIAMOND_PICKAXE, ITEM_DIAMOND_AXE, ITEM_DIAMOND_SHOVEL, ITEM_DIAMOND_SWORD, ITEM_HOE_DIAMOND,
     ITEM_ARMOR_BASE, ITEM_ARMOR_BASE + 1, ITEM_ARMOR_BASE + 2, ITEM_ARMOR_BASE + 3,
     ITEM_ARMOR_BASE + 4, ITEM_ARMOR_BASE + 5, ITEM_ARMOR_BASE + 6, ITEM_ARMOR_BASE + 7,
     ITEM_ARMOR_BASE + 8, ITEM_ARMOR_BASE + 9, ITEM_ARMOR_BASE + 10, ITEM_ARMOR_BASE + 11,
     ITEM_ARMOR_BASE + 12, ITEM_ARMOR_BASE + 13, ITEM_ARMOR_BASE + 14, ITEM_ARMOR_BASE + 15,
     ITEM_BUCKET, ITEM_BUCKET_WATER, ITEM_BUCKET_LAVA, ITEM_BUCKET_DISTILLED},
    // Rohstoffe
    {ITEM_STICK, ITEM_COAL, ITEM_IRON_INGOT, ITEM_GOLD_INGOT, ITEM_DIAMOND, ITEM_CLAY_BALL, ITEM_BRICK, ITEM_PAPER, ITEM_BOOK,
     ITEM_DYE_RED, ITEM_DYE_YELLOW, ITEM_DYE_BLUE, ITEM_DYE_GREEN, ITEM_DYE_BLACK, ITEM_LEATHER, ITEM_TAR, ITEM_FUEL_OIL, ITEM_SALT},
    // Essen
    {ITEM_WHEAT, ITEM_BREAD, ITEM_CARROT, ITEM_MEAT, ITEM_COOKED_MEAT, ITEM_BERRIES},
};

static bool g_steampunk = true;
bool steampunkEnabled() { return g_steampunk; }
void setSteampunkEnabled(bool on) { g_steampunk = on; }
bool isSteampunkItem(ItemId id) { return (id >= BOILER && id < BLOCK_COUNT) || (id >= ITEM_TAR && id <= ITEM_LAST); }

// Themenlisten ohne die Steampunk-Gegenstaende (fuer ausgeschaltete Erweiterung)
static const std::vector<ItemId>* const tabLists(bool steam) {
    static const std::array<std::vector<ItemId>, CREATIVE_TAB_COUNT> plain = [] {
        std::array<std::vector<ItemId>, CREATIVE_TAB_COUNT> a;
        for (int t = 0; t < CREATIVE_TAB_COUNT; ++t)
            for (ItemId id : TAB_ITEMS[t])
                if (!isSteampunkItem(id)) a[static_cast<size_t>(t)].push_back(id);
        return a;
    }();
    return steam ? TAB_ITEMS : plain.data();
}

static const std::vector<ItemId>& creativeFlat() {
    static const auto build = [](bool steam) {
        std::vector<ItemId> v;
        for (int t = 0; t < CREATIVE_TAB_COUNT; ++t) v.insert(v.end(), tabLists(steam)[t].begin(), tabLists(steam)[t].end());
        return v;
    };
    static const std::vector<ItemId> all = build(true), plain = build(false);
    return g_steampunk ? all : plain;
}

bool itemValid(ItemId id) {
    if (id == ITEM_MEAT || (id >= ITEM_STICK && id <= ITEM_LAST)) return true;
    if (id > 0 && id < BLOCK_COUNT) {
        // Fliessendes Wasser gibt es nicht als Gegenstand, ebenso keine Zustandsvarianten (nur die Grundform)
        if ((id >= WATER_1 && id <= WATER_FALL) || (id >= LAVA_1 && id <= LAVA_FALL) || id == FURNACE_LIT || (id > TORCH && id <= TORCH_NZ))
            return false;
        const BlockId b = static_cast<BlockId>(id);
        if (isStairs(b)) return stairFacing(b) == 0 && !stairIsTop(b);
        if (isWedge(b)) return wedgeFacing(b) == 0;
        if (isSign(b)) return b == SIGN_BASE;
        if (b == BOILER_LIT) return false;
        if (isDrill(b)) return b == DRILL_BASE;
        if (isConveyor(b)) return b == CONVEYOR_BASE;
        if (isSlab(b)) return !slabIsTop(b);
        if (isDoor(b) || b == LANTERN_HANG || isCrop(b) || isBedPart(b)) return false;
        if (isLadder(b)) return b == LADDER_BASE;
        if (isGate(b)) return b == makeGate(0, false, 0) || b == makeGate(1, false, 0);
        return true;
    }
    return false;
}

#define ARM(name, mat, slot, dur, pts) {name, 1, false, static_cast<Tile>(TILE_ARMOR_BASE + (mat) * 4 + (slot)), static_cast<Tile>(TILE_ARMOR_BASE + (mat) * 4 + (slot)), TOOL_NONE, 0, dur, 1.0f, 2, false, slot, pts}
#define ARMOR_ENTRIES \
    ARM("Lederkappe", 0, 0, 55, 1), ARM("Lederjacke", 0, 1, 80, 3), ARM("Lederhose", 0, 2, 75, 2), ARM("Lederstiefel", 0, 3, 65, 1), \
    ARM("Eisenhelm", 1, 0, 165, 2), ARM("Eisenbrustpanzer", 1, 1, 240, 6), ARM("Eisenhose", 1, 2, 225, 5), ARM("Eisenstiefel", 1, 3, 195, 2), \
    ARM("Goldhelm", 2, 0, 77, 2), ARM("Goldbrustpanzer", 2, 1, 112, 5), ARM("Goldhose", 2, 2, 105, 3), ARM("Goldstiefel", 2, 3, 91, 1), \
    ARM("Diamanthelm", 3, 0, 363, 3), ARM("Diamantbrustpanzer", 3, 1, 528, 8), ARM("Diamanthose", 3, 2, 495, 6), ARM("Diamantstiefel", 3, 3, 429, 3)

// Bloecke, deren Symbol flach (Kachel statt Wuerfel) gezeichnet wird
static bool flatIcon(BlockId b) {
    return b == TORCH || isPlant(b) || isPane(b) || isLantern(b) || isGate(b) || isLadder(b) || isSign(b) || b == PIPE || b == WATER_PIPE ||
           b == SHAFT || b == LIFT || isConveyor(b);
}

const ItemInfo& itemInfo(ItemId id) {
    static ItemInfo cache[BLOCK_COUNT];
    static bool init = false;
    static const ItemInfo meat = {"Rohes Fleisch", 64, false, TILE_MEAT, TILE_MEAT, TOOL_NONE, 0, 0, 1.0f, 2};
    if (!init) {
        for (int i = 1; i < BLOCK_COUNT; ++i) {
            const BlockInfo& bi = blockInfo(static_cast<BlockId>(i));
            cache[i] = {bi.name, 64, true, bi.top, bi.side, TOOL_NONE, 0, 0, 1.0f, 2, flatIcon(static_cast<BlockId>(i))};
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
        {"Ziegel", 64, false, TILE_BRICK, TILE_BRICK, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Tonklumpen", 64, false, TILE_CLAY_BALL, TILE_CLAY_BALL, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Papier", 64, false, TILE_PAPER, TILE_PAPER, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Buch", 64, false, TILE_BOOK, TILE_BOOK, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Rote Farbe", 64, false, TILE_DYE_RED, TILE_DYE_RED, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Gelbe Farbe", 64, false, TILE_DYE_YELLOW, TILE_DYE_YELLOW, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Blaue Farbe", 64, false, TILE_DYE_BLUE, TILE_DYE_BLUE, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Gruene Farbe", 64, false, TILE_DYE_GREEN, TILE_DYE_GREEN, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Schwarze Farbe", 64, false, TILE_DYE_BLACK, TILE_DYE_BLACK, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Beeren", 64, false, TILE_BERRIES, TILE_BERRIES, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Samen", 64, false, TILE_SEEDS, TILE_SEEDS, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Weizen", 64, false, TILE_WHEAT, TILE_WHEAT, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Brot", 64, false, TILE_BREAD, TILE_BREAD, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Karotte", 64, false, TILE_CARROT, TILE_CARROT, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Holzhacke", 1, false, TILE_HOE_WOOD, TILE_HOE_WOOD, TOOL_HOE, 1, 60, 2.0f, 2},
        {"Steinhacke", 1, false, TILE_HOE_STONE, TILE_HOE_STONE, TOOL_HOE, 2, 132, 3.5f, 2},
        {"Eisenhacke", 1, false, TILE_HOE_IRON, TILE_HOE_IRON, TOOL_HOE, 3, 250, 5.5f, 2},
        {"Goldhacke", 1, false, TILE_HOE_GOLD, TILE_HOE_GOLD, TOOL_HOE, 1, 33, 7.0f, 2},
        {"Diamanthacke", 1, false, TILE_HOE_DIAMOND, TILE_HOE_DIAMOND, TOOL_HOE, 4, 1560, 8.0f, 2},
        {"Holztuer", 64, false, TILE_DOOR_WOOD_ITEM, TILE_DOOR_WOOD_ITEM, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Eisentuer", 64, false, TILE_DOOR_IRON_ITEM, TILE_DOOR_IRON_ITEM, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Leder", 64, false, TILE_LEATHER, TILE_LEATHER, TOOL_NONE, 0, 0, 1.0f, 2},
        ARMOR_ENTRIES,
        {"Eimer", 16, false, TILE_BUCKET, TILE_BUCKET, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Wassereimer", 1, false, TILE_BUCKET_WATER, TILE_BUCKET_WATER, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Lavaeimer", 1, false, TILE_BUCKET_LAVA, TILE_BUCKET_LAVA, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Teer", 64, false, TILE_TAR, TILE_TAR, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Heizoel", 64, false, TILE_FUEL_OIL, TILE_FUEL_OIL, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Salz", 64, false, TILE_SALT, TILE_SALT, TOOL_NONE, 0, 0, 1.0f, 2},
        {"Destilliertes Wasser", 1, false, TILE_BUCKET_DISTILLED, TILE_BUCKET_DISTILLED, TOOL_NONE, 0, 0, 1.0f, 2},
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
        else if (ii.tool == TOOL_SWORD && isLeaves(b)) t /= ii.speed;
    }
    if (!canHarvest(b, held)) t *= 4.0f;  // ohne passendes Werkzeug muehsam und ohne Beute
    return t;
}

int attackDamage(const ItemStack& held) { return held.empty() ? 2 : itemInfo(held.id).attack; }

int creativeItemCount() { return static_cast<int>(creativeFlat().size()); }
ItemId creativeItem(int index) { return creativeFlat()[static_cast<size_t>(index)]; }

const char* creativeTabName(int tab) {
    static const char* NAMES[CREATIVE_TAB_COUNT + 1] = {"Gelaende", "Baumaterial", "Dekoration", "Pflanzen", "Stationen",
                                                         "Werkzeuge", "Rohstoffe", "Essen", "Suche"};
    return tab >= 0 && tab <= CREATIVE_TAB_COUNT ? NAMES[tab] : "";
}

ItemId creativeTabIcon(int tab) {
    static const ItemId ICONS[CREATIVE_TAB_COUNT] = {GRASS, BRICKS, LANTERN, FLOWER_RED, CRAFTING_TABLE, ITEM_IRON_PICKAXE, ITEM_IRON_INGOT, ITEM_BREAD};
    return tab >= 0 && tab < CREATIVE_TAB_COUNT ? ICONS[tab] : ITEM_NONE;
}

const std::vector<ItemId>& creativeTabItems(int tab) {
    static const std::vector<ItemId> none;
    return tab >= 0 && tab < CREATIVE_TAB_COUNT ? tabLists(g_steampunk)[tab] : none;
}

static std::string searchNormalize(const std::string& in) {
    std::string out;
    for (char c : in) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<ItemId> creativeSearch(const std::string& query) {
    const std::string q = searchNormalize(query);
    std::vector<ItemId> out;
    for (ItemId id : creativeFlat())
        if (q.empty() || searchNormalize(itemInfo(id).name).find(q) != std::string::npos) out.push_back(id);
    return out;
}

}  // namespace oc
