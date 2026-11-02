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

float guiScale(int windowHeight) { return std::max(1.0f, std::floor(static_cast<float>(windowHeight) / 360.0f)); }

// ---------------------------------------------------------------------------------------------------------------
// Layout des Inventarfensters
// ---------------------------------------------------------------------------------------------------------------

InvLayout InvLayout::make(int w, int h, bool creative, int craftSize, bool furnace) {
    InvLayout l;
    l.furnace = furnace;
    l.s = guiScale(h);
    l.creative = creative;
    l.craftSize = craftSize;
    const float s = l.s, pad = 7.0f * s, title = 12.0f * s;
    const float width = 9.0f * 18.0f * s + 2.0f * pad;
    const int paletteRows = (creativeItemCount() + PALETTE_COLS - 1) / PALETTE_COLS;
    float height = pad + title;
    float craftOffset = 0.0f;
    if (creative) height += paletteRows * 18.0f * s + 8.0f * s;
    if (craftSize > 0) {
        craftOffset = height;
        height += craftSize * 18.0f * s + 8.0f * s;
    }
    const float storageOffset = height;
    height += 3.0f * 18.0f * s + 4.0f * s + 18.0f * s + pad;

    l.panelSize = glm::vec2(width, height);
    l.panelPos = glm::vec2(std::floor((w - width) * 0.5f), std::floor((h - height) * 0.5f));
    l.paletteTop = l.panelPos + glm::vec2(pad, pad + title);
    l.craftTop = l.panelPos + glm::vec2(pad + 18.0f * s, craftOffset);
    l.resultPos = l.panelPos + glm::vec2(pad + 6.0f * 18.0f * s, craftOffset + (craftSize - 1) * 9.0f * s);
    l.storageTop = l.panelPos + glm::vec2(pad, storageOffset);
    l.hotbarTop = l.storageTop + glm::vec2(0.0f, 3.0f * 18.0f * s + 4.0f * s);
    return l;
}

glm::vec2 InvLayout::slotPos(int index) const {
    if (index < Inventory::HOTBAR) return hotbarTop + glm::vec2(index * pitch(), 0.0f);
    int i = index - Inventory::HOTBAR;
    return storageTop + glm::vec2((i % 9) * pitch(), (i / 9) * pitch());
}

glm::vec2 InvLayout::palettePos(int index) const {
    return paletteTop + glm::vec2((index % PALETTE_COLS) * pitch(), (index / PALETTE_COLS) * pitch());
}

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
    for (int i = 0; i < creativeItemCount(); ++i)
        if (inRect(m, palettePos(i), pitch(), pitch())) return i;
    return -1;
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

