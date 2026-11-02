#pragma once
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
        default: return false;
    }
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
        case SH_STAIRS: {
            add(0, 0, 0, 1, 0.5f, 1);
            switch (stairFacing(b)) {
                case 0: add(0.5f, 0.5f, 0, 1, 1, 1); break;
                case 1: add(0, 0.5f, 0, 0.5f, 1, 1); break;
                case 2: add(0, 0.5f, 0.5f, 1, 1, 1); break;
                default: add(0, 0.5f, 0, 1, 1, 0.5f); break;
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
