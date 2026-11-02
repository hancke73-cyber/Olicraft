#include "EditorUi.h"

#include <algorithm>
#include <cmath>

#include "GuiTexture.h"

namespace oc {

float editorScale(int w, int h) {
    return std::max(1.0f, std::min(std::floor(static_cast<float>(h) / 360.0f), std::floor(static_cast<float>(w) / 640.0f)));
}

const std::vector<glm::vec3>& editorPalette() {
    static const std::vector<glm::vec3> p = [] {
        std::vector<glm::vec3> v;
        const int grays[8] = {0, 36, 72, 108, 144, 180, 216, 255};
        for (int g : grays) v.push_back(glm::vec3(static_cast<float>(g)) / 255.0f);
        const int hues[24][3] = {{200, 40, 40},  {230, 100, 40}, {240, 170, 40},  {235, 220, 60},  {160, 210, 60}, {60, 170, 70},
                                 {40, 130, 110}, {50, 190, 200}, {60, 130, 220},  {50, 80, 190},   {110, 70, 190}, {170, 70, 200},
                                 {220, 90, 170}, {250, 170, 190}, {120, 80, 50},  {170, 120, 70},  {210, 170, 110}, {236, 152, 162},
                                 {90, 134, 68},  {56, 60, 150},  {198, 146, 110}, {140, 140, 146}, {96, 96, 100},   {240, 232, 200}};
        for (auto& h : hues) v.push_back(glm::vec3(static_cast<float>(h[0]), static_cast<float>(h[1]), static_cast<float>(h[2])) / 255.0f);
        return v;
    }();
    return p;
}

bool insideRect(const glm::vec4& r, glm::vec2 m) { return m.x >= r.x && m.x < r.x + r.z && m.y >= r.y && m.y < r.y + r.w; }

int maxListScroll(const EditorLayout& l) { return std::max(0, l.listTotalRows - l.listRows); }

namespace {

Widget makeButton(int id, const std::string& text, float x, float y, float w, float h, bool selected = false) {
    Widget wd;
    wd.id = id;
    wd.text = text;
    wd.pos = {x, y};
    wd.size = {w, h};
    wd.selected = selected;
    return wd;
}

struct Row {
    bool header;
    int entry;
    std::string text;
};

std::vector<Row> listRows(const TextureEditor& ed) {
    std::vector<Row> rows;
    const char* lastGroup = nullptr;
    for (size_t i = 0; i < ed.entries().size(); ++i) {
        const EditorEntry& e = ed.entries()[i];
        if (!lastGroup || std::string(lastGroup) != e.group) {
            rows.push_back({true, -1, e.group});
            lastGroup = e.group;
        }
        rows.push_back({false, static_cast<int>(i), e.name});
    }
    return rows;
}

std::string clip(const std::string& s, size_t maxChars) { return s.size() <= maxChars ? s : s.substr(0, maxChars); }

}  // namespace

EditorLayout buildEditorLayout(int w, int h, const TextureEditor& ed, const EditorUiState& ui) {
    EditorLayout L;
    const float s = editorScale(w, h);
    L.s = s;
    const float lw = static_cast<float>(w) / s, lh = static_cast<float>(h) / s;  // logische Groesse
    auto px = [&](float v) { return v * s; };

    const float margin = 6.0f, listW = 118.0f, rightW = 196.0f, topY = 18.0f;
    const float bottomRowsY = lh - 70.0f;  // oberste Zeile der Knopfleisten

    L.titlePos = {px(margin), px(5.0f)};
    L.statusPos = {px(margin + listW + 6.0f), px(lh - 15.0f)};

    // --- Liste ---------------------------------------------------------------------------------------------------
    const float listH = lh - topY - 26.0f;
    L.list = {px(margin), px(topY), px(listW), px(listH)};
    const float rowH = 11.0f;
    L.listRows = std::max(1, static_cast<int>(listH / rowH));
    const std::vector<Row> rows = listRows(ed);
    L.listTotalRows = static_cast<int>(rows.size());
    const int scroll = std::max(0, std::min(ui.listScroll, std::max(0, L.listTotalRows - L.listRows)));
    for (int i = 0; i < L.listRows && scroll + i < L.listTotalRows; ++i) {
        const Row& r = rows[static_cast<size_t>(scroll + i)];
        Widget wd = makeButton(r.header ? W_NONE : W_ED_LIST_BASE + r.entry, clip(r.text, 18), px(margin), px(topY + i * rowH),
                               px(listW), px(rowH - 1.0f), !r.header && r.entry == ed.current());
        wd.enabled = !r.header;
        wd.left = true;
        L.widgets.push_back(wd);
    }
    L.widgets.push_back(makeButton(W_ED_LIST_UP, "^", px(margin), px(lh - 24.0f), px(listW / 2 - 1), px(11.0f)));
    L.widgets.push_back(makeButton(W_ED_LIST_DOWN, "v", px(margin + listW / 2 + 1), px(lh - 24.0f), px(listW / 2 - 1), px(11.0f)));

    // --- Leinwand ------------------------------------------------------------------------------------------------
    const float areaX = margin + listW + 6.0f, areaW = lw - areaX - rightW - 12.0f, areaY = topY, areaH = bottomRowsY - topY - 4.0f;
    const Image& view = ed.view();
    float zoom = std::max(1.0f, std::floor(std::min(areaW / static_cast<float>(view.w), areaH / static_cast<float>(view.h))));
    L.zoom = zoom * s;
    const float cw = view.w * zoom, ch = view.h * zoom;
    L.canvas = {px(areaX + (areaW - cw) * 0.5f), px(areaY + (areaH - ch) * 0.5f), px(cw), px(ch)};

    // --- Werkzeugleisten -----------------------------------------------------------------------------------------
    {
        float x = areaX, y = bottomRowsY;
        const float bh = 12.0f;
        auto add = [&](int id, const std::string& text, float bw, bool sel = false) {
            L.widgets.push_back(makeButton(id, text, px(x), px(y), px(bw), px(bh), sel));
            x += bw + 2.0f;
        };
        add(W_ED_PENCIL, "Stift", 36, ed.tool == Tool::Pencil);
        add(W_ED_ERASER, "Radierer", 54, ed.tool == Tool::Eraser);
        add(W_ED_FILL, "Fuellen", 46, ed.tool == Tool::Fill);
        add(W_ED_PICKER, "Pipette", 46, ed.tool == Tool::Picker);
        add(W_ED_BRUSH, "Pinsel: " + std::to_string(ed.brush), 58);
        add(W_ED_UNDO, "Rueckg.", 46);
        add(W_ED_REDO, "Wiederh.", 54);
        x = areaX;
        y += bh + 2.0f;
        add(W_ED_GRID, std::string("Raster: ") + (ui.grid ? "An" : "Aus"), 66, ui.grid);
        add(W_ED_NET, std::string("Netz: ") + (ui.net ? "An" : "Aus"), 54, ui.net);
        add(W_ED_ROTATE, std::string("Drehen: ") + (ui.rotate ? "An" : "Aus"), 66, ui.rotate);
        add(W_ED_ROT_LEFT, "<", 14);
        add(W_ED_ROT_RIGHT, ">", 14);
        x = areaX;
        y += bh + 2.0f;
        add(W_ED_SAVE, "Speichern", 58);
        add(W_ED_EXPORT, "Exportieren", 70);
        add(W_ED_IMPORT, "Importieren", 70);
        add(W_ED_RESET, "Zuruecksetzen", 82);
        add(W_ED_BACK, "Zurueck", 46);
    }

    // --- Vorschau, Farben ----------------------------------------------------------------------------------------
    const float rx = lw - margin - rightW;
    L.preview = {px(rx), px(topY), px(rightW), px(124.0f)};
    {
        // Farbfelder: 8 x 4
        const auto& pal = editorPalette();
        const float sw = 11.0f, gap = 1.0f, py = topY + 124.0f + 6.0f;
        for (size_t i = 0; i < pal.size(); ++i) {
            Widget wd = makeButton(W_ED_SWATCH_BASE + static_cast<int>(i), "", px(rx + (i % 8) * (sw + gap)), px(py + (i / 8) * (sw + gap)),
                                   px(sw), px(sw));
            wd.swatch = true;
            wd.color = glm::vec4(pal[i], 1.0f);
            L.widgets.push_back(wd);
        }
        // aktuelle Farbe daneben
        Widget cur = makeButton(W_NONE, "", px(rx + 8 * (sw + gap) + 4.0f), px(py), px(rightW - 8 * (sw + gap) - 4.0f), px(4 * (sw + gap) - gap));
        cur.swatch = true;
        cur.enabled = false;
        cur.color = glm::vec4(ed.color[0] / 255.0f, ed.color[1] / 255.0f, ed.color[2] / 255.0f, ed.color[3] / 255.0f);
        L.widgets.push_back(cur);
        // RGBA-Schieber
        const char* names[4] = {"R", "G", "B", "A"};
        const int ids[4] = {W_ED_R, W_ED_G, W_ED_B, W_ED_A};
        float sy = py + 4 * (sw + gap) + 6.0f;
        for (int i = 0; i < 4; ++i) {
            Widget wd = makeButton(ids[i], std::string(names[i]) + ": " + std::to_string(ed.color[i]), px(rx), px(sy), px(rightW), px(11.0f));
            wd.type = Widget::Slider;
            wd.fraction = ed.color[i] / 255.0f;
            L.widgets.push_back(wd);
            sy += 12.0f;
        }
    }
    return L;
}

bool canvasPixel(const EditorLayout& l, const TextureEditor& ed, glm::vec2 m, int& x, int& y) {
    if (!insideRect(l.canvas, m)) return false;
    x = static_cast<int>(std::floor((m.x - l.canvas.x) / l.zoom));
    y = static_cast<int>(std::floor((m.y - l.canvas.y) / l.zoom));
    return x >= 0 && y >= 0 && x < ed.view().w && y < ed.view().h;
}

}  // namespace oc
