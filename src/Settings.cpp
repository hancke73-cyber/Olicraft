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
    return static_cast<bool>(f);
}

}  // namespace oc
