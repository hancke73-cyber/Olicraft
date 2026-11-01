#pragma once

namespace oc {

// Gemeinsame Flaechendefinition fuer Chunk-Mesher und Entity-Modelle.
// Reihenfolge der Flaechen: +X, -X, +Y (oben), -Y (unten), +Z, -Z.
// t1 x t2 == n, damit die Eckenfolge (0,0),(1,0),(1,1),(0,1) von aussen gegen den Uhrzeigersinn laeuft.
struct FaceDef {
    int n[3];
    int t1[3];
    int t2[3];
    float shade;
};

inline const FaceDef& faceDef(int f) {
    static const FaceDef FACES[6] = {
        {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, 0.66f},   // +X
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, 0.66f},  // -X
        {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}, 1.00f},   // +Y
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, 0.50f},  // -Y
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 0.82f},   // +Z
        {{0, 0, -1}, {0, 1, 0}, {1, 0, 0}, 0.82f},  // -Z
    };
    return FACES[f];
}

constexpr int FACE_CORNER_I[4] = {0, 1, 1, 0};
constexpr int FACE_CORNER_J[4] = {0, 0, 1, 1};

// Texturkoordinaten (0..1) der Ecke (i, j) der Flaeche f; v zeigt bei Seitenflaechen nach oben.
inline void faceUV(int f, int i, int j, float& uu, float& vv) {
    switch (f) {
        case 0: uu = 1.0f - j; vv = static_cast<float>(i); break;
        case 1: uu = static_cast<float>(i); vv = static_cast<float>(j); break;
        case 2: uu = static_cast<float>(j); vv = static_cast<float>(i); break;
        case 3: uu = static_cast<float>(i); vv = static_cast<float>(j); break;
        case 4: uu = static_cast<float>(i); vv = static_cast<float>(j); break;
        default: uu = 1.0f - j; vv = static_cast<float>(i); break;
    }
}

}  // namespace oc
