#pragma once
#include <cstdint>
#include <vector>

namespace oc {

// Figuren-Texturen ("Skins"): aufgeklappte Quadernetze wie bei Minecraft.
enum SkinId { SKIN_PLAYER = 0, SKIN_PIG, SKIN_COW, SKIN_SHEEP, SKIN_ZOMBIE, SKIN_VILLAGER, SKIN_COUNT };

struct SkinInfo {
    const char* name;      // Anzeigename
    const char* fileName;  // Dateiname im Texturordner (PNG)
    int width, height;
};
const SkinInfo& skinInfo(SkinId id);

// RGBA8, Zeile 0 = oben (wie in Bilddateien). Erzeugt die prozedurale Standardtextur.
std::vector<uint8_t> generateSkinPixels(SkinId id);

// Spieler-Skins: 8 Plaetze mit je eigener Standardfarbgebung. Platz 0 entspricht generateSkinPixels(SKIN_PLAYER).
constexpr int PLAYER_SKIN_SLOTS = 8;
const char* playerSkinName(int slot);                       // "Standard", "Rot", ...
std::vector<uint8_t> generatePlayerSkinPixels(int slot);    // 64x32 RGBA

}  // namespace oc
