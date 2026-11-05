#pragma once
#include <cstdint>
#include <vector>

namespace oc {

// Figuren-Texturen ("Skins"): aufgeklappte Quadernetze wie bei Minecraft.
enum SkinId { SKIN_PLAYER = 0, SKIN_PIG, SKIN_COW, SKIN_SHEEP, SKIN_ZOMBIE, SKIN_VILLAGER, SKIN_VILLAGER_LIBRARIAN, SKIN_VILLAGER_SMITH, SKIN_VILLAGER_MASON, SKIN_VILLAGER_SHEPHERD,
               SKIN_ARMOR_LEATHER, SKIN_ARMOR_IRON, SKIN_ARMOR_GOLD, SKIN_ARMOR_DIAMOND, SKIN_GOLEM, SKIN_COUNT };

// Dorfbewohner: je Beruf (Bauer, Bibliothekar, Schmied, Steinmetz, Schaefer) eine eigene Textur
inline bool isVillagerSkin(SkinId id) { return id >= SKIN_VILLAGER && id <= SKIN_VILLAGER_SHEPHERD; }
// Ruestungstexturen (gleiches Netz-Layout wie die Spielerfigur, aber nur die Ruestungsteile): je Material eine
inline bool isArmorSkin(SkinId id) { return id >= SKIN_ARMOR_LEATHER && id <= SKIN_ARMOR_DIAMOND; }
inline SkinId armorSkinFor(int material) { return static_cast<SkinId>(SKIN_ARMOR_LEATHER + (material < 0 || material > 3 ? 0 : material)); }
inline SkinId villagerSkinFor(int profession) { return static_cast<SkinId>(SKIN_VILLAGER + (profession < 0 || profession > 4 ? 0 : profession)); }

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
