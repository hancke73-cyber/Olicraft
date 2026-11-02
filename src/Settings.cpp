#include "Settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace oc {

void Settings::clamp() {
    renderDistance = std::max(MIN_DISTANCE, std::min(MAX_DISTANCE, renderDistance));
    mouseSensitivity = std::max(MIN_SENS, std::min(MAX_SENS, mouseSensitivity));
    fov = std::max(MIN_FOV, std::min(MAX_FOV, fov));
    masterVolume = std::max(0.0f, std::min(1.0f, masterVolume));
    effectVolume = std::max(0.0f, std::min(1.0f, effectVolume));
    musicVolume = std::max(0.0f, std::min(1.0f, musicVolume));
    playerSkin = std::max(0, std::min(7, playerSkin));
    shadows = std::max(0, std::min(2, shadows));
    texFilter = std::max(0, std::min(1, texFilter));
    guiSize = std::max(0, std::min(4, guiSize));
    blockStyle = std::max(0, std::min(1, blockStyle));
    // Anisotropie auf eine erlaubte Stufe runden
    int a = 1;
    while (a * 2 <= anisotropy && a < 16) a *= 2;
    anisotropy = a;
}

bool loadSettings(const std::string& path, Settings& s) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#') continue;
        const std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        char* end = nullptr;
        if (key == "render_distance") {
            long v = std::strtol(val.c_str(), &end, 10);
            if (end != val.c_str()) s.renderDistance = static_cast<int>(v);
        } else if (key == "mouse_sensitivity") {
            double v = std::strtod(val.c_str(), &end);
            if (end != val.c_str()) s.mouseSensitivity = static_cast<float>(v);
        } else if (key == "fov") {
            double v = std::strtod(val.c_str(), &end);
            if (end != val.c_str()) s.fov = static_cast<float>(v);
        } else if (key == "master_volume" || key == "effect_volume" || key == "music_volume") {
            double v = std::strtod(val.c_str(), &end);
            if (end != val.c_str()) {
                float& t = key == "master_volume" ? s.masterVolume : key == "effect_volume" ? s.effectVolume : s.musicVolume;
                t = static_cast<float>(v);
            }
        } else if (key == "player_skin") {
            long v = std::strtol(val.c_str(), &end, 10);
            if (end != val.c_str()) s.playerSkin = static_cast<int>(v) - 1;  // in der Datei 1..8
        } else if (key == "shadows" || key == "tex_filter" || key == "anisotropy") {
            long v = std::strtol(val.c_str(), &end, 10);
            if (end != val.c_str()) (key == "shadows" ? s.shadows : key == "tex_filter" ? s.texFilter : s.anisotropy) = static_cast<int>(v);
        } else if (key == "block_style") {
            long v = std::strtol(val.c_str(), &end, 10);
            if (end != val.c_str()) s.blockStyle = static_cast<int>(v);
        } else if (key == "gui_size") {
            long v = std::strtol(val.c_str(), &end, 10);
            if (end != val.c_str()) s.guiSize = static_cast<int>(v);
        } else if (key == "clouds" || key == "particles") {
            (key == "clouds" ? s.clouds : s.particles) = val == "1" || val == "true" || val == "an";
        } else if (key == "vsync") {
            s.vsync = val == "1" || val == "true" || val == "an";
        }
    }
    s.clamp();
    return true;
}

bool saveSettings(const std::string& path, const Settings& s) {
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << "# OliCraft Einstellungen\n";
    f << "render_distance=" << s.renderDistance << "\n";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.4f", s.mouseSensitivity);
    f << "mouse_sensitivity=" << buf << "\n";
    std::snprintf(buf, sizeof(buf), "%.1f", s.fov);
    f << "fov=" << buf << "\n";
    f << "vsync=" << (s.vsync ? 1 : 0) << "\n";
    f << "shadows=" << s.shadows << "\n";
    f << "clouds=" << (s.clouds ? 1 : 0) << "\n";
    f << "particles=" << (s.particles ? 1 : 0) << "\n";
    f << "tex_filter=" << s.texFilter << "\n";
    f << "anisotropy=" << s.anisotropy << "\n";
    f << "gui_size=" << s.guiSize << "\n";
    f << "block_style=" << s.blockStyle << "\n";
    f << "player_skin=" << s.playerSkin + 1 << "\n";
    std::snprintf(buf, sizeof(buf), "%.2f", s.masterVolume);
    f << "master_volume=" << buf << "\n";
    std::snprintf(buf, sizeof(buf), "%.2f", s.effectVolume);
    f << "effect_volume=" << buf << "\n";
    std::snprintf(buf, sizeof(buf), "%.2f", s.musicVolume);
    f << "music_volume=" << buf << "\n";
    return static_cast<bool>(f);
}

}  // namespace oc
