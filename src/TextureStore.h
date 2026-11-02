#pragma once
#include <string>
#include <vector>

#include "Atlas.h"
#include "Image.h"
#include "Skins.h"

namespace oc {

struct TileName {
    const char* file;  // Dateiname ohne Endung, im Ordner als "block_<file>.png"
    const char* name;  // Anzeigename
};
const TileName& tileName(int tile);

// Haelt alle Texturen des Spiels: den Blockatlas (16x16-Kacheln) und die Figuren-Skins.
// Beim Start werden die prozeduralen Standardtexturen erzeugt und danach durch gespeicherte PNG-Dateien ueberschrieben.
class TextureStore {
public:
    TextureStore();

    void setDirectory(const std::string& dir) { dir_ = dir; }
    const std::string& directory() const { return dir_; }

    // Atlas fuer die GPU (RGBA, Zeile 0 = unten)
    const std::vector<uint8_t>& atlasPixels() const { return atlas_; }
    bool atlasDirty() const { return atlasDirty_; }
    void clearAtlasDirty() { atlasDirty_ = false; }

    // Kacheln als 16x16-Bild (Zeile 0 = oben)
    Image tile(int t) const;
    void setTile(int t, const Image& img);
    void setTilePixel(int t, int x, int yTop, const uint8_t rgba[4]);
    Image defaultTile(int t) const;
    void resetTile(int t);
    bool tileCustom(int t) const { return customTile_[t]; }

    // Skins
    const Image& skin(SkinId id) const { return skins_[id]; }
    void setSkin(SkinId id, const Image& img);
    void setSkinPixel(SkinId id, int x, int y, const uint8_t rgba[4]);
    bool removeTileFile(int t);
    bool removeSkinFile(SkinId id);
    Image defaultSkin(SkinId id) const;
    void resetSkin(SkinId id);
    bool skinCustom(SkinId id) const { return customSkin_[id]; }
    bool skinDirty(SkinId id) const { return skinDirty_[id]; }
    void clearSkinDirty(SkinId id) { skinDirty_[id] = false; }

    // Dateien
    std::string tilePath(int t) const;
    std::string skinPath(SkinId id) const;
    bool saveTile(int t, std::string* error);
    bool loadTile(int t, std::string* error);
    bool saveSkin(SkinId id, std::string* error);
    bool loadSkin(SkinId id, std::string* error);
    // Prueft eine geladene Datei und passt sie ggf. an (64x64-Minecraft-Skins werden auf die obere Haelfte gekuerzt)
    static bool conformTile(Image& img, std::string* error);
    static bool conformSkin(SkinId id, Image& img, std::string* error);

    // Laedt alle vorhandenen Dateien (Kacheln und Skins). Gibt die Zahl geladener Dateien zurueck.
    int loadAllFromDirectory(std::vector<std::string>* messages);

private:
    std::string dir_ = "textures";
    std::vector<uint8_t> atlas_;
    bool atlasDirty_ = true;
    Image skins_[SKIN_COUNT];
    bool skinDirty_[SKIN_COUNT];
    bool customTile_[TILE_COUNT];
    bool customSkin_[SKIN_COUNT];
};

}  // namespace oc
