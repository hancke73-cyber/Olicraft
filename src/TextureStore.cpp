#include "TextureStore.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace oc {

namespace {
const TileName TILE_NAMES[TILE_COUNT] = {
    {"grass_top", "Gras oben"},
    {"grass_side", "Gras Seite"},
    {"dirt", "Erde"},
    {"stone", "Stein"},
    {"sand", "Sand"},
    {"water", "Wasser"},
    {"log_side", "Holz Seite"},
    {"log_top", "Holz oben"},
    {"leaves", "Laub"},
    {"planks", "Bretter"},
    {"cobble", "Bruchstein"},
    {"bedrock", "Grundgestein"},
    {"glass", "Glas"},
    {"snow", "Schnee"},
    {"meat", "Rohes Fleisch"},
    {"lava", "Lava"},
    {"obsidian", "Obsidian"},
    {"craft_top", "Werkbank oben"},
    {"craft_side", "Werkbank Seite"},
    {"stick", "Stock"},
    {"pickaxe_wood", "Holzspitzhacke"},
    {"axe_wood", "Holzaxt"},
    {"shovel_wood", "Holzschaufel"},
    {"sword_wood", "Holzschwert"},
    {"pickaxe_stone", "Steinspitzhacke"},
    {"axe_stone", "Steinaxt"},
    {"shovel_stone", "Steinschaufel"},
    {"sword_stone", "Steinschwert"},
    {"coal_ore", "Kohleerz"},
    {"iron_ore", "Eisenerz"},
    {"diamond_ore", "Diamanterz"},
    {"furnace", "Ofen Seite"},
    {"furnace_lit", "Ofen Seite (an)"},
    {"furnace_top", "Ofen oben"},
    {"coal", "Kohle"},
    {"iron_ingot", "Eisenbarren"},
    {"diamond", "Diamant"},
    {"cooked_meat", "Gebratenes Fleisch"},
    {"pickaxe_iron", "Eisenspitzhacke"},
    {"axe_iron", "Eisenaxt"},
    {"shovel_iron", "Eisenschaufel"},
    {"sword_iron", "Eisenschwert"},
    {"pickaxe_diamond", "Diamantspitzhacke"},
    {"axe_diamond", "Diamantaxt"},
    {"shovel_diamond", "Diamantschaufel"},
    {"sword_diamond", "Diamantschwert"},
    {"gold_ore", "Golderz"},
    {"gold_ingot", "Goldbarren"},
    {"pickaxe_gold", "Goldspitzhacke"},
    {"axe_gold", "Goldaxt"},
    {"shovel_gold", "Goldschaufel"},
    {"sword_gold", "Goldschwert"},
    {"torch", "Fackel"},
    {"bricks", "Ziegel"},
    {"stone_bricks", "Steinziegel"},
    {"sandstone_top", "Sandstein oben"},
    {"sandstone_side", "Sandstein Seite"},
    {"gravel", "Kies"},
    {"clay", "Ton"},
    {"wool_white", "Weisse Wolle"},
    {"wool_red", "Rote Wolle"},
    {"wool_blue", "Blaue Wolle"},
    {"wool_green", "Gruene Wolle"},
    {"wool_yellow", "Gelbe Wolle"},
    {"wool_black", "Schwarze Wolle"},
    {"bookshelf", "Buecherregal"},
    {"iron_block", "Eisenblock"},
    {"gold_block", "Goldblock"},
    {"diamond_block", "Diamantblock"},
    {"coal_block", "Kohleblock"},
    {"lamp", "Lampe"},
    {"pumpkin_top", "Kuerbis oben"},
    {"pumpkin_side", "Kuerbis Seite"},
    {"cactus_top", "Kaktus oben"},
    {"cactus_side", "Kaktus Seite"},
    {"birch_log_side", "Birkenholz Seite"},
    {"birch_log_top", "Birkenholz oben"},
    {"birch_leaves", "Birkenlaub"},
    {"spruce_log_side", "Fichtenholz Seite"},
    {"spruce_log_top", "Fichtenholz oben"},
    {"spruce_leaves", "Fichtenlaub"},
    {"chest_top", "Truhe oben"},
    {"chest_side", "Truhe Seite"},
    {"bed_top", "Bett oben"},
    {"bed_side", "Bett Seite"},
    {"tall_grass", "Hohes Gras"},
    {"fern", "Farn"},
    {"flower_red", "Mohn"},
    {"flower_yellow", "Loewenzahn"},
    {"flower_blue", "Kornblume"},
    {"flower_white", "Gaensebluemchen"},
    {"mushroom_red", "Roter Pilz"},
    {"mushroom_brown", "Brauner Pilz"},
    {"dead_bush", "Toter Busch"},
    {"sugar_cane", "Zuckerrohr"},
    {"berry_bush", "Beerenstrauch"},
    {"brick", "Ziegel (Gegenstand)"},
    {"clay_ball", "Tonklumpen"},
    {"paper", "Papier"},
    {"book", "Buch"},
    {"dye_red", "Rote Farbe"},
    {"dye_yellow", "Gelbe Farbe"},
    {"dye_blue", "Blaue Farbe"},
    {"dye_green", "Gruene Farbe"},
    {"dye_black", "Schwarze Farbe"},
    {"berries", "Beeren"},
    {"iron_bars", "Eisengitter"},
    {"lantern", "Laterne"},
    {"door_wood_lower", "Holztuer unten"},
    {"door_wood_upper", "Holztuer oben"},
    {"door_iron_lower", "Eisentuer unten"},
    {"door_iron_upper", "Eisentuer oben"},
    {"gate_wood", "Holztor"},
    {"gate_iron", "Eisentor"},
    {"farmland", "Acker"},
    {"wheat_0", "Weizen 1"},
    {"wheat_1", "Weizen 2"},
    {"wheat_2", "Weizen 3"},
    {"wheat_3", "Weizen reif"},
    {"carrot_0", "Karotten 1"},
    {"carrot_1", "Karotten 2"},
    {"carrot_2", "Karotten 3"},
    {"carrot_3", "Karotten reif"},
    {"seeds", "Samen"},
    {"wheat", "Weizen (Gegenstand)"},
    {"bread", "Brot"},
    {"carrot", "Karotte"},
    {"hoe_wood", "Holzhacke"},
    {"hoe_stone", "Steinhacke"},
    {"hoe_iron", "Eisenhacke"},
    {"hoe_gold", "Goldhacke"},
    {"hoe_diamond", "Diamanthacke"},
    {"door_wood_item", "Holztuer (Gegenstand)"},
    {"door_iron_item", "Eisentuer (Gegenstand)"},
};
static_assert(sizeof(TILE_NAMES) / sizeof(TILE_NAMES[0]) == TILE_COUNT, "Kachelnamen passen nicht zur Kachelliste");

size_t atlasIndex(int tile, int x, int yBottom) {
    const int tx = (tile % ATLAS_TILES) * TILE_PX + x;
    const int ty = (tile / ATLAS_TILES) * TILE_PX + yBottom;
    return (static_cast<size_t>(ty) * ATLAS_PX + tx) * 4;
}
}  // namespace

