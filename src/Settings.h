#pragma once
#include <string>

namespace oc {

struct Settings {
    int renderDistance = 10;         // Chunks, 4..24
    float mouseSensitivity = 0.09f;  // Grad je Pixel, 0.02..0.25
    float fov = 70.0f;               // Grad, 50..110
    bool vsync = true;
    float masterVolume = 0.8f;  // 0..1
    float effectVolume = 1.0f;
    float musicVolume = 0.5f;
    int playerSkin = 0;         // Skin-Platz 0..7
    // Grafik
    int shadows = 1;            // 0 aus, 1 mittel (1024), 2 hoch (2048)
    bool clouds = true;
    bool particles = true;
    int texFilter = 0;          // 0 Pixel (scharf), 1 Weich (linear)
    int anisotropy = 8;         // 1, 2, 4, 8, 16
    int guiSize = 0;            // Oberflaeche: 0 automatisch, 1 bis 4 feste Stufe

    static constexpr int MIN_DISTANCE = 4, MAX_DISTANCE = 24;
    static constexpr float MIN_SENS = 0.02f, MAX_SENS = 0.25f, DEFAULT_SENS = 0.09f;
    static constexpr float MIN_FOV = 50.0f, MAX_FOV = 110.0f;

    void clamp();
};

// Textdatei mit Zeilen "schluessel=wert". Unbekannte oder fehlerhafte Zeilen werden ignoriert.
bool loadSettings(const std::string& path, Settings& s);
bool saveSettings(const std::string& path, const Settings& s);

}  // namespace oc
