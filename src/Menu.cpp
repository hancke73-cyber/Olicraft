#include "Menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Hud.h"
#include "Settings.h"
#include "Skins.h"

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
    // Stufe der Oberflaeche, aber so, dass der Inhalt des Bildschirms ins Fenster passt (Einheiten bei Stufe 1)
    const float wantedS = guiScale(h);
    float s = wantedS;
    const float H = static_cast<float>(h);
    switch (screen) {
        // Weltliste beginnt bei 36 % der Hoehe und braucht darunter 165 Einheiten, "Neue Welt" beginnt bei 30 % und braucht 128
        case MenuScreen::Main: s = std::max(1.0f, std::min(wantedS, std::floor((0.64f * H - 8.0f) / 165.0f))); break;
        case MenuScreen::NewWorld: s = std::max(1.0f, std::min(wantedS, std::floor((0.70f * H - 8.0f) / 128.0f))); break;
        case MenuScreen::Pause: s = guiScaleFit(h, 150.0f, wantedS); break;
        case MenuScreen::Graphics: s = guiScaleFit(h, 7 * 24.0f + 20.0f, wantedS); break;
        default: s = guiScaleFit(h, 14 * 24.0f + 20.0f, wantedS); break;  // Einstellungen: 14 Zeilen
    }
    m.scale = s;
    const float bw = 200.0f * s, bh = 20.0f * s, gap = 4.0f * s;
    const float x = std::floor((w - bw) * 0.5f);
    char buf[96];

    if (screen == MenuScreen::Main) {
        m.title = "OliCraft";
        m.titleScale = 6.0f;
        m.subtitle = "Ein Minecraft-Klon in C++";
        m.dim = 0.22f;
        m.titlePos = {0.0f, std::floor(h * 0.07f)};
        const float lw = std::floor(std::min(bw * 1.7f, w - 16.0f * s)), lx = std::floor((w - lw) * 0.5f);
        const float rowH = bh, rowGap = 2.0f * s;
        float y = std::floor(h * 0.36f);
        const int n = static_cast<int>(st.worldLabels.size());
        if (n == 0) {
            Widget e = button(W_NONE, "Noch keine Welt. Lege mit Neue Welt eine an.", lx, y, lw, rowH);
            e.enabled = false;
            m.widgets.push_back(e);
        }
        const int maxScroll = std::max(0, n - WORLD_ROWS);
        const int scroll = std::max(0, std::min(maxScroll, st.worldScroll));
        for (int r = 0; r < WORLD_ROWS && scroll + r < n; ++r) {
            Widget row = button(W_WORLD_BASE + r, st.worldLabels[static_cast<size_t>(scroll + r)], lx, y + r * (rowH + rowGap), lw - (n > WORLD_ROWS ? rowH + rowGap : 0.0f), rowH);
            row.left = true;
            row.selected = scroll + r == st.worldSel;
            m.widgets.push_back(row);
        }
        if (n > WORLD_ROWS) {
            Widget up = button(W_WORLD_UP, "^", lx + lw - rowH, y, rowH, rowH);
            up.enabled = scroll > 0;
            Widget dn = button(W_WORLD_DOWN, "v", lx + lw - rowH, y + (WORLD_ROWS - 1) * (rowH + rowGap), rowH, rowH);
            dn.enabled = scroll < maxScroll;
            m.widgets.push_back(up);
            m.widgets.push_back(dn);
        }
        y += WORLD_ROWS * (rowH + rowGap) + 6.0f * s;
        const float cw = std::floor((lw - gap) * 0.5f);
        Widget play = button(W_MAIN_PLAY, "Spielen", lx, y, cw, bh);
        play.enabled = st.worldSel >= 0 && st.worldSel < n;
        m.widgets.push_back(play);
        m.widgets.push_back(button(W_MAIN_NEW, "Neue Welt ...", lx + cw + gap, y, cw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_SETTINGS, "Einstellungen", lx, y, cw, bh));
        m.widgets.push_back(button(W_QUIT, "Beenden", lx + cw + gap, y, cw, bh));
        return m;
    }
    if (screen == MenuScreen::NewWorld) {
        m.title = "Neue Welt";
        m.titleScale = 3.0f;
        m.dim = 0.5f;
        float y = std::floor(h * 0.30f);
        m.titlePos = {0.0f, std::floor(h * 0.14f)};
        const std::string nameLabel = "Name: " + st.newName + (st.nameFocus ? "_" : "");
        Widget nm = button(W_NEW_NAME, nameLabel, x, y, bw, bh);
        nm.selected = st.nameFocus;
        m.widgets.push_back(nm);
        y += bh + gap;
        const std::string seedLabel = st.seedText.empty() ? std::string(st.nameFocus ? "Seed: zufaellig (anklicken)" : "Seed: zufaellig (Ziffern tippen)") : "Seed: " + st.seedText + (st.nameFocus ? "" : "_");
        Widget sd = button(W_NEW_SEED, seedLabel, x, y, bw, bh);
        sd.selected = !st.nameFocus;
        m.widgets.push_back(sd);
        y += bh + gap;
        m.widgets.push_back(button(W_NEW_MODE, std::string("Spielmodus: ") + (st.newCreative ? "Kreativ" : "Survival"), x, y, bw, bh));
        y += bh + gap;
        y += gap;
        m.widgets.push_back(button(W_NEW_CREATE, "Welt erstellen", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_NEW_BACK, "Zurueck", x, y, bw, bh));
        return m;
    }

    if (screen == MenuScreen::Pause) {
        m.title = "Spiel pausiert";
        const float total = 5 * bh + 4 * gap + 24.0f * s;
        float y = std::floor((h - total) * 0.5f) + 24.0f * s;
        m.titlePos = {0.0f, y - 22.0f * s};
        m.widgets.push_back(button(W_RESUME, "Weiter", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_SETTINGS, "Einstellungen", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_SAVE, "Spiel speichern", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_TO_MAIN, "Hauptmenue", x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_QUIT, "Beenden", x, y, bw, bh));
        return m;
    }

    if (screen == MenuScreen::Graphics) {
        m.title = "Grafik";
        const int rowsG = 7;
        const float total = rowsG * bh + (rowsG - 1) * gap + 24.0f * s;
        float y = std::floor((h - total) * 0.5f) + 24.0f * s;
        m.titlePos = {0.0f, y - 22.0f * s};
        static const char* SH[3] = {"Aus", "Mittel", "Hoch"};
        m.widgets.push_back(button(W_GFX_SHADOWS, std::string("Schatten: ") + SH[st.shadows < 0 || st.shadows > 2 ? 0 : st.shadows], x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_GFX_CLOUDS, std::string("Wolken: ") + (st.clouds ? "An" : "Aus"), x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_GFX_PARTICLES, std::string("Partikel: ") + (st.particles ? "An" : "Aus"), x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_GFX_FILTER, std::string("Texturfilter: ") + (st.texFilter == 0 ? "Pixel (scharf)" : "Weich"), x, y, bw, bh));
        y += bh + gap;
        std::snprintf(buf, sizeof(buf), "Anisotrope Filterung: %dx", st.anisotropy);
        m.widgets.push_back(button(W_GFX_ANISO, buf, x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_GFX_STYLE, std::string("Stil: ") + (st.blockStyle == 1 ? "Kloetzchen" : "Standard"), x, y, bw, bh));
        y += bh + gap;
        m.widgets.push_back(button(W_BACK, "Zurueck", x, y, bw, bh));
        return m;
    }

    m.title = "Einstellungen";
    const int rows = 14;
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
    {
        const struct { int id; const char* name; float v; } vols[3] = {{W_VOL_MASTER, "Gesamtlautstaerke", st.masterVolume},
                                                                    {W_VOL_EFFECTS, "Effekte", st.effectVolume},
                                                                    {W_VOL_MUSIC, "Musik", st.musicVolume}};
        for (const auto& v : vols) {
            std::snprintf(buf, sizeof(buf), "%s: %d%%", v.name, static_cast<int>(std::lround(v.v * 100.0f)));
            m.widgets.push_back(slider(v.id, buf, v.v, x, y, bw, bh));
            y += bh + gap;
        }
    }
    {
        static const char* GS[5] = {"Auto", "1x", "2x", "3x", "4x"};
        m.widgets.push_back(button(W_GUI_SIZE, std::string("GUI-Groesse: ") + GS[st.guiSize < 0 || st.guiSize > 4 ? 0 : st.guiSize], x, y, bw, bh));
        y += bh + gap;
    }
    std::snprintf(buf, sizeof(buf), "Spieler-Skin: %s (%d/%d)", playerSkinName(st.skinSlot), st.skinSlot + 1, PLAYER_SKIN_SLOTS);
    m.widgets.push_back(button(W_SKIN, buf, x, y, bw, bh));
    {
        const float pw = 80.0f * s, ph = 80.0f * s, px = x + bw + 10.0f * s;
        if (px + pw <= w) m.skinPreview = {px, y + bh * 0.5f - ph * 0.5f, pw, ph};
    }
    y += bh + gap;
    m.widgets.push_back(button(W_VSYNC, std::string("VSync: ") + (st.vsync ? "An" : "Aus"), x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_FULLSCREEN, std::string("Vollbild: ") + (st.fullscreen ? "An" : "Aus"), x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_MODE, std::string("Spielmodus: ") + (st.creative ? "Kreativ" : "Survival"), x, y, bw, bh));
    y += bh + gap;
    m.widgets.push_back(button(W_GRAPHICS, "Grafik ...", x, y, bw, bh));
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

float volumeFromFraction(float f) { return std::round(std::max(0.0f, std::min(1.0f, f)) * 20.0f) / 20.0f; }

float fovFromFraction(float f) {
    return std::round(Settings::MIN_FOV + f * (Settings::MAX_FOV - Settings::MIN_FOV));
}

}  // namespace oc