const TileName& tileName(int tile) { return TILE_NAMES[tile]; }

TextureStore::TextureStore() {
    atlas_ = generateAtlasPixels();
    for (int i = 0; i < SKIN_COUNT; ++i) {
        const SkinInfo& si = skinInfo(static_cast<SkinId>(i));
        skins_[i] = Image(si.width, si.height);
        skins_[i].rgba = generateSkinPixels(static_cast<SkinId>(i));
        skinDirty_[i] = true;
        customSkin_[i] = false;
    }
    std::fill(std::begin(customTile_), std::end(customTile_), false);
    for (int s = 0; s < PLAYER_SKIN_SLOTS; ++s) {
        slotImg_[s] = Image(64, 32);
        slotImg_[s].rgba = generatePlayerSkinPixels(s);
        slotCustom_[s] = false;
    }
    skins_[SKIN_PLAYER] = slotImg_[0];
}

void TextureStore::selectPlayerSkin(int slot) {
    slot = std::max(0, std::min(PLAYER_SKIN_SLOTS - 1, slot));
    if (slot == playerSlot_) return;
    slotImg_[playerSlot_] = skins_[SKIN_PLAYER];
    slotCustom_[playerSlot_] = customSkin_[SKIN_PLAYER];
    playerSlot_ = slot;
    skins_[SKIN_PLAYER] = slotImg_[slot];
    customSkin_[SKIN_PLAYER] = slotCustom_[slot];
    skinDirty_[SKIN_PLAYER] = true;
}

