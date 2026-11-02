#pragma once
#include <cstdint>
#include <vector>

namespace oc {

// Figuren-Texturen ("Skins"): aufgeklappte Quadernetze wie bei Minecraft.
enum SkinId { SKIN_PLAYER = 0, SKIN_PIG, SKIN_COW, SKIN_SHEEP, SKIN_ZOMBIE, SKIN_COUNT };

struct SkinInfo {
    const char* name;      // Anzeigename
    const char* fileName;  // Dateiname im Texturordner (PNG)
    int width, height;
};
const SkinInfo& skinInfo(SkinId id);

// RGBA8, Zeile 0 = oben (wie in Bilddateien). Erzeugt die prozedurale Standardtextur.
std::vector<uint8_t> generateSkinPixels(SkinId id);

}  // namespace oc
