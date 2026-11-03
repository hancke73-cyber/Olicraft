#include "Hud.h"

#include <algorithm>
#include <cmath>

#include "Atlas.h"

namespace oc {

// ---------------------------------------------------------------------------------------------------------------
// HudBatch
// ---------------------------------------------------------------------------------------------------------------

void HudBatch::quad(const glm::vec2 p[4], const glm::vec2 uv[4], glm::vec4 c, int tex) {
    if (cmds.empty() || cmds.back().tex != tex) cmds.push_back({tex, verts.size(), 0});
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i : order) verts.push_back({p[i].x, p[i].y, uv[i].x, uv[i].y, c.r, c.g, c.b, c.a});
    cmds.back().count += 6;
}

void HudBatch::rect(float x0, float y0, float x1, float y1, glm::vec4 c) {
    const glm::vec2 p[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    const glm::vec2 uv[4] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
    quad(p, uv, c, HUD_TEX_GUI);
}

void HudBatch::sprite(const GuiRect& r, float x, float y, float w, float h, glm::vec4 c) {
    const float u0 = static_cast<float>(r.x) / GUI_TEX_W, v0 = static_cast<float>(r.y) / GUI_TEX_H;
    const float u1 = static_cast<float>(r.x + r.w) / GUI_TEX_W, v1 = static_cast<float>(r.y + r.h) / GUI_TEX_H;
    const glm::vec2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    const glm::vec2 uv[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    quad(p, uv, c, HUD_TEX_GUI);
}

void HudBatch::text(float x, float y, float scale, const std::string& s, glm::vec4 c, bool shadow) {
    if (shadow) {
        glm::vec4 sc(c.r * 0.22f, c.g * 0.22f, c.b * 0.22f, c.a);
        text(x + scale, y + scale, scale, s, sc, false);
    }
    float cx = x;
    for (unsigned char ch : s) {
        if (ch != ' ') sprite(glyphRect(ch), cx, y, GLYPH_W * scale, GLYPH_H * scale, c);
        cx += GLYPH_ADVANCE * scale;
    }
}

void HudBatch::itemIcon(ItemId id, float cx, float cy, float s) {
    if (!itemValid(id)) return;
    const ItemInfo& info = itemInfo(id);
    if (!info.isBlock || info.flatIcon) {
        const UVRect t = tileUV(info.iconSide);
        const float h = s * 1.15f;
        const glm::vec2 p[4] = {{cx - h, cy - h}, {cx + h, cy - h}, {cx + h, cy + h}, {cx - h, cy + h}};
        const glm::vec2 uv[4] = {{t.u0, t.v1}, {t.u1, t.v1}, {t.u1, t.v0}, {t.u0, t.v0}};
        quad(p, uv, glm::vec4(1.0f), HUD_TEX_ATLAS);
        return;
    }
    const UVRect top = tileUV(info.iconTop), side = tileUV(info.iconSide);
    const float w = 0.866f * s;
    // Teilformen (Treppe, Platte, Zaun, Mauer, Bett, Acker) als Quader in derselben Schraegansicht wie der Vollblock
    const Shape shape = id < BLOCK_COUNT ? blockInfo(static_cast<BlockId>(id)).shape : SH_FULL;
    if (shape != SH_FULL) {
        struct Part { float p0, p1, q0, q1, h0, h1; };
        Part parts[5];
        int np = 0;
        auto add = [&](float p0, float p1, float q0, float q1, float h0, float h1) { parts[np++] = Part{p0, p1, q0, q1, h0, h1}; };
        switch (shape) {
            case SH_STAIRS: add(0, 1, 0, 1, 0, 0.5f); add(0, 0.5f, 0, 1, 0.5f, 1); break;  // hohe Stufe hinten
            case SH_SLAB: add(0, 1, 0, 1, 0, 0.5f); break;
            case SH_FENCE:
                add(0.375f, 0.625f, 0.375f, 0.625f, 0, 1);
                add(0.44f, 0.56f, 0.625f, 1, 0.6f, 0.8f);
                add(0.44f, 0.56f, 0.625f, 1, 0.2f, 0.4f);
                add(0.625f, 1, 0.44f, 0.56f, 0.6f, 0.8f);
                add(0.625f, 1, 0.44f, 0.56f, 0.2f, 0.4f);
                break;
            case SH_WALL:
                add(0.25f, 0.75f, 0.25f, 0.75f, 0, 1);
                add(0.3125f, 0.6875f, 0.75f, 1, 0, 0.8f);
                add(0.75f, 1, 0.3125f, 0.6875f, 0, 0.8f);
                break;
            case SH_BED: add(0, 1, 0, 1, 0, 0.56f); break;
            case SH_FARMLAND: add(0, 1, 0, 1, 0, 0.94f); break;
            default: add(0, 1, 0, 1, 0, 1); break;
        }
        // Punkt in der Schraegansicht: p laeuft nach links-vorn, q nach rechts-vorn, h nach oben
        auto P = [&](float pp, float qq, float hh) { return glm::vec2(cx + (qq - pp) * w, cy - s + (pp + qq) * 0.5f * s + (1.0f - hh) * s); };
        for (int i = 0; i < np; ++i) {
            const Part& b = parts[i];
            {   // Oberseite
                const glm::vec2 pt[4] = {P(b.p0, b.q0, b.h1), P(b.p0, b.q1, b.h1), P(b.p1, b.q1, b.h1), P(b.p1, b.q0, b.h1)};
                auto U = [&](float q) { return top.u0 + (top.u1 - top.u0) * q; };
                auto V = [&](float pp) { return top.v1 - (top.v1 - top.v0) * pp; };
                const glm::vec2 uv[4] = {{U(b.q0), V(b.p0)}, {U(b.q1), V(b.p0)}, {U(b.q1), V(b.p1)}, {U(b.q0), V(b.p1)}};
                quad(pt, uv, glm::vec4(1.0f), HUD_TEX_ATLAS);
            }
            auto SV = [&](float hh) { return side.v0 + (side.v1 - side.v0) * hh; };
            {   // linke Seite (p = p1)
                const glm::vec2 pt[4] = {P(b.p1, b.q0, b.h1), P(b.p1, b.q1, b.h1), P(b.p1, b.q1, b.h0), P(b.p1, b.q0, b.h0)};
                auto U = [&](float q) { return side.u0 + (side.u1 - side.u0) * q; };
                const glm::vec2 uv[4] = {{U(b.q0), SV(b.h1)}, {U(b.q1), SV(b.h1)}, {U(b.q1), SV(b.h0)}, {U(b.q0), SV(b.h0)}};
                quad(pt, uv, glm::vec4(0.78f, 0.78f, 0.78f, 1.0f), HUD_TEX_ATLAS);
            }
            {   // rechte Seite (q = q1)
                const glm::vec2 pt[4] = {P(b.p1, b.q1, b.h1), P(b.p0, b.q1, b.h1), P(b.p0, b.q1, b.h0), P(b.p1, b.q1, b.h0)};
                auto U = [&](float pp) { return side.u0 + (side.u1 - side.u0) * (1.0f - pp); };
                const glm::vec2 uv[4] = {{U(b.p1), SV(b.h1)}, {U(b.p0), SV(b.h1)}, {U(b.p0), SV(b.h0)}, {U(b.p1), SV(b.h0)}};
                quad(pt, uv, glm::vec4(0.58f, 0.58f, 0.58f, 1.0f), HUD_TEX_ATLAS);
            }
        }
        return;
    }
    {
        const glm::vec2 p[4] = {{cx, cy - s}, {cx + w, cy - 0.5f * s}, {cx, cy}, {cx - w, cy - 0.5f * s}};
        const glm::vec2 uv[4] = {{top.u0, top.v1}, {top.u1, top.v1}, {top.u1, top.v0}, {top.u0, top.v0}};
        quad(p, uv, glm::vec4(1.0f), HUD_TEX_ATLAS);
    }
    {
        const glm::vec2 p[4] = {{cx - w, cy - 0.5f * s}, {cx, cy}, {cx, cy + s}, {cx - w, cy + 0.5f * s}};
        const glm::vec2 uv[4] = {{side.u0, side.v1}, {side.u1, side.v1}, {side.u1, side.v0}, {side.u0, side.v0}};
        quad(p, uv, glm::vec4(0.78f, 0.78f, 0.78f, 1.0f), HUD_TEX_ATLAS);
    }
    {
        const glm::vec2 p[4] = {{cx, cy}, {cx + w, cy - 0.5f * s}, {cx + w, cy + 0.5f * s}, {cx, cy + s}};
        const glm::vec2 uv[4] = {{side.u0, side.v1}, {side.u1, side.v1}, {side.u1, side.v0}, {side.u0, side.v0}};
        quad(p, uv, glm::vec4(0.58f, 0.58f, 0.58f, 1.0f), HUD_TEX_ATLAS);
    }
}

static int g_guiSize = 0;
void setGuiSize(int size) { g_guiSize = std::max(0, std::min(4, size)); }
int guiSizeSetting() { return g_guiSize; }

float guiScale(int windowHeight) {
    const float autoScale = std::max(1.0f, std::floor(static_cast<float>(windowHeight) / 360.0f));
    if (g_guiSize == 0) return autoScale;
    // feste Stufe, aber nie so gross, dass ein kleines Fenster nur noch Oberflaeche zeigt
    return std::max(1.0f, std::min(static_cast<float>(g_guiSize), std::floor(static_cast<float>(windowHeight) / 180.0f)));
}

float guiScaleFit(int windowHeight, float units, float wanted) {
    const float fit = std::floor((static_cast<float>(windowHeight) - 8.0f) / std::max(1.0f, units));
    return std::max(1.0f, std::min(wanted, fit));
}

// ---------------------------------------------------------------------------------------------------------------
// Layout des Inventarfensters
// ---------------------------------------------------------------------------------------------------------------

// Layout bei der Stufe s (s = 1 liefert die Masse in Einheiten)
static InvLayout layoutAt(float s, int w, int h, bool creative, int craftSize, bool furnace, bool chest, int tradeRows) {
    InvLayout l;
    l.furnace = furnace;
    l.chest = chest;
    l.tradeRows = tradeRows;
    if (chest || tradeRows > 0) craftSize = 0;
    l.s = s;
    l.creative = creative;
    l.craftSize = craftSize;
    const float pad = 7.0f * s, title = 12.0f * s, pt = 18.0f * s;
    const float width = 9.0f * pt + 2.0f * pad;
    float height = pad;
    if (creative) height += pt + 2.0f * s;  // Reiterleiste
    height += title;
    float craftOffset = 0.0f;
    if (creative) height += InvLayout::PALETTE_ROWS * pt + 8.0f * s;
    l.armor = craftSize > 0 && !furnace && !chest && tradeRows == 0 && !creative;
    if (craftSize > 0) {
        craftOffset = height;
        height += std::max(craftSize, l.armor ? 4 : 0) * pt + 8.0f * s;
    }
    float chestOffset = 0.0f;
    if (chest) {
        chestOffset = height;
        height += 3.0f * pt + 8.0f * s;
    } else if (tradeRows > 0) {
        chestOffset = height;
        height += static_cast<float>(tradeRows) * pt + 8.0f * s;
    }
    const float storageOffset = height;
    height += 3.0f * pt + 4.0f * s + pt + pad;

    l.panelSize = glm::vec2(width, height);
    l.panelPos = glm::vec2(std::floor((w - width) * 0.5f), std::floor((h - height) * 0.5f));
    l.tabsTop = l.panelPos + glm::vec2(pad, pad);
    l.titlePos = l.panelPos + glm::vec2(pad, creative ? pad + pt + 2.0f * s + 2.0f * s : 6.0f * s);
    l.paletteTop = l.panelPos + glm::vec2(pad, pad + title + (creative ? pt + 2.0f * s : 0.0f));
    l.craftTop = l.panelPos + glm::vec2(pad + pt, craftOffset);
    l.resultPos = l.panelPos + glm::vec2(pad + 6.0f * pt, craftOffset + (craftSize - 1) * 9.0f * s);
    l.chestTop = l.panelPos + glm::vec2(pad, chestOffset);
    l.storageTop = l.panelPos + glm::vec2(pad, storageOffset);
    if (l.armor && craftSize == 2) l.figure = glm::vec4(l.craftTop.x, l.craftTop.y + 2.0f * pt, 2.0f * pt, 2.0f * pt);
    l.hotbarTop = l.storageTop + glm::vec2(0.0f, 3.0f * pt + 4.0f * s);
    return l;
}

// Eigener Skalierungsfaktor fuer die Inventarfenster: groesser als die uebrige Oberflaeche (1,5-fach), soweit es ins Fenster passt
InvLayout InvLayout::make(int w, int h, bool creative, int craftSize, bool furnace, bool chest, int tradeRows) {
    const InvLayout unit = layoutAt(1.0f, w, h, creative, craftSize, furnace, chest, tradeRows);
    const float hud = guiScale(h);
    const float wanted = std::max(hud, std::floor(hud * 1.5f + 0.5f));
    float s = guiScaleFit(h, unit.panelSize.y, wanted);
    const float fitW = std::floor((static_cast<float>(w) - 8.0f) / unit.panelSize.x);
    s = std::max(1.0f, std::min(s, fitW));
    return layoutAt(s, w, h, creative, craftSize, furnace, chest, tradeRows);
}

glm::vec2 InvLayout::slotPos(int index) const {
    if (index < Inventory::HOTBAR) return hotbarTop + glm::vec2(index * pitch(), 0.0f);
    int i = index - Inventory::HOTBAR;
    return storageTop + glm::vec2((i % 9) * pitch(), (i / 9) * pitch());
}

glm::vec2 InvLayout::palettePos(int index) const {
    return paletteTop + glm::vec2((index % PALETTE_COLS) * pitch(), (index / PALETTE_COLS) * pitch());
}

glm::vec2 InvLayout::tabPos(int tab) const { return tabsTop + glm::vec2(tab * pitch(), 0.0f); }

static bool inRect(glm::vec2 m, glm::vec2 p, float w, float h) {
    return m.x >= p.x && m.x < p.x + w && m.y >= p.y && m.y < p.y + h;
}

int InvLayout::hitSlot(glm::vec2 m) const {
    for (int i = 0; i < Inventory::SIZE; ++i)
        if (inRect(m, slotPos(i), pitch(), pitch())) return i;
    return -1;
}

int InvLayout::hitPalette(glm::vec2 m) const {
    if (!creative) return -1;
    for (int i = 0; i < PALETTE_PAGE; ++i)
        if (inRect(m, palettePos(i), pitch(), pitch())) return i;
    return -1;
}

int InvLayout::hitTab(glm::vec2 m) const {
    if (!creative) return -1;
    for (int t = 0; t <= CREATIVE_TAB_COUNT; ++t)
        if (inRect(m, tabPos(t), pitch(), pitch())) return t;
    return -1;
}

bool InvLayout::hitScrollbar(glm::vec2 m) const {
    if (!creative) return false;
    return inRect(m, paletteTop + glm::vec2(PALETTE_COLS * pitch(), 0.0f), 7.0f * s, scrollbarHeight());
}

glm::vec2 InvLayout::craftPos(int index) const {
    return craftTop + glm::vec2((index % craftSize) * pitch(), (index / craftSize) * pitch());
}

int InvLayout::hitCraft(glm::vec2 m) const {
    if (furnace) return -1;
    for (int i = 0; i < craftSize * craftSize; ++i)
        if (inRect(m, craftPos(i), pitch(), pitch())) return i;
    return -1;
}

bool InvLayout::hitResult(glm::vec2 m) const { return craftSize > 0 && !furnace && inRect(m, resultPos, pitch(), pitch()); }

glm::vec2 InvLayout::furnaceSlotPos(int which) const {
    if (which == 0) return craftTop;
    if (which == 1) return craftTop + glm::vec2(0.0f, 2.0f * pitch());
    return resultPos;
}

int InvLayout::hitFurnace(glm::vec2 m) const {
    if (!furnace) return -1;
    for (int i = 0; i < 3; ++i)
        if (inRect(m, furnaceSlotPos(i), pitch(), pitch())) return i;
    return -1;
}

glm::vec2 InvLayout::chestPos(int index) const { return chestTop + glm::vec2((index % 9) * pitch(), (index / 9) * pitch()); }

int InvLayout::hitChest(glm::vec2 m) const {
    if (!chest) return -1;
    for (int i = 0; i < Chest::SIZE; ++i)
        if (inRect(m, chestPos(i), pitch(), pitch())) return i;
    return -1;
}

int InvLayout::hitArmor(glm::vec2 m) const {
    if (!armor) return -1;
    for (int i = 0; i < 4; ++i)
        if (inRect(m, armorPos(i), pitch(), pitch())) return i;
    return -1;
}

int InvLayout::hitTrade(glm::vec2 m) const {
    for (int i = 0; i < tradeRows; ++i)
        if (inRect(m, tradePos(i), 9.0f * pitch(), pitch())) return i;
    return -1;
}

bool InvLayout::inside(glm::vec2 m) const { return inRect(m, panelPos, panelSize.x, panelSize.y); }

// Rezeptbuch: rechts neben dem Fenster, sonst links; schmale Fenster: ueberlappend am rechten Rand
BookLayout BookLayout::make(const InvLayout& inv, int windowW, int windowH) {
    (void)windowH;
    BookLayout b;
    b.s = inv.s;
    const float s = b.s, pad = 7.0f * s, pt = 18.0f * s;
    b.cols = 6;
    const float width = b.cols * pt + 2.0f * pad;
    float x = inv.panelPos.x + inv.panelSize.x + 6.0f * s;
    if (x + width > static_cast<float>(windowW)) x = inv.panelPos.x - 6.0f * s - width;
    if (x < 0.0f) x = std::max(0.0f, static_cast<float>(windowW) - width);
    b.pos = glm::vec2(std::floor(x), inv.panelPos.y);
    b.size = glm::vec2(width, inv.panelSize.y);
    const float header = pad + 12.0f * s + 14.0f * s;  // Titel + Filterknopf
    b.gridTop = b.pos + glm::vec2(pad, header);
    b.rows = std::max(1, static_cast<int>((b.size.y - header - pad) / pt));
    b.filterPos = b.pos + glm::vec2(pad, pad + 11.0f * s);
    b.filterSize = glm::vec2(b.cols * pt, 12.0f * s);
    return b;
}

glm::vec2 BookLayout::entryPos(int vi) const { return gridTop + glm::vec2((vi % cols) * pitch(), (vi / cols) * pitch()); }

int BookLayout::maxScroll(int total) const {
    const int totalRows = (total + cols - 1) / cols;
    return std::max(0, totalRows - rows);
}

int BookLayout::hitEntry(glm::vec2 m, int total, int scroll) const {
    for (int vi = 0; vi < cols * rows; ++vi) {
        const int idx = scroll * cols + vi;
        if (idx >= total) break;
        if (inRect(m, entryPos(vi), pitch(), pitch())) return idx;
    }
    return -1;
}

bool BookLayout::hitFilter(glm::vec2 m) const { return inRect(m, filterPos, filterSize.x, filterSize.y); }
bool BookLayout::inside(glm::vec2 m) const { return inRect(m, pos, size.x, size.y); }

// ---------------------------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------------------------

static void drawDurability(HudBatch& b, const ItemStack& st, float x, float y, float pitch, float s) {
    if (st.empty()) return;
    const ItemInfo& info = itemInfo(st.id);
    if (info.maxDurability <= 0 || st.damage <= 0) return;
    const float frac = std::max(0.0f, 1.0f - static_cast<float>(st.damage) / info.maxDurability);
    const float bw = pitch - 6.0f * s, bx = x + 3.0f * s, by = y + pitch - 4.5f * s;
    b.rect(bx, by, bx + bw, by + 1.5f * s, glm::vec4(0.0f, 0.0f, 0.0f, 0.9f));
    b.rect(bx, by, bx + bw * frac, by + 1.5f * s, glm::vec4(1.0f - frac, 0.2f + 0.8f * frac, 0.1f, 1.0f));
}

static void drawSlotCount(HudBatch& b, const ItemStack& st, float x, float y, float pitch, float s) {
    if (st.count <= 1) return;
    std::string n = std::to_string(st.count);
    float w = textWidthPx(n) * s;
    b.text(x + pitch - w - 1.5f * s, y + pitch - 8.0f * s - 0.5f * s, s, n);
}

static void drawInventoryScreen(HudBatch& b, const HudState& st, int w, int h) {
    const InvLayout L = InvLayout::make(w, h, st.paletteOpen, st.craftSize, st.furnaceOpen, st.chestOpen, st.tradeOpen ? static_cast<int>(st.trades.size()) : 0);
    const float s = L.s, pt = L.pitch();
    b.rect(0, 0, static_cast<float>(w), static_cast<float>(h), glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));

    // Fenster
    b.rect(L.panelPos.x - 2 * s, L.panelPos.y - 2 * s, L.panelPos.x + L.panelSize.x + 2 * s,
           L.panelPos.y + L.panelSize.y + 2 * s, glm::vec4(0.05f, 0.05f, 0.07f, 0.95f));
    b.rect(L.panelPos.x, L.panelPos.y, L.panelPos.x + L.panelSize.x, L.panelPos.y + L.panelSize.y,
           glm::vec4(0.30f, 0.31f, 0.35f, 0.97f));
    std::string title = st.windowTitle;
    if (title.empty())
        title = st.paletteOpen ? "Kreativ-Inventar" : (st.chestOpen ? "Truhe" : (st.furnaceOpen ? "Ofen" : (st.craftSize == 3 ? "Werkbank" : "Inventar")));
    if (st.paletteOpen) {
        title = st.creativeTab == CREATIVE_SEARCH_TAB ? "Suche: " + st.creativeQuery + "_" : creativeTabName(st.creativeTab);
    }
    b.text(L.titlePos.x, L.titlePos.y, s, title, glm::vec4(1.0f));

    const glm::vec4 slotBg(0.12f, 0.12f, 0.15f, 1.0f);
    const glm::vec4 hoverFx(1, 1, 1, 0.3f);
    const int hoverSlot = L.hitSlot(st.mouse), hoverPal = L.hitPalette(st.mouse);
    const int hoverCraft = L.hitCraft(st.mouse);
    const bool hoverResult = L.hitResult(st.mouse);
    auto slotRect = [&](glm::vec2 p, bool hover) {
        b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, slotBg);
        if (hover) b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, hoverFx);
    };

    // Auswahlfeld (Kreativ): Reiterleiste, 9 x 5 Felder mit Bildlauf
    const int hoverTab = L.hitTab(st.mouse);
    const std::vector<ItemId> noItems;
    const std::vector<ItemId>& palItems = st.paletteItems ? *st.paletteItems : noItems;
    if (st.paletteOpen) {
        for (int t = 0; t <= CREATIVE_TAB_COUNT; ++t) {
            const glm::vec2 p = L.tabPos(t);
            const bool sel = t == st.creativeTab;
            b.rect(p.x, p.y, p.x + pt, p.y + pt, glm::vec4(0.05f, 0.05f, 0.07f, 1.0f));
            glm::vec4 fill = sel ? glm::vec4(0.38f, 0.41f, 0.50f, 1.0f) : glm::vec4(0.18f, 0.19f, 0.23f, 1.0f);
            if (t == hoverTab && !sel) fill += glm::vec4(0.10f, 0.10f, 0.12f, 0.0f);
            b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - (sel ? 0.0f : s), fill);
            if (t == CREATIVE_SEARCH_TAB) {
                // Lupe aus Rechtecken
                const float cx = p.x + pt * 0.5f, cy = p.y + pt * 0.5f;
                const glm::vec4 lc(0.92f, 0.94f, 1.0f, 1.0f);
                b.rect(cx - 4 * s, cy - 4.5f * s, cx + 2 * s, cy - 3.5f * s, lc);
                b.rect(cx - 4 * s, cy + 0.5f * s, cx + 2 * s, cy + 1.5f * s, lc);
                b.rect(cx - 4 * s, cy - 4.5f * s, cx - 3 * s, cy + 1.5f * s, lc);
                b.rect(cx + 1 * s, cy - 4.5f * s, cx + 2 * s, cy + 1.5f * s, lc);
                b.rect(cx + 2 * s, cy + 1.5f * s, cx + 3 * s, cy + 2.5f * s, lc);
                b.rect(cx + 3 * s, cy + 2.5f * s, cx + 4.5f * s, cy + 4 * s, lc);
            } else {
                b.itemIcon(creativeTabIcon(t), p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
            }
        }
        for (int vi = 0; vi < InvLayout::PALETTE_PAGE; ++vi) {
            const glm::vec2 p = L.palettePos(vi);
            const size_t idx = static_cast<size_t>(st.creativeScroll * InvLayout::PALETTE_COLS + vi);
            const bool has = idx < palItems.size();
            slotRect(p, has && vi == hoverPal);
            if (has) b.itemIcon(palItems[idx], p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
        }
        // Bildlaufleiste
        {
            const int totalRows = static_cast<int>((palItems.size() + InvLayout::PALETTE_COLS - 1) / InvLayout::PALETTE_COLS);
            const int maxScroll = std::max(0, totalRows - InvLayout::PALETTE_ROWS);
            const float bx = L.paletteTop.x + InvLayout::PALETTE_COLS * pt + 2.0f * s, bw = 4.0f * s;
            const float top = L.scrollbarTop(), hh = L.scrollbarHeight();
            b.rect(bx, top, bx + bw, top + hh, glm::vec4(0.12f, 0.12f, 0.15f, 1.0f));
            const float handle = maxScroll > 0 ? std::max(6.0f * s, hh * InvLayout::PALETTE_ROWS / static_cast<float>(totalRows)) : hh;
            const float hy = maxScroll > 0 ? top + (hh - handle) * static_cast<float>(st.creativeScroll) / static_cast<float>(maxScroll) : top;
            b.rect(bx, hy, bx + bw, hy + handle, maxScroll > 0 ? glm::vec4(0.75f, 0.78f, 0.88f, 1.0f) : glm::vec4(0.28f, 0.29f, 0.34f, 1.0f));
        }
        float sepY = L.storageTop.y - 5.0f * s;
        b.rect(L.panelPos.x + 7 * s, sepY, L.panelPos.x + L.panelSize.x - 7 * s, sepY + s, glm::vec4(0, 0, 0, 0.5f));
    }

    // Truhe: 3 Reihen
    const int hoverChest = L.hitChest(st.mouse);
    if (st.chestOpen && st.chest) {
        for (int i = 0; i < Chest::SIZE; ++i) {
            const glm::vec2 p = L.chestPos(i);
            slotRect(p, i == hoverChest);
            if (!st.chest->slots[static_cast<size_t>(i)].empty())
                b.itemIcon(st.chest->slots[static_cast<size_t>(i)].id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
        }
        const float sepY = L.storageTop.y - 5.0f * s;
        b.rect(L.panelPos.x + 7 * s, sepY, L.panelPos.x + L.panelSize.x - 7 * s, sepY + s, glm::vec4(0, 0, 0, 0.5f));
    }

    // Handel: je Angebot eine Reihe
    const int hoverTrade = L.hitTrade(st.mouse);
    if (st.tradeOpen) {
        for (size_t i = 0; i < st.trades.size(); ++i) {
            const glm::vec2 p = L.tradePos(static_cast<int>(i));
            const HudState::TradeRowHud& tr = st.trades[i];
            const glm::vec4 rowBg = tr.affordable ? glm::vec4(0.16f, 0.30f, 0.16f, 1.0f) : slotBg;
            b.rect(p.x + s, p.y + s, p.x + 9.0f * pt - s, p.y + pt - s, rowBg);
            if (static_cast<int>(i) == hoverTrade) b.rect(p.x + s, p.y + s, p.x + 9.0f * pt - s, p.y + pt - s, hoverFx);
            b.itemIcon(tr.cost.id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
            drawSlotCount(b, tr.cost, p.x, p.y, pt, s);
            b.text(p.x + pt * 1.3f, p.y + pt * 0.5f - 4.0f * s, s * 1.5f, "=>", glm::vec4(0.9f));
            b.itemIcon(tr.result.id, p.x + pt * 3.5f, p.y + pt * 0.5f, 6.3f * s);
            drawSlotCount(b, tr.result, p.x + pt * 3.0f, p.y, pt, s);
            b.text(p.x + pt * 4.3f, p.y + pt * 0.5f - 4.0f * s, s, itemInfo(tr.result.id).name,
                   tr.affordable ? glm::vec4(1.0f) : glm::vec4(0.7f, 0.7f, 0.75f, 1.0f));
            if (!tr.affordable) b.rect(p.x + s, p.y + s, p.x + 9.0f * pt - s, p.y + pt - s, glm::vec4(0.0f, 0.0f, 0.0f, 0.35f));
        }
        const float sepY = L.storageTop.y - 5.0f * s;
        b.rect(L.panelPos.x + 7 * s, sepY, L.panelPos.x + L.panelSize.x - 7 * s, sepY + s, glm::vec4(0, 0, 0, 0.5f));
    }

    // Ofen: Eingabe, Brennstoff, Flamme, Fortschrittspfeil, Ausgabe
    const int hoverFurnace = L.hitFurnace(st.mouse);
    if (st.furnaceOpen) {
        for (int i = 0; i < 3; ++i) slotRect(L.furnaceSlotPos(i), i == hoverFurnace);
        const ItemStack* fs[3] = {&st.furnaceIn, &st.furnaceFuel, &st.furnaceOut};
        for (int i = 0; i < 3; ++i) {
            const glm::vec2 p = L.furnaceSlotPos(i);
            if (!fs[i]->empty()) b.itemIcon(fs[i]->id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
        }
        // Flamme zwischen Eingabe und Brennstoff
        {
            const glm::vec2 p = L.furnaceSlotPos(0) + glm::vec2(pt * 0.25f, pt * 1.15f);
            const float fw = pt * 0.5f, fh = pt * 0.6f;
            b.rect(p.x, p.y, p.x + fw, p.y + fh, glm::vec4(0.12f, 0.1f, 0.1f, 1.0f));
            const float fill = std::max(0.0f, std::min(1.0f, st.furnaceBurn));
            b.rect(p.x, p.y + fh * (1.0f - fill), p.x + fw, p.y + fh, glm::vec4(1.0f, 0.55f, 0.1f, 1.0f));
        }
        // Pfeil: Fortschritt
        {
            const float ax = L.craftTop.x + 2.2f * pt, ay = L.resultPos.y + pt * 0.5f - 3.0f * s, aw = 3.0f * pt;
            b.rect(ax, ay, ax + aw, ay + 6.0f * s, glm::vec4(0.12f, 0.12f, 0.15f, 1.0f));
            b.rect(ax, ay, ax + aw * std::max(0.0f, std::min(1.0f, st.furnaceCook)), ay + 6.0f * s,
                   glm::vec4(0.9f, 0.9f, 0.95f, 1.0f));
        }
    }

    // Handwerk: Raster, Pfeil, Ergebnis
    if (st.craftSize > 0 && !st.furnaceOpen) {
        for (int i = 0; i < st.craftSize * st.craftSize; ++i) {
            glm::vec2 p = L.craftPos(i);
            slotRect(p, i == hoverCraft);
            const ItemStack& cs = st.craftCells[i];
            if (!cs.empty()) {
                b.itemIcon(cs.id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
            }
        }
        const float ay = L.resultPos.y + pt * 0.5f - 4.0f * s;
        b.text(L.craftTop.x + st.craftSize * pt + 0.7f * pt, ay, s * 1.5f, "=>", glm::vec4(0.9f));
        slotRect(L.resultPos, hoverResult);
        if (!st.craftResult.empty())
            b.itemIcon(st.craftResult.id, L.resultPos.x + pt * 0.5f, L.resultPos.y + pt * 0.5f, 6.3f * s);
    }

    // Ruestungsplaetze und Figur
    const int hoverArmor = L.hitArmor(st.mouse);
    if (L.armor) {
        for (int i = 0; i < 4; ++i) {
            const glm::vec2 p = L.armorPos(i);
            slotRect(p, i == hoverArmor);
            const ItemStack& as = st.armorSlots[static_cast<size_t>(i)];
            if (!as.empty()) {
                b.itemIcon(as.id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
                drawDurability(b, as, p.x, p.y, pt, s);
            } else {  // schwacher Hinweis, welches Teil hierher gehoert
                static const char* hint[4] = {"H", "B", "O", "S"};
                b.text(p.x + pt * 0.5f - 2.5f * s, p.y + pt * 0.5f - 3.5f * s, s, hint[i], glm::vec4(1, 1, 1, 0.25f));
            }
        }
        if (L.figure.z > 0.0f) {
            const float fx = L.figure.x, fy = L.figure.y, fw = L.figure.z, fh = L.figure.w;
            b.rect(fx, fy, fx + fw, fy + fh, glm::vec4(0.12f, 0.12f, 0.15f, 1.0f));
            const glm::vec2 c(fx + fw * 0.5f, fy + fh * 0.5f);
            const float scale = std::min(fw, fh) * 0.5f * 0.95f;
            for (const PreviewQuad& q : st.invPreview) {
                glm::vec2 pts2[4];
                for (int k = 0; k < 4; ++k) pts2[k] = c + q.p[k] * scale;
                b.quad(pts2, q.uv, glm::vec4(q.shade, q.shade, q.shade, 1.0f), HUD_TEX_SKIN);
            }
        }
    }

    // Rucksack + Hotbar
    const Inventory* inv = st.inventory;
    for (int i = 0; i < Inventory::SIZE; ++i) {
        glm::vec2 p = L.slotPos(i);
        slotRect(p, i == hoverSlot);
        if (i < Inventory::HOTBAR && i == st.selectedSlot)
            b.rect(p.x, p.y, p.x + pt, p.y + s, glm::vec4(1, 1, 1, 0.9f));  // Markierung der aktiven Hotbar
        if (inv && !inv->slots[i].empty()) b.itemIcon(inv->slots[i].id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
    }
    // Zaehler und Haltbarkeit liegen ueber den Symbolen
    if (inv) {
        for (int i = 0; i < Inventory::SIZE; ++i) {
            glm::vec2 p = L.slotPos(i);
            if (!st.creative) drawSlotCount(b, inv->slots[i], p.x, p.y, pt, s);
            drawDurability(b, inv->slots[i], p.x, p.y, pt, s);
        }
    }
    if (st.chestOpen && st.chest) {
        for (int i = 0; i < Chest::SIZE; ++i) {
            const glm::vec2 p = L.chestPos(i);
            drawSlotCount(b, st.chest->slots[static_cast<size_t>(i)], p.x, p.y, pt, s);
            drawDurability(b, st.chest->slots[static_cast<size_t>(i)], p.x, p.y, pt, s);
        }
    }
    if (st.furnaceOpen) {
        const ItemStack* fs[3] = {&st.furnaceIn, &st.furnaceFuel, &st.furnaceOut};
        for (int i = 0; i < 3; ++i) {
            const glm::vec2 p = L.furnaceSlotPos(i);
            drawSlotCount(b, *fs[i], p.x, p.y, pt, s);
            drawDurability(b, *fs[i], p.x, p.y, pt, s);
        }
    } else if (st.craftSize > 0) {
        for (int i = 0; i < st.craftSize * st.craftSize; ++i) {
            glm::vec2 p = L.craftPos(i);
            drawSlotCount(b, st.craftCells[i], p.x, p.y, pt, s);
            drawDurability(b, st.craftCells[i], p.x, p.y, pt, s);
        }
        drawSlotCount(b, st.craftResult, L.resultPos.x, L.resultPos.y, pt, s);
    }

    // Tooltip
    ItemId hoverItem = ITEM_NONE;
    std::string hoverName;
    if (hoverTab >= 0) hoverName = creativeTabName(hoverTab);
    else if (hoverPal >= 0) {
        const size_t idx = static_cast<size_t>(st.creativeScroll * InvLayout::PALETTE_COLS + hoverPal);
        if (idx < palItems.size()) hoverItem = palItems[idx];
    } else if (hoverSlot >= 0 && inv && !inv->slots[hoverSlot].empty()) hoverItem = inv->slots[hoverSlot].id;
    else if (hoverCraft >= 0 && !st.craftCells[hoverCraft].empty()) hoverItem = st.craftCells[hoverCraft].id;
    else if (hoverArmor >= 0 && !st.armorSlots[static_cast<size_t>(hoverArmor)].empty()) hoverItem = st.armorSlots[static_cast<size_t>(hoverArmor)].id;
    else if (hoverChest >= 0 && st.chest && !st.chest->slots[static_cast<size_t>(hoverChest)].empty())
        hoverItem = st.chest->slots[static_cast<size_t>(hoverChest)].id;
    else if (hoverFurnace == 0 && !st.furnaceIn.empty()) hoverItem = st.furnaceIn.id;
    else if (hoverFurnace == 1 && !st.furnaceFuel.empty()) hoverItem = st.furnaceFuel.id;
    else if (hoverFurnace == 2 && !st.furnaceOut.empty()) hoverItem = st.furnaceOut.id;
    else if (hoverResult && !st.craftResult.empty()) hoverItem = st.craftResult.id;
    if ((hoverItem != ITEM_NONE || !hoverName.empty()) && st.cursor.empty()) {
        std::string name = hoverItem != ITEM_NONE ? std::string(itemInfo(hoverItem).name) : hoverName;
        float tw = textWidthPx(name) * s;
        float tx = st.mouse.x + 10 * s, ty = st.mouse.y - 12 * s;
        b.rect(tx - 3 * s, ty - 3 * s, tx + tw + 3 * s, ty + 8 * s + 3 * s, glm::vec4(0.08f, 0.0f, 0.12f, 0.92f));
        b.text(tx, ty, s, name);
    }

    // Rezeptbuch
    if (st.book.open) {
        const BookLayout B = BookLayout::make(L, w, h);
        b.rect(B.pos.x - 2 * s, B.pos.y - 2 * s, B.pos.x + B.size.x + 2 * s, B.pos.y + B.size.y + 2 * s, glm::vec4(0.05f, 0.05f, 0.07f, 0.95f));
        b.rect(B.pos.x, B.pos.y, B.pos.x + B.size.x, B.pos.y + B.size.y, glm::vec4(0.30f, 0.31f, 0.35f, 0.97f));
        b.text(B.pos.x + 7 * s, B.pos.y + 6 * s, s, "Rezepte", glm::vec4(1.0f));
        const bool hf = B.hitFilter(st.mouse);
        {
            const float x0 = B.filterPos.x, y0 = B.filterPos.y, x1 = x0 + B.filterSize.x, y1 = y0 + B.filterSize.y;
            b.rect(x0 - s, y0 - s, x1 + s, y1 + s, glm::vec4(0.02f, 0.02f, 0.03f, 0.95f));
            glm::vec4 fill = st.book.onlyCraftable ? glm::vec4(0.26f, 0.42f, 0.66f, 1.0f) : glm::vec4(0.30f, 0.31f, 0.36f, 1.0f);
            if (hf) fill += glm::vec4(0.12f, 0.12f, 0.14f, 0.0f);
            b.rect(x0, y0, x1, y1, fill);
            const std::string label = st.book.onlyCraftable ? "Nur machbare" : "Alle zeigen";
            b.text(std::floor(x0 + (B.filterSize.x - textWidthPx(label) * s) * 0.5f), y0 + 2.0f * s, s, label);
        }
        const int total = static_cast<int>(st.book.entries.size());
        const int hoverEntry = B.hitEntry(st.mouse, total, st.book.scroll);
        for (int vi = 0; vi < B.cols * B.rows; ++vi) {
            const int idx = st.book.scroll * B.cols + vi;
            if (idx >= total) break;
            const BookEntryHud& e = st.book.entries[static_cast<size_t>(idx)];
            const glm::vec2 p = B.entryPos(vi);
            b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, e.craftable ? glm::vec4(0.16f, 0.30f, 0.16f, 1.0f) : slotBg);
            if (idx == hoverEntry) b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, hoverFx);
            if (e.craftable) {  // gruener Rand
                const glm::vec4 g(0.35f, 0.85f, 0.35f, 0.9f);
                b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + 2 * s, g);
                b.rect(p.x + s, p.y + pt - 2 * s, p.x + pt - s, p.y + pt - s, g);
                b.rect(p.x + s, p.y + s, p.x + 2 * s, p.y + pt - s, g);
                b.rect(p.x + pt - 2 * s, p.y + s, p.x + pt - s, p.y + pt - s, g);
            }
            b.itemIcon(e.result.id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
            if (!e.craftable) b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
            drawSlotCount(b, e.result, p.x, p.y, pt, s);
        }
        if (total == 0) b.text(B.pos.x + 7 * s, B.gridTop.y + 4 * s, s, "Nichts zu zeigen", glm::vec4(0.8f));
        // Bildlaufleiste
        if (B.maxScroll(total) > 0) {
            const float tx = B.pos.x + B.size.x - 4.0f * s, ty0 = B.gridTop.y, th = B.rows * pt;
            b.rect(tx, ty0, tx + 2.0f * s, ty0 + th, glm::vec4(0, 0, 0, 0.4f));
            const float frac = static_cast<float>(B.rows) / static_cast<float>((total + B.cols - 1) / B.cols);
            const float hh = std::max(6.0f * s, th * frac);
            const float yy = ty0 + (th - hh) * static_cast<float>(st.book.scroll) / static_cast<float>(B.maxScroll(total));
            b.rect(tx, yy, tx + 2.0f * s, yy + hh, glm::vec4(0.85f, 0.85f, 0.9f, 0.9f));
        }
        // Hinweis: Zutaten
        if (hoverEntry >= 0 && st.cursor.empty()) {
            const BookEntryHud& e = st.book.entries[static_cast<size_t>(hoverEntry)];
            std::vector<std::string> lines;
            lines.push_back(std::string(itemInfo(e.result.id).name) + (e.result.count > 1 ? " x" + std::to_string(e.result.count) : ""));
            for (const Ingredient& ing : e.ingredients)
                lines.push_back(std::to_string(ing.count) + "x " + itemInfo(ing.item).name);
            if (!e.fits) lines.push_back("Braucht eine Werkbank");
            else if (e.craftable) lines.push_back("Klick: Raster fuellen");
            float tw = 0.0f;
            for (const std::string& ln : lines) tw = std::max(tw, textWidthPx(ln) * s);
            float tx = st.mouse.x + 10 * s, ty = st.mouse.y - 12 * s;
            if (tx + tw + 6 * s > static_cast<float>(w)) tx = st.mouse.x - tw - 12 * s;
            const float th = static_cast<float>(lines.size()) * 10.0f * s;
            b.rect(tx - 3 * s, ty - 3 * s, tx + tw + 3 * s, ty + th + 1 * s, glm::vec4(0.08f, 0.0f, 0.12f, 0.92f));
            for (size_t i = 0; i < lines.size(); ++i)
                b.text(tx, ty + static_cast<float>(i) * 10.0f * s, s, lines[i], i == 0 ? glm::vec4(1.0f) : glm::vec4(0.75f, 0.75f, 0.85f, 1.0f));
        }
    }

    // Gehaltener Stapel
    if (!st.cursor.empty()) {
        b.itemIcon(st.cursor.id, st.mouse.x, st.mouse.y, 6.3f * s);
        if (!st.creative || st.craftSize > 0 || st.furnaceOpen)
            drawSlotCount(b, st.cursor, st.mouse.x - pt * 0.5f, st.mouse.y - pt * 0.5f, pt, s);
        drawDurability(b, st.cursor, st.mouse.x - pt * 0.5f, st.mouse.y - pt * 0.5f, pt, s);
    }
}

static void drawWidget(HudBatch& b, const Widget& wd, bool hot, float s) {
    const float x0 = wd.pos.x, y0 = wd.pos.y, x1 = wd.pos.x + wd.size.x, y1 = wd.pos.y + wd.size.y;
    b.rect(x0 - s, y0 - s, x1 + s, y1 + s, glm::vec4(0.02f, 0.02f, 0.03f, 0.95f));
    if (wd.swatch) {
        b.rect(x0, y0, x1, y1, wd.color);
        if (wd.selected || hot) b.rect(x0 - s, y0 - s, x1 + s, y0, glm::vec4(1, 1, 1, 0.9f));
        return;
    }
    glm::vec4 fill(0.30f, 0.31f, 0.36f, 1.0f);
    if (!wd.enabled) fill = glm::vec4(0.16f, 0.16f, 0.20f, 1.0f);
    else if (wd.selected) fill = glm::vec4(0.26f, 0.42f, 0.66f, 1.0f);
    if (hot && wd.enabled) fill += glm::vec4(0.12f, 0.12f, 0.14f, 0.0f);
    b.rect(x0, y0, x1, y1, fill);
    if (wd.type == Widget::Slider) {
        const float fx = x0 + wd.size.x * wd.fraction;
        b.rect(x0, y0, fx, y1, hot ? glm::vec4(0.28f, 0.50f, 0.78f, 1.0f) : glm::vec4(0.22f, 0.42f, 0.68f, 1.0f));
        b.rect(fx - s, y0, fx + s, y1, glm::vec4(0.9f, 0.9f, 0.95f, 1.0f));
    }
    const float tw = textWidthPx(wd.text) * s;
    const float tx = wd.left ? x0 + 3.0f * s : std::floor(x0 + (wd.size.x - tw) * 0.5f);
    const glm::vec4 tc = wd.enabled ? glm::vec4(1.0f) : glm::vec4(0.7f, 0.8f, 1.0f, 1.0f);
    b.text(tx, std::floor(y0 + (wd.size.y - 8.0f * s) * 0.5f), s, wd.text, tc);
}

static void drawMenu(HudBatch& b, const MenuLayout& m, const std::vector<PreviewQuad>& preview, int hover, int w, int h) {
    const float s = m.scale;
    b.rect(0, 0, static_cast<float>(w), static_cast<float>(h), glm::vec4(0.0f, 0.0f, 0.0f, m.dim));
    const float ts = m.titleScale * s;
    const float tw = textWidthPx(m.title) * ts;
    if (m.titleScale >= 5.0f) {  // Titelbild: grosse Schrift mit Schatten und warmer Farbe
        const float tx = std::floor((w - tw) * 0.5f);
        b.text(tx + ts * 0.7f, m.titlePos.y + ts * 0.7f, ts, m.title, glm::vec4(0.05f, 0.04f, 0.02f, 0.85f));
        b.text(tx, m.titlePos.y, ts, m.title, glm::vec4(1.0f, 0.86f, 0.35f, 1.0f));
    } else {
        b.text(std::floor((w - tw) * 0.5f), m.titlePos.y, ts, m.title);
    }
    if (!m.subtitle.empty()) {
        const float ss = 1.5f * s;
        b.text(std::floor((w - textWidthPx(m.subtitle) * ss) * 0.5f), m.titlePos.y + 9.0f * ts + 6.0f * s, ss, m.subtitle, glm::vec4(0.85f, 0.88f, 0.95f, 1.0f));
    }
    for (size_t i = 0; i < m.widgets.size(); ++i) drawWidget(b, m.widgets[i], static_cast<int>(i) == hover, s);
    if (m.skinPreview.z > 0.0f) {
        const float px = m.skinPreview.x, py = m.skinPreview.y, pw = m.skinPreview.z, ph = m.skinPreview.w;
        b.rect(px - s, py - s, px + pw + s, py + ph + s, glm::vec4(0.02f, 0.02f, 0.03f, 1.0f));
        b.rect(px, py, px + pw, py + ph, glm::vec4(0.20f, 0.30f, 0.45f, 1.0f));
        b.rect(px, py + ph * 0.80f, px + pw, py + ph, glm::vec4(0.22f, 0.36f, 0.20f, 1.0f));
        const glm::vec2 c(px + pw * 0.5f, py + ph * 0.5f);
        const float scale = std::min(pw, ph) * 0.5f * 0.95f;
        for (const PreviewQuad& q : preview) {
            glm::vec2 pts[4];
            for (int k = 0; k < 4; ++k) pts[k] = c + q.p[k] * scale;
            b.quad(pts, q.uv, glm::vec4(q.shade, q.shade, q.shade, 1.0f), HUD_TEX_SKIN);
        }
    }
}

static void drawEditor(HudBatch& b, const EditorHud& e, int w, int h) {
    const EditorLayout& L = e.layout;
    const float s = L.s;
    b.rect(0, 0, static_cast<float>(w), static_cast<float>(h), glm::vec4(0.09f, 0.09f, 0.11f, 1.0f));
    b.text(L.titlePos.x, L.titlePos.y, s, e.title, glm::vec4(1.0f));

    for (size_t i = 0; i < L.widgets.size(); ++i) drawWidget(b, L.widgets[i], static_cast<int>(i) == e.hover, s);

    // Leinwand: Schachbrett fuer Transparenz, Bild, Raster, Netz
    const float cx = L.canvas.x, cy = L.canvas.y, cw = L.canvas.z, ch = L.canvas.w, z = L.zoom;
    if (e.view && e.view->valid()) {
        const float cell = std::max(z * 2.0f, 4.0f * s);
        for (float y = 0; y < ch; y += cell)
            for (float x = 0; x < cw; x += cell) {
                const bool dark = (static_cast<int>(x / cell) + static_cast<int>(y / cell)) % 2 == 0;
                const float v = dark ? 0.30f : 0.42f;
                b.rect(cx + x, cy + y, std::min(cx + x + cell, cx + cw), std::min(cy + y + cell, cy + ch), glm::vec4(v, v, v, 1.0f));
            }
        const glm::vec2 p[4] = {{cx, cy}, {cx + cw, cy}, {cx + cw, cy + ch}, {cx, cy + ch}};
        const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        b.quad(p, uv, glm::vec4(1.0f), HUD_TEX_VIEW);
        if (e.grid && z >= 4.0f * s) {
            const glm::vec4 gc(0.0f, 0.0f, 0.0f, 0.22f);
            for (int x = 0; x <= e.view->w; ++x) b.rect(cx + x * z, cy, cx + x * z + 1.0f, cy + ch, gc);
            for (int y = 0; y <= e.view->h; ++y) b.rect(cx, cy + y * z, cx + cw, cy + y * z + 1.0f, gc);
        }
        if (e.net) {
            for (const NetRegion& r : e.regions) {
                const glm::vec4 lc(1.0f, 0.95f, 0.3f, 0.9f);
                const float x0 = cx + r.x * z, y0 = cy + r.y * z, x1 = x0 + r.w * z, y1 = y0 + r.h * z;
                b.rect(x0, y0, x1, y0 + s, lc);
                b.rect(x0, y1 - s, x1, y1, lc);
                b.rect(x0, y0, x0 + s, y1, lc);
                b.rect(x1 - s, y0, x1, y1, lc);
                if (r.w * z >= 9.0f * s && r.h * z >= 11.0f * s)
                    b.text(std::floor(x0 + (r.w * z - 5.0f * s) * 0.5f), std::floor(y0 + (r.h * z - 8.0f * s) * 0.5f), s,
                           std::string(1, r.label), glm::vec4(1.0f, 1.0f, 1.0f, 0.65f));
            }
        }
        if (e.hoverPixel.x >= 0) {
            const float x0 = cx + e.hoverPixel.x * z, y0 = cy + e.hoverPixel.y * z;
            const glm::vec4 hc = e.hoverEditable ? glm::vec4(1, 1, 1, 0.95f) : glm::vec4(1, 0.3f, 0.3f, 0.8f);
            b.rect(x0, y0, x0 + z, y0 + s, hc);
            b.rect(x0, y0 + z - s, x0 + z, y0 + z, hc);
            b.rect(x0, y0, x0 + s, y0 + z, hc);
            b.rect(x0 + z - s, y0, x0 + z, y0 + z, hc);
        }
    }

    // Vorschau
    {
        const float px = L.preview.x, py = L.preview.y, pw = L.preview.z, ph = L.preview.w;
        b.rect(px - s, py - s, px + pw + s, py + ph + s, glm::vec4(0.02f, 0.02f, 0.03f, 1.0f));
        b.rect(px, py, px + pw, py + ph, glm::vec4(0.20f, 0.30f, 0.45f, 1.0f));
        b.rect(px, py + ph * 0.72f, px + pw, py + ph, glm::vec4(0.22f, 0.36f, 0.20f, 1.0f));  // Boden
        const glm::vec2 c(px + pw * 0.5f, py + ph * 0.5f);
        const float scale = std::min(pw, ph) * 0.5f * 0.92f;
        for (const PreviewQuad& q : e.quads) {
            glm::vec2 pts[4];
            for (int k = 0; k < 4; ++k) pts[k] = c + q.p[k] * scale;
            b.quad(pts, q.uv, glm::vec4(q.shade, q.shade, q.shade, 1.0f), q.skin ? HUD_TEX_SKIN : HUD_TEX_ATLAS);
        }
        b.text(px + 3 * s, py + 3 * s, s, "Vorschau", glm::vec4(1, 1, 1, 0.8f));
    }
    b.text(L.statusPos.x, L.statusPos.y, s, e.status, glm::vec4(0.85f, 0.95f, 0.85f, 1.0f));
}

void buildHud(HudBatch& b, const HudState& st, int w, int h) {
    b.clear();
    if (!st.show) return;
    const float W = static_cast<float>(w), H = static_cast<float>(h);
    const float s = guiScale(h);

    if (st.underwater) b.rect(0, 0, W, H, glm::vec4(0.08f, 0.25f, 0.55f, 0.35f));
    if (st.underLava) b.rect(0, 0, W, H, glm::vec4(0.95f, 0.35f, 0.02f, 0.6f));
    else if (st.burning) b.rect(0, H * 0.82f, W, H, glm::vec4(1.0f, 0.45f, 0.05f, 0.22f));
    if (st.damageFlash > 0.0f) b.rect(0, 0, W, H, glm::vec4(0.85f, 0.05f, 0.05f, 0.4f * st.damageFlash));

    if (st.editor.open) {
        drawEditor(b, st.editor, w, h);
        return;
    }
    if (!st.menu.widgets.empty()) {
        drawMenu(b, st.menu, st.menuPreview, st.menuHover, w, h);
        return;
    }

    if (st.inventoryOpen && !st.dead) {
        drawInventoryScreen(b, st, w, h);
    } else if (!st.dead) {
        // Fadenkreuz
        const float cx = std::floor(W * 0.5f), cy = std::floor(H * 0.5f);
        const float arm = 5.0f * s, th = std::max(1.0f, s);
        b.rect(cx - arm - 1, cy - th * 0.5f - 1, cx + arm + 1, cy + th * 0.5f + 1, glm::vec4(0, 0, 0, 0.45f));
        b.rect(cx - th * 0.5f - 1, cy - arm - 1, cx + th * 0.5f + 1, cy + arm + 1, glm::vec4(0, 0, 0, 0.45f));
        b.rect(cx - arm, cy - th * 0.5f, cx + arm, cy + th * 0.5f, glm::vec4(1, 1, 1, 0.9f));
        b.rect(cx - th * 0.5f, cy - arm, cx + th * 0.5f, cy + arm, glm::vec4(1, 1, 1, 0.9f));

        if (st.breakProgress > 0.0f) {
            const float bw = 20.0f * s, by = cy + 12.0f * s;
            b.rect(cx - bw * 0.5f - s, by - s, cx + bw * 0.5f + s, by + 2 * s + s, glm::vec4(0, 0, 0, 0.6f));
            b.rect(cx - bw * 0.5f, by, cx + bw * 0.5f * (2.0f * std::min(1.0f, st.breakProgress) - 1.0f), by + 2 * s,
                   glm::vec4(0.9f, 0.9f, 0.9f, 0.95f));
        }
    }

    // Hotbar
    if (st.inventory && !st.dead) {
        const float slot = 20.0f * s, total = slot * Inventory::HOTBAR;
        const float x0 = std::floor((W - total) * 0.5f), y0 = H - slot - 4.0f * s;
        // Im Inventarfenster steht die Hotbar schon im Fenster selbst
        if (!st.inventoryOpen) b.rect(x0 - 2 * s, y0 - 2 * s, x0 + total + 2 * s, y0 + slot + 2 * s, glm::vec4(0, 0, 0, 0.55f));
        for (int i = 0; i < Inventory::HOTBAR && !st.inventoryOpen; ++i) {
            const float sx = x0 + i * slot;
            const bool sel = i == st.selectedSlot;
            if (sel) b.rect(sx - 2 * s, y0 - 2 * s, sx + slot + 2 * s, y0 + slot + 2 * s, glm::vec4(1, 1, 1, 0.95f));
            b.rect(sx + s, y0 + s, sx + slot - s, y0 + slot - s,
                   sel ? glm::vec4(0.25f, 0.25f, 0.25f, 0.9f) : glm::vec4(0.12f, 0.12f, 0.12f, 0.75f));
        }
        for (int i = 0; i < Inventory::HOTBAR && !st.inventoryOpen; ++i) {
            const ItemStack& stack = st.inventory->slots[i];
            if (stack.empty()) continue;
            b.itemIcon(stack.id, x0 + i * slot + slot * 0.5f, y0 + slot * 0.5f, 7.0f * s);
            if (!st.creative) drawSlotCount(b, stack, x0 + i * slot, y0, slot, s);
            drawDurability(b, stack, x0 + i * slot, y0, slot, s);
        }

        // Ruestungsanzeige ueber den Herzen
        if (!st.creative && st.armorPoints > 0) {
            static const char* SHIELD[9] = {".#######.", "#########", "#########", "#########", "#########", ".#######.", "..#####..", "...###...", "....#...."};
            const float hy = y0 - 22.0f * s;
            for (int i = 0; i < 10; ++i) {
                const float hx = x0 + i * 8.0f * s;
                const int state = st.armorPoints >= 2 * i + 2 ? 2 : (st.armorPoints == 2 * i + 1 ? 1 : 0);
                for (int row = 0; row < 9; ++row)
                    for (int col = 0; col < 9; ++col) {
                        if (SHIELD[row][col] != '#') continue;
                        const bool edge = row == 0 || row == 5 || col == 0 || col == 8 || row >= 6 || (row > 0 && SHIELD[row - 1][col] != '#');
                        glm::vec4 colr = state == 2 ? glm::vec4(0.82f, 0.86f, 0.92f, 1.0f) : glm::vec4(0.16f, 0.16f, 0.2f, 0.9f);
                        if (state == 1 && col < 5) colr = glm::vec4(0.82f, 0.86f, 0.92f, 1.0f);
                        if (edge && state > 0) colr *= 0.78f;
                        colr.a = 1.0f;
                        b.rect(hx + col * s, hy + row * s, hx + (col + 1) * s, hy + (row + 1) * s, colr);
                    }
            }
        }
        // Herzen
        if (!st.creative) {
            const float hy = y0 - 12.0f * s;
            for (int i = 0; i < 10; ++i) {
                const float hx = x0 + i * 8.0f * s;
                b.sprite(iconHeartEmpty(), hx, hy, 9.0f * s, 8.0f * s);
                if (st.health >= 2 * i + 2) b.sprite(iconHeartFull(), hx, hy, 9.0f * s, 8.0f * s);
                else if (st.health == 2 * i + 1) b.sprite(iconHeartHalf(), hx, hy, 9.0f * s, 8.0f * s);
            }
        }
        // Luftblasen
        if (st.showAir) {
            const int n = static_cast<int>(std::ceil(std::max(0.0f, st.air) * 10.0f));
            const float hy = y0 - 12.0f * s - (st.creative ? 0.0f : 10.0f * s) - (!st.creative && st.armorPoints > 0 ? 10.0f * s : 0.0f);
            for (int i = 0; i < n; ++i)
                b.sprite(iconBubble(), x0 + total - (i + 1) * 8.0f * s - s, hy, 9.0f * s, 9.0f * s);
        }
        // Name des gewaehlten Gegenstands
        if (st.itemNameAlpha > 0.0f && !st.itemName.empty() && !st.inventoryOpen) {
            const float tw = textWidthPx(st.itemName) * s;
            b.text(std::floor((W - tw) * 0.5f), y0 - 26.0f * s, s, st.itemName,
                   glm::vec4(1, 1, 1, std::min(1.0f, st.itemNameAlpha)));
        }
    }

    if (st.messageAlpha > 0.0f && !st.message.empty())
        b.text(4.0f * s, 4.0f * s, s, st.message, glm::vec4(1, 1, 1, std::min(1.0f, st.messageAlpha)));

    if (st.dead) {
        b.rect(0, 0, W, H, glm::vec4(0.35f, 0.0f, 0.0f, 0.6f));
        const std::string t1 = "Du bist gestorben!", t2 = "Leertaste: Wiedergeburt";
        const float s1 = 3.0f * s, s2 = 1.0f * s;
        b.text(std::floor((W - textWidthPx(t1) * s1) * 0.5f), std::floor(H * 0.38f), s1, t1);
        b.text(std::floor((W - textWidthPx(t2) * s2 * 1.5f) * 0.5f), std::floor(H * 0.38f + 40.0f * s), s2 * 1.5f, t2);
    }
}

}  // namespace oc