Image TextureStore::playerSkinImage(int slot) const {
    slot = std::max(0, std::min(PLAYER_SKIN_SLOTS - 1, slot));
    return slot == playerSlot_ ? skins_[SKIN_PLAYER] : slotImg_[slot];
}

bool TextureStore::playerSkinCustom(int slot) const {
    slot = std::max(0, std::min(PLAYER_SKIN_SLOTS - 1, slot));
    return slot == playerSlot_ ? customSkin_[SKIN_PLAYER] : slotCustom_[slot];
}

std::string TextureStore::playerSkinPath(int slot) const {
    return dir_ + "/" + (slot <= 0 ? std::string("skin_player.png") : "skin_player_" + std::to_string(slot + 1) + ".png");
}

Image TextureStore::tile(int t) const {
    Image img(TILE_PX, TILE_PX);
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) std::memcpy(img.at(x, y), &atlas_[atlasIndex(t, x, TILE_PX - 1 - y)], 4);
    return img;
}

void TextureStore::setTile(int t, const Image& img) {
    if (img.w != TILE_PX || img.h != TILE_PX || !img.valid()) return;
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) std::memcpy(&atlas_[atlasIndex(t, x, TILE_PX - 1 - y)], img.at(x, y), 4);
    atlasDirty_ = true;
}

void TextureStore::setTilePixel(int t, int x, int yTop, const uint8_t rgba[4]) {
    if (x < 0 || yTop < 0 || x >= TILE_PX || yTop >= TILE_PX) return;
    std::memcpy(&atlas_[atlasIndex(t, x, TILE_PX - 1 - yTop)], rgba, 4);
    atlasDirty_ = true;
}

Image TextureStore::defaultTile(int t) const {
    static const std::vector<uint8_t> defaults = generateAtlasPixels();
    Image img(TILE_PX, TILE_PX);
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) std::memcpy(img.at(x, y), &defaults[atlasIndex(t, x, TILE_PX - 1 - y)], 4);
    return img;
}

void TextureStore::resetTile(int t) {
    setTile(t, defaultTile(t));
    customTile_[t] = false;
}

void TextureStore::setSkin(SkinId id, const Image& img) {
    const SkinInfo& si = skinInfo(id);
    if (img.w != si.width || img.h != si.height || !img.valid()) return;
    skins_[id] = img;
    skinDirty_[id] = true;
}

void TextureStore::setSkinPixel(SkinId id, int x, int y, const uint8_t rgba[4]) {
    Image& img = skins_[id];
    if (x < 0 || y < 0 || x >= img.w || y >= img.h) return;
    std::memcpy(img.at(x, y), rgba, 4);
    skinDirty_[id] = true;
}

bool TextureStore::removeTileFile(int t) {
    std::error_code ec;
    return std::filesystem::remove(tilePath(t), ec);
}

bool TextureStore::removeSkinFile(SkinId id) {
    std::error_code ec;
    return std::filesystem::remove(skinPath(id), ec);
}

