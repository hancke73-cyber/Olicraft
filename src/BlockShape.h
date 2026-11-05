#pragma once
#include <algorithm>

#include "Block.h"

namespace oc {

// Quader innerhalb einer Blockzelle (0..1). Einige Formen reichen fuer die Kollision ueber die Zelle hinaus (Zaeune 1.5).
struct Box {
    float x0, y0, z0, x1, y1, z1;
};
constexpr int MAX_BOXES = 12;

inline bool isFullSolidBlock(BlockId b) {
    const BlockInfo& bi = blockInfo(b);
    return bi.shape == SH_FULL && bi.solid && bi.opaque;
}

// Verbindet sich ein Zaun/eine Mauer/eine Scheibe (self) mit dem Nachbarn?
inline bool shapeConnects(BlockId self, BlockId other) {
    switch (blockInfo(self).shape) {
        case SH_FENCE: return isFence(other) || isGate(other) || isFullSolidBlock(other);
        case SH_WALL: return isWall(other) || isGate(other) || isFullSolidBlock(other);
        case SH_PANE: return isPane(other) || isFullSolidBlock(other) || other == GLASS;
        case SH_PIPE: return isSteamPart(other);
        case SH_SHAFT: return isMechPart(other);
        default: return false;
    }
}

// Treppenecken werden wie bei Minecraft aus den Nachbarn abgeleitet (nichts davon steht im Block):
// Steht vor der Treppe (in Richtung der hohen Seite) eine gleichartige Treppe quer dazu, ist es eine Aussenecke,
// steht hinter ihr eine quer dazu, eine Innenecke. corner = Richtung der Nachbarstufe, -1 gerade.
template <class G>
int stairCorner(BlockId b, int x, int y, int z, const G& get, bool& inner) {
    static const int DX[4] = {1, -1, 0, 0}, DZ[4] = {0, 0, 1, -1};
    inner = false;
    const int f = stairFacing(b);
    const bool top = stairIsTop(b);
    auto same = [&](BlockId n) { return isStairs(n) && stairIsTop(n) == top; };
    auto canTake = [&](int dir) {
        const BlockId n = get(x + DX[dir], y, z + DZ[dir]);
        return !isStairs(n) || stairFacing(n) != f || stairIsTop(n) != top;
    };
    const BlockId front = get(x + DX[f], y, z + DZ[f]);
    if (same(front)) {
        const int g = stairFacing(front);
        if ((g >> 1) != (f >> 1) && canTake(g ^ 1)) return g;
    }
    const BlockId back = get(x - DX[f], y, z - DZ[f]);
    if (same(back)) {
        const int g = stairFacing(back);
        if ((g >> 1) != (f >> 1) && canTake(g)) {
            inner = true;
            return g;
        }
    }
    return -1;
}

// Boxen des Blocks b an (x, y, z). get(x, y, z) liefert Nachbarbloecke (fuer verbindende Formen).
// visual = false: Kollision (Zaeune 1.5 hoch, offene Tueren behalten ihre Platte), true: Darstellung.
// Rueckgabe: Anzahl Boxen (0 = keine, z. B. Laterne bei der Kollision).
template <class G>
int blockBoxes(BlockId b, int x, int y, int z, const G& get, bool visual, Box* out) {
    const BlockInfo& bi = blockInfo(b);
    int n = 0;
    auto add = [&](float x0, float y0, float z0, float x1, float y1, float z1) { out[n++] = Box{x0, y0, z0, x1, y1, z1}; };
    constexpr float S = 1.0f / 16.0f;
    switch (bi.shape) {
        case SH_FULL:
            if (bi.solid) add(0, 0, 0, 1, 1, 1);
            return n;
        case SH_SLAB:
            if (slabIsTop(b)) add(0, 0.5f, 0, 1, 1, 1);
            else add(0, 0, 0, 1, 0.5f, 1);
            return n;
        case SH_WEDGE: {
            // Schraege als vier Stufen von je 0,25 (Kollision und Strahl); gezeichnet wird die glatte Flaeche
            const int f = wedgeFacing(b);
            for (int i = 0; i < 4; ++i) {
                const float lo = f == 0 || f == 2 ? i * 0.25f : 1.0f - (i + 1) * 0.25f, hi = lo + 0.25f, hgt = (i + 1) * 0.25f;
                if (f <= 1) add(lo, 0, 0, hi, hgt, 1);
                else add(0, 0, lo, 1, hgt, hi);
            }
            return n;
        }
        case SH_STAIRS: {
            const bool top = stairIsTop(b);
            const int f = stairFacing(b);
            if (top) add(0, 0.5f, 0, 1, 1, 1);  // umgedreht: Platte oben, Stufe darunter
            else add(0, 0, 0, 1, 0.5f, 1);
            const float y0 = top ? 0.0f : 0.5f, y1 = top ? 0.5f : 1.0f;
            // Hälften der Zelle in Richtung d: {x0, z0, x1, z1}
            auto half = [](int d, float r[4]) {
                r[0] = 0; r[1] = 0; r[2] = 1; r[3] = 1;
                switch (d) {
                    case 0: r[0] = 0.5f; break;
                    case 1: r[2] = 0.5f; break;
                    case 2: r[1] = 0.5f; break;
                    default: r[3] = 0.5f; break;
                }
            };
            bool inner = false;
            const int g = stairCorner(b, x, y, z, get, inner);
            float hf[4];
            half(f, hf);
            if (g < 0) {  // gerade: die ganze hintere Haelfte
                add(hf[0], y0, hf[1], hf[2], y1, hf[3]);
            } else if (!inner) {  // Aussenecke: nur das Viertel zur Seite der Nachbarstufe
                float hg[4];
                half(g, hg);
                add(std::max(hf[0], hg[0]), y0, std::max(hf[1], hg[1]), std::min(hf[2], hg[2]), y1, std::min(hf[3], hg[3]));
            } else {  // Innenecke: hintere Haelfte plus das Viertel vorn zur Seite der Nachbarstufe
                float hg[4], ho[4];
                half(g, hg);
                half(f ^ 1, ho);
                add(hf[0], y0, hf[1], hf[2], y1, hf[3]);
                add(std::max(ho[0], hg[0]), y0, std::max(ho[1], hg[1]), std::min(ho[2], hg[2]), y1, std::min(ho[3], hg[3]));
            }
            return n;
        }
        case SH_FARMLAND:
            add(0, 0, 0, 1, 15 * S, 1);
            return n;
        case SH_FENCE: {
            const bool c[4] = {shapeConnects(b, get(x + 1, y, z)), shapeConnects(b, get(x - 1, y, z)), shapeConnects(b, get(x, y, z + 1)),
                               shapeConnects(b, get(x, y, z - 1))};
            const float top = visual ? 1.0f : 1.5f;
            add(6 * S, 0, 6 * S, 10 * S, top, 10 * S);
            for (int d = 0; d < 4; ++d) {
                if (!c[d]) continue;
                const float a0 = d == 0 || d == 2 ? 10 * S : 0.0f, a1 = d == 0 || d == 2 ? 1.0f : 6 * S;
                float lo[2] = {6 * S, 12 * S}, hi[2] = {9 * S, 15 * S};
                for (int r = 0; r < (visual ? 2 : 1); ++r) {
                    const float yl = visual ? lo[r] : 0.0f, yh = visual ? hi[r] : 1.5f;
                    if (d < 2) add(a0, yl, 7 * S, a1, yh, 9 * S);
                    else add(7 * S, yl, a0, 9 * S, yh, a1);
                }
            }
            return n;
        }
        case SH_WALL: {
            const bool c[4] = {shapeConnects(b, get(x + 1, y, z)), shapeConnects(b, get(x - 1, y, z)), shapeConnects(b, get(x, y, z + 1)),
                               shapeConnects(b, get(x, y, z - 1))};
            add(4 * S, 0, 4 * S, 12 * S, visual ? 1.0f : 1.5f, 12 * S);
            for (int d = 0; d < 4; ++d) {
                if (!c[d]) continue;
                const float a0 = d == 0 || d == 2 ? 12 * S : 0.0f, a1 = d == 0 || d == 2 ? 1.0f : 4 * S;
                const float yh = visual ? 14 * S : 1.5f;
                if (d < 2) add(a0, 0, 5 * S, a1, yh, 11 * S);
                else add(5 * S, 0, a0, 11 * S, yh, a1);
            }
            return n;
        }
        case SH_PANE: {
            const bool c[4] = {shapeConnects(b, get(x + 1, y, z)), shapeConnects(b, get(x - 1, y, z)), shapeConnects(b, get(x, y, z + 1)),
                               shapeConnects(b, get(x, y, z - 1))};
            const bool any = c[0] || c[1] || c[2] || c[3];
            add(7 * S, 0, 7 * S, 9 * S, 1, 9 * S);
            if (!any) {  // freistehend: kleines Kreuz
                add(0, 0, 7 * S, 1, 1, 9 * S);
                add(7 * S, 0, 0, 9 * S, 1, 1);
                return n;
            }
            for (int d = 0; d < 4; ++d) {
                if (!c[d]) continue;
                const float a0 = d == 0 || d == 2 ? 9 * S : 0.0f, a1 = d == 0 || d == 2 ? 1.0f : 7 * S;
                if (d < 2) add(a0, 0, 7 * S, a1, 1, 9 * S);
                else add(7 * S, 0, a0, 9 * S, 1, a1);
            }
            return n;
        }
        case SH_DOOR: {
            const bool open = doorOpen(b);
            const int f = doorFacing(b);
            if (!open) {
                switch (f) {
                    case 0: add(13 * S, 0, 0, 1, 1, 1); break;
                    case 1: add(0, 0, 0, 3 * S, 1, 1); break;
                    case 2: add(0, 0, 13 * S, 1, 1, 1); break;
                    default: add(0, 0, 0, 1, 1, 3 * S); break;
                }
            } else {  // um das Scharnier gedreht: Platte an der Seite (-Z bzw. -X)
                if (f < 2) add(0, 0, 0, 1, 1, 3 * S);
                else add(0, 0, 0, 3 * S, 1, 1);
            }
            return n;
        }
        case SH_GATE: {
            const int axis = gateAxis(b);
            if (!gateOpen(b)) {
                const float top = visual ? 1.0f : 1.5f;
                if (axis == 0) add(0, 0, 6 * S, 1, top, 10 * S);
                else add(6 * S, 0, 0, 10 * S, top, 1);
            } else {
                const float top = visual ? 1.0f : 1.5f;
                if (axis == 0) {
                    add(0, 0, 6 * S, 2 * S, top, 10 * S);
                    add(14 * S, 0, 6 * S, 1, top, 10 * S);
                } else {
                    add(6 * S, 0, 0, 10 * S, top, 2 * S);
                    add(6 * S, 0, 14 * S, 10 * S, top, 1);
                }
            }
            return n;
        }
        case SH_LADDER: {
            if (!visual) return 0;
            switch (ladderFacing(b)) {
                case 0: add(15 * S, 0, 0, 1, 1, 1); break;
                case 1: add(0, 0, 0, 1 * S, 1, 1); break;
                case 2: add(0, 0, 15 * S, 1, 1, 1); break;
                default: add(0, 0, 0, 1, 1, 1 * S); break;
            }
            return n;
        }
        case SH_BED:
            add(0, 0, 0, 1, 9 * S, 1);
            return n;
        case SH_PIPE:
        case SH_SHAFT: {
            // Mittelstueck und je ein Arm zu jedem verbundenen Nachbarn (alle sechs Richtungen)
            const bool shaft = bi.shape == SH_SHAFT;
            const float lo = (shaft ? 6 : 5) * S, hi = (shaft ? 10 : 11) * S;
            const bool c[6] = {shapeConnects(b, get(x + 1, y, z)), shapeConnects(b, get(x - 1, y, z)), shapeConnects(b, get(x, y + 1, z)),
                               shapeConnects(b, get(x, y - 1, z)), shapeConnects(b, get(x, y, z + 1)), shapeConnects(b, get(x, y, z - 1))};
            add(lo, lo, lo, hi, hi, hi);
            if (c[0]) add(hi, lo, lo, 1, hi, hi);
            if (c[1]) add(0, lo, lo, lo, hi, hi);
            if (c[2]) add(lo, hi, lo, hi, 1, hi);
            if (c[3]) add(lo, 0, lo, hi, lo, hi);
            if (c[4]) add(lo, lo, hi, hi, hi, 1);
            if (c[5]) add(lo, lo, 0, hi, hi, lo);
            return n;
        }
        case SH_DRILL: {
            // Gehaeuse und davor der Bohrkopf (schmaler); f: Richtung des Bohrers
            const int f = drillFacing(b);
            const float body = 11 * S, a = 6 * S, bb = 10 * S;
            switch (f) {
                case 0: add(0, 0, 0, body, 1, 1); add(body, a, a, 1, bb, bb); break;
                case 1: add(1 - body, 0, 0, 1, 1, 1); add(0, a, a, 1 - body, bb, bb); break;
                case 2: add(0, 0, 0, 1, 1, body); add(a, a, body, bb, bb, 1); break;
                case 3: add(0, 0, 1 - body, 1, 1, 1); add(a, a, 0, bb, bb, 1 - body); break;
                case 4: add(0, 0, 0, 1, body, 1); add(a, body, a, bb, 1, bb); break;
                default: add(0, 1 - body, 0, 1, 1, 1); add(a, 0, a, bb, 1 - body, bb); break;
            }
            return n;
        }
        case SH_CONVEYOR:
            add(0, 0, 0, 1, 4 * S, 1);
            return n;
        case SH_LIFT:
            if (!visual) return 0;  // der Lift hat keine Kollision, er traegt
            add(0, 0, 0, 3 * S, 1, 3 * S);
            add(13 * S, 0, 0, 1, 1, 3 * S);
            add(0, 0, 13 * S, 3 * S, 1, 1);
            add(13 * S, 0, 13 * S, 1, 1, 1);
            return n;
        case SH_SIGN: {
            if (!visual) return 0;  // Schilder haben keine Kollision, sind aber anwaehlbar
            const int f = signFacing(b);
            const bool alongX = f <= 1;  // Brett liegt quer zur Richtung der Vorderseite
            const float t0 = 7 * S, t1 = 9 * S;
            auto board = [&](float c0, float c1, float y0, float y1) {
                if (alongX) add(c0, y0, 1 * S, c1, y1, 15 * S);
                else add(1 * S, y0, c0, 15 * S, y1, c1);
            };
            if (!signIsWall(b)) {  // Pfosten und Brett darueber
                add(7 * S, 0, 7 * S, 9 * S, 8 * S, 9 * S);
                board(t0, t1, 8 * S, 16 * S);
            } else {  // Brett an der Wand: der Traeger liegt auf der Seite gegenueber der Vorderseite
                const float c0 = (f == 0 || f == 2) ? 0.0f : 14 * S, c1 = c0 + 2 * S;
                board(c0, c1, 4 * S, 12 * S);
            }
            return n;
        }
        case SH_LANTERN:
            if (!visual) return 0;
            if (b == LANTERN) {
                add(5 * S, 0, 5 * S, 11 * S, 9 * S, 11 * S);
                add(6 * S, 9 * S, 6 * S, 10 * S, 11 * S, 10 * S);
            } else {
                add(5 * S, 5 * S, 5 * S, 11 * S, 14 * S, 11 * S);
                add(7.5f * S, 14 * S, 7.5f * S, 8.5f * S, 1, 8.5f * S);
            }
            return n;
    }
    return n;
}

}  // namespace oc