bool InvLayout::inside(glm::vec2 m) const { return inRect(m, panelPos, panelSize.x, panelSize.y); }

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
    const InvLayout L = InvLayout::make(w, h, st.paletteOpen, st.craftSize, st.furnaceOpen);
    const float s = L.s, pt = L.pitch();
    b.rect(0, 0, static_cast<float>(w), static_cast<float>(h), glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));

    // Fenster
    b.rect(L.panelPos.x - 2 * s, L.panelPos.y - 2 * s, L.panelPos.x + L.panelSize.x + 2 * s,
           L.panelPos.y + L.panelSize.y + 2 * s, glm::vec4(0.05f, 0.05f, 0.07f, 0.95f));
    b.rect(L.panelPos.x, L.panelPos.y, L.panelPos.x + L.panelSize.x, L.panelPos.y + L.panelSize.y,
           glm::vec4(0.30f, 0.31f, 0.35f, 0.97f));
    std::string title = st.windowTitle;
    if (title.empty())
        title = st.paletteOpen ? "Kreativ-Inventar" : (st.furnaceOpen ? "Ofen" : (st.craftSize == 3 ? "Werkbank" : "Inventar"));
    b.text(L.panelPos.x + 7 * s, L.panelPos.y + 6 * s, s, title, glm::vec4(1.0f));

    const glm::vec4 slotBg(0.12f, 0.12f, 0.15f, 1.0f);
    const glm::vec4 hoverFx(1, 1, 1, 0.3f);
    const int hoverSlot = L.hitSlot(st.mouse), hoverPal = L.hitPalette(st.mouse);
    const int hoverCraft = L.hitCraft(st.mouse);
    const bool hoverResult = L.hitResult(st.mouse);
    auto slotRect = [&](glm::vec2 p, bool hover) {
        b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, slotBg);
        if (hover) b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, hoverFx);
    };

    // Auswahlfeld (Kreativ)
    if (st.paletteOpen) {
        for (int i = 0; i < creativeItemCount(); ++i) {
            glm::vec2 p = L.palettePos(i);
            slotRect(p, i == hoverPal);
            b.itemIcon(creativeItem(i), p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
        }
        float sepY = L.storageTop.y - 5.0f * s;
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
    if (hoverPal >= 0) hoverItem = creativeItem(hoverPal);
    else if (hoverSlot >= 0 && inv && !inv->slots[hoverSlot].empty()) hoverItem = inv->slots[hoverSlot].id;
    else if (hoverCraft >= 0 && !st.craftCells[hoverCraft].empty()) hoverItem = st.craftCells[hoverCraft].id;
    else if (hoverFurnace == 0 && !st.furnaceIn.empty()) hoverItem = st.furnaceIn.id;
    else if (hoverFurnace == 1 && !st.furnaceFuel.empty()) hoverItem = st.furnaceFuel.id;
    else if (hoverFurnace == 2 && !st.furnaceOut.empty()) hoverItem = st.furnaceOut.id;
    else if (hoverResult && !st.craftResult.empty()) hoverItem = st.craftResult.id;
    if (hoverItem != ITEM_NONE && st.cursor.empty()) {
        std::string name = itemInfo(hoverItem).name;
        float tw = textWidthPx(name) * s;
        float tx = st.mouse.x + 10 * s, ty = st.mouse.y - 12 * s;
        b.rect(tx - 3 * s, ty - 3 * s, tx + tw + 3 * s, ty + 8 * s + 3 * s, glm::vec4(0.08f, 0.0f, 0.12f, 0.92f));
        b.text(tx, ty, s, name);
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

static void drawMenu(HudBatch& b, const MenuLayout& m, int hover, int w, int h) {
    const float s = m.scale;
    b.rect(0, 0, static_cast<float>(w), static_cast<float>(h), glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
    const float ts = 2.0f * s;
    const float tw = textWidthPx(m.title) * ts;
    b.text(std::floor((w - tw) * 0.5f), m.titlePos.y, ts, m.title);
    for (size_t i = 0; i < m.widgets.size(); ++i) drawWidget(b, m.widgets[i], static_cast<int>(i) == hover, s);
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
        drawMenu(b, st.menu, st.menuHover, w, h);
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
        b.rect(x0 - 2 * s, y0 - 2 * s, x0 + total + 2 * s, y0 + slot + 2 * s, glm::vec4(0, 0, 0, 0.55f));
        for (int i = 0; i < Inventory::HOTBAR; ++i) {
            const float sx = x0 + i * slot;
            const bool sel = i == st.selectedSlot;
            if (sel) b.rect(sx - 2 * s, y0 - 2 * s, sx + slot + 2 * s, y0 + slot + 2 * s, glm::vec4(1, 1, 1, 0.95f));
            b.rect(sx + s, y0 + s, sx + slot - s, y0 + slot - s,
                   sel ? glm::vec4(0.25f, 0.25f, 0.25f, 0.9f) : glm::vec4(0.12f, 0.12f, 0.12f, 0.75f));
        }
        for (int i = 0; i < Inventory::HOTBAR; ++i) {
            const ItemStack& stack = st.inventory->slots[i];
            if (stack.empty()) continue;
            b.itemIcon(stack.id, x0 + i * slot + slot * 0.5f, y0 + slot * 0.5f, 7.0f * s);
            if (!st.creative) drawSlotCount(b, stack, x0 + i * slot, y0, slot, s);
            drawDurability(b, stack, x0 + i * slot, y0, slot, s);
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
            const float hy = y0 - 12.0f * s - (st.creative ? 0.0f : 10.0f * s);
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