Image TextureStore::defaultSkin(SkinId id) const {
    const SkinInfo& si = skinInfo(id);
    Image img(si.width, si.height);
    img.rgba = id == SKIN_PLAYER ? generatePlayerSkinPixels(playerSlot_) : generateSkinPixels(id);
    return img;
}

void TextureStore::resetSkin(SkinId id) {
    setSkin(id, defaultSkin(id));
    customSkin_[id] = false;
}

std::string TextureStore::tilePath(int t) const { return dir_ + "/block_" + TILE_NAMES[t].file + ".png"; }
std::string TextureStore::skinPath(SkinId id) const {
    return id == SKIN_PLAYER ? playerSkinPath(playerSlot_) : dir_ + "/" + skinInfo(id).fileName;
}

bool TextureStore::conformTile(Image& img, std::string* error) {
    if (img.w == TILE_PX && img.h == TILE_PX) return true;
    if (error) *error = "Falsche Groesse " + std::to_string(img.w) + "x" + std::to_string(img.h) + ", erwartet 16x16";
    return false;
}

bool TextureStore::conformSkin(SkinId id, Image& img, std::string* error) {
    const SkinInfo& si = skinInfo(id);
    if (img.w == si.width && img.h == si.height) return true;
    // Minecraft-Skins im neuen Format (64x64) enthalten oben das alte 64x32-Layout
    if ((id == SKIN_PLAYER || id == SKIN_ZOMBIE || id == SKIN_VILLAGER) && img.w == 64 && img.h == 64) {
        Image cut(64, 32);
        std::memcpy(cut.rgba.data(), img.rgba.data(), cut.rgba.size());
        img = cut;
        return true;
    }
    if (error)
        *error = "Falsche Groesse " + std::to_string(img.w) + "x" + std::to_string(img.h) + ", erwartet " +
                 std::to_string(si.width) + "x" + std::to_string(si.height);
    return false;
}

bool TextureStore::saveTile(int t, std::string* error) {
    if (!savePng(tilePath(t), tile(t), error)) return false;
    customTile_[t] = true;
    return true;
}

bool TextureStore::loadTile(int t, std::string* error) {
    Image img;
    if (!loadImage(tilePath(t), img, error)) return false;
    if (!conformTile(img, error)) return false;
    setTile(t, img);
    customTile_[t] = true;
    return true;
}

bool TextureStore::saveSkin(SkinId id, std::string* error) {
    if (!savePng(skinPath(id), skins_[id], error)) return false;
    customSkin_[id] = true;
    return true;
}

bool TextureStore::loadSkin(SkinId id, std::string* error) {
    Image img;
    if (!loadImage(skinPath(id), img, error)) return false;
    if (!conformSkin(id, img, error)) return false;
    setSkin(id, img);
    customSkin_[id] = true;
    return true;
}

int TextureStore::loadAllFromDirectory(std::vector<std::string>* messages) {
    namespace fs = std::filesystem;
    int loaded = 0;
    std::error_code ec;
    if (!fs::is_directory(dir_, ec)) return 0;
    for (int t = 0; t < TILE_COUNT; ++t) {
        if (!fs::exists(tilePath(t), ec)) continue;
        std::string err;
        if (loadTile(t, &err)) ++loaded;
        else if (messages) messages->push_back(tilePath(t) + ": " + err);
    }
    const int keepSlot = playerSlot_;
    for (int i = 0; i < SKIN_COUNT; ++i) {
        const SkinId id = static_cast<SkinId>(i);
        // Spieler: alle Plaetze nacheinander laden
        for (int s = 0; s < (id == SKIN_PLAYER ? PLAYER_SKIN_SLOTS : 1); ++s) {
            if (id == SKIN_PLAYER) selectPlayerSkin(s);
            if (!fs::exists(skinPath(id), ec)) continue;
            std::string err;
            if (loadSkin(id, &err)) ++loaded;
            else if (messages) messages->push_back(skinPath(id) + ": " + err);
        }
    }
    selectPlayerSkin(keepSlot);
    return loaded;
}

}  // namespace oc
