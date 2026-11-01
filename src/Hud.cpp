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
    if (!info.isBlock) {
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

InvLayout InvLayout::make(int w, int h, bool creative) {
    InvLayout l;
    l.s = guiScale(h);
    l.creative = creative;
    const float s = l.s, pad = 7.0f * s, title = 12.0f * s;
    const float width = 9.0f * 18.0f * s + 2.0f * pad;
    const int paletteRows = (creativeItemCount() + PALETTE_COLS - 1) / PALETTE_COLS;
    float height = pad + title;
    if (creative) height += paletteRows * 18.0f * s + 8.0f * s;
    const float storageOffset = height;
    height += 3.0f * 18.0f * s + 4.0f * s + 18.0f * s + pad;

    l.panelSize = glm::vec2(width, height);
    l.panelPos = glm::vec2(std::floor((w - width) * 0.5f), std::floor((h - height) * 0.5f));
    l.paletteTop = l.panelPos + glm::vec2(pad, pad + title);
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

bool InvLayout::inside(glm::vec2 m) const { return inRect(m, panelPos, panelSize.x, panelSize.y); }

// ---------------------------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------------------------

static void drawSlotCount(HudBatch& b, const ItemStack& st, float x, float y, float pitch, float s) {
    if (st.count <= 1) return;
    std::string n = std::to_string(st.count);
    float w = textWidthPx(n) * s;
    b.text(x + pitch - w - 1.5f * s, y + pitch - 8.0f * s - 0.5f * s, s, n);
}

static void drawInventoryScreen(HudBatch& b, const HudState& st, int w, int h) {
    const InvLayout L = InvLayout::make(w, h, st.creative);
    const float s = L.s, pt = L.pitch();
    b.rect(0, 0, static_cast<float>(w), static_cast<float>(h), glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));

    // Fenster
    b.rect(L.panelPos.x - 2 * s, L.panelPos.y - 2 * s, L.panelPos.x + L.panelSize.x + 2 * s,
           L.panelPos.y + L.panelSize.y + 2 * s, glm::vec4(0.05f, 0.05f, 0.07f, 0.95f));
    b.rect(L.panelPos.x, L.panelPos.y, L.panelPos.x + L.panelSize.x, L.panelPos.y + L.panelSize.y,
           glm::vec4(0.30f, 0.31f, 0.35f, 0.97f));
    b.text(L.panelPos.x + 7 * s, L.panelPos.y + 6 * s, s, st.creative ? "Kreativ-Inventar" : "Inventar",
           glm::vec4(1.0f));

    const glm::vec4 slotBg(0.12f, 0.12f, 0.15f, 1.0f);
    const int hoverSlot = L.hitSlot(st.mouse), hoverPal = L.hitPalette(st.mouse);

    // Auswahlfeld (Kreativ)
    std::vector<std::pair<glm::vec2, ItemId>> icons;
    if (st.creative) {
        for (int i = 0; i < creativeItemCount(); ++i) {
            glm::vec2 p = L.palettePos(i);
            b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, slotBg);
            if (i == hoverPal) b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, glm::vec4(1, 1, 1, 0.3f));
            b.itemIcon(creativeItem(i), p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
        }
        float sepY = L.storageTop.y - 5.0f * s;
        b.rect(L.panelPos.x + 7 * s, sepY, L.panelPos.x + L.panelSize.x - 7 * s, sepY + s, glm::vec4(0, 0, 0, 0.5f));
    }

    // Rucksack + Hotbar
    const Inventory* inv = st.inventory;
    for (int i = 0; i < Inventory::SIZE; ++i) {
        glm::vec2 p = L.slotPos(i);
        b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, slotBg);
        if (i < Inventory::HOTBAR && i == st.selectedSlot)
            b.rect(p.x, p.y, p.x + pt, p.y + s, glm::vec4(1, 1, 1, 0.9f));  // Markierung der aktiven Hotbar
        if (i == hoverSlot) b.rect(p.x + s, p.y + s, p.x + pt - s, p.y + pt - s, glm::vec4(1, 1, 1, 0.3f));
        if (inv && !inv->slots[i].empty()) b.itemIcon(inv->slots[i].id, p.x + pt * 0.5f, p.y + pt * 0.5f, 6.3f * s);
    }
    if (inv && !st.creative) {
        for (int i = 0; i < Inventory::SIZE; ++i) {
            glm::vec2 p = L.slotPos(i);
            drawSlotCount(b, inv->slots[i], p.x, p.y, pt, s);
        }
    }

    // Tooltip
    ItemId hoverItem = ITEM_NONE;
    if (hoverPal >= 0) hoverItem = creativeItem(hoverPal);
    else if (hoverSlot >= 0 && inv && !inv->slots[hoverSlot].empty()) hoverItem = inv->slots[hoverSlot].id;
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
        if (!st.creative) drawSlotCount(b, st.cursor, st.mouse.x - pt * 0.5f, st.mouse.y - pt * 0.5f, pt, s);
    }
}

void buildHud(HudBatch& b, const HudState& st, int w, int h) {
    b.clear();
    if (!st.show) return;
    const float W = static_cast<float>(w), H = static_cast<float>(h);
    const float s = guiScale(h);

    if (st.underwater) b.rect(0, 0, W, H, glm::vec4(0.08f, 0.25f, 0.55f, 0.35f));
    if (st.damageFlash > 0.0f) b.rect(0, 0, W, H, glm::vec4(0.85f, 0.05f, 0.05f, 0.4f * st.damageFlash));

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
