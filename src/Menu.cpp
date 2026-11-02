#include "Menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Hud.h"
#include "Settings.h"

namespace oc {

namespace {
Widget button(int id, const std::string& text, float x, float y, float w, float h) {
    Widget wd;
    wd.type = Widget::Button;
    wd.id = id;
    wd.text = text;
    wd.pos = {x, y};
    wd.size = {w, h};
    return wd;
}
Widget slider(int id, const std::string& text, float fraction, float x, float y, float w, float h) {
    Widget wd = button(id, text, x, y, w, h);
    wd.type = Widget::Slider;
    wd.fraction = std::max(0.0f, std::min(1.0f, fraction));
    return wd;
}
}  // namespace

MenuLayout buildMenu(MenuScreen screen, const MenuState& st, int w, int h) {
    MenuLayout m;
    if (screen == MenuScreen::None) return m;
    const float s = guiScale(h);
    m.scale = s;
    const float bw = 200.0f * s, bh = 20.0f * s, gap = 4.0f * s;
    const float x = std::floor((w - bw) * 0.5f);
    char buf[96];

    if (screen == MenuScreen::Pause) {
        m.title = "Spiel pausiert";
        const float total = 4 * bh + 3 * gap + 24.0f * s;
        float y = std::floor((h - total) * 0.5f) + 24.0f * s;
        m.titlePos = {0.0f, y - 22.0f * s};
        m.widgets.push_back(button(W_RESUME, "Weiter", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_SETTINGS, "Einstellungen", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_SAVE, "Spiel speichern", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_QUIT, "Beenden", x, y, bw, bh));
        return m;
    }

    m.title = "Einstellungen";
    const int rows = 8;
    const float total = rows * bh + (rows - 1) * gap + 24.0f * s;
    float y = std::floor((h - total) * 0.5f) + 24.0f * s;
    m.titlePos = {0.0f, y - 22.0f * s};
    std::snprintf(buf, sizeof(buf), "Sichtweite: %d Chunks", st.renderDistance);
    m.widgets.push_back(slider(W_DISTANCE, buf,
                               static_cast<float>(st.renderDistance - Settings::MIN_DISTANCE) /
                                   (Settings::MAX_DISTANCE - Settings::MIN_DISTANCE),
                               x, y, bw, bh));
    y += bh + gap;
    std::snprintf(buf, sizeof(buf), "Mausempfindlichkeit: %d%%",
                  static_cast<int>(std::lround(st.mouseSensitivity / Settings::DEFAULT_SENS * 100.0f)));
    m.widgets.push_back(slider(W_SENSITIVITY, buf,
                               (st.mouseSensitivity - Settings::MIN_SENS) / (Settings::MAX_SENS - Settings::MIN_SENS), x, y,
                               bw, bh));
    y += bh + gap;
    std::snprintf(buf, sizeof(buf), "Sichtfeld: %d Grad", static_cast<int>(std::lround(st.fov)));
    m.widgets.push_back(
        slider(W_FOV, buf, (st.fov - Settings::MIN_FOV) / (Settings::MAX_FOV - Settings::MIN_FOV), x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_VSYNC, std::string("VSync: ") + (st.vsync ? "An" : "Aus"), x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_FULLSCREEN, std::string("Vollbild: ") + (st.fullscreen ? "An" : "Aus"), x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_MODE, std::string("Spielmodus: ") + (st.creative ? "Kreativ" : "Survival"), x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_EDITOR, "Textur-Editor", x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_BACK, "Zurueck", x, y, bw, bh));
    return m;
}

int hitWidget(const MenuLayout& m, glm::vec2 p) {
    for (size_t i = 0; i < m.widgets.size(); ++i) {
        const Widget& w = m.widgets[i];
        if (p.x >= w.pos.x && p.x < w.pos.x + w.size.x && p.y >= w.pos.y && p.y < w.pos.y + w.size.y)
            return static_cast<int>(i);
    }
    return -1;
}

float sliderFraction(const Widget& w, float mouseX) {
    return std::max(0.0f, std::min(1.0f, (mouseX - w.pos.x) / w.size.x));
}

int distanceFromFraction(float f) {
    return Settings::MIN_DISTANCE +
           static_cast<int>(std::lround(f * (Settings::MAX_DISTANCE - Settings::MIN_DISTANCE)));
}

float sensitivityFromFraction(float f) {
    float v = Settings::MIN_SENS + f * (Settings::MAX_SENS - Settings::MIN_SENS);
    return std::round(v * 100.0f) / 100.0f < Settings::MIN_SENS ? Settings::MIN_SENS : std::round(v * 100.0f) / 100.0f;
}

float fovFromFraction(float f) {
    return std::round(Settings::MIN_FOV + f * (Settings::MAX_FOV - Settings::MIN_FOV));
}

}  // namespace oc
