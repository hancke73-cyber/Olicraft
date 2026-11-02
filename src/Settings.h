#pragma once
#include <string>

namespace oc {

struct Settings {
    int renderDistance = 10;         // Chunks, 4..24
    float mouseSensitivity = 0.09f;  // Grad je Pixel, 0.02..0.25
    float fov = 70.0f;               // Grad, 50..110
    bool vsync = true;

    static constexpr int MIN_DISTANCE = 4, MAX_DISTANCE = 24;
    static constexpr float MIN_SENS = 0.02f, MAX_SENS = 0.25f, DEFAULT_SENS = 0.09f;
    static constexpr float MIN_FOV = 50.0f, MAX_FOV = 110.0f;

    void clamp();
};

// Textdatei mit Zeilen "schluessel=wert". Unbekannte oder fehlerhafte Zeilen werden ignoriert.
bool loadSettings(const std::string& path, Settings& s);
bool saveSettings(const std::string& path, const Settings& s);

}  // namespace oc
