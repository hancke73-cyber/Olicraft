#include "Mesher.h"

#include <algorithm>
#include <cstdint>

#include <glm/glm.hpp>

#include "Atlas.h"
#include "BlockShape.h"
#include "Faces.h"
#include "Noise.h"

namespace oc {

namespace {

const float AO_CURVE[4] = {0.50f, 0.68f, 0.84f, 1.00f};

struct Ctx {
    const BlockId* p;
    const uint8_t* light;       // Blocklicht (Layout wie p) oder nullptr
    int16_t hm[PAD_W * PAD_W];  // hoechster lichtblockierender Block je Spalte (-1 = keiner)
    int maxY;                   // hoechster nicht-leerer Block im Chunk

    BlockId at(int x, int y, int z) const {
        if (y < 0) return BEDROCK;
        if (y >= CHUNK_H) return AIR;
        return p[padIndex(x + 1, y, z + 1)];
    }
    bool opaqueAt(int x, int y, int z) const { return isOpaque(at(x, y, z)); }
    float lightAt(int x, int y, int z) const {
        if (!light || y < 0 || y >= CHUNK_H) return 0.0f;
        return static_cast<float>(light[padIndex(x + 1, y, z + 1)]);
    }
    float skyAt(int x, int y, int z) const {
        if (y >= CHUNK_H) return 1.0f;
        if (y < 0) return 0.0f;
        return y > hm[(z + 1) * PAD_W + (x + 1)] ? 1.0f : 0.0f;
    }
};

void computeHeightmap(Ctx& c) {
    c.maxY = 0;
    for (int pz = 0; pz < PAD_W; ++pz) {
        for (int px = 0; px < PAD_W; ++px) {
            int16_t h = -1;
            for (int y = CHUNK_H - 1; y >= 0; --y) {
                BlockId b = c.p[padIndex(px, y, pz)];
                if (b == AIR) continue;
                if (px >= 1 && px <= CHUNK_W && pz >= 1 && pz <= CHUNK_W && y > c.maxY) c.maxY = y;
                if (blockInfo(b).blocksSky) {
                    h = static_cast<int16_t>(y);
                    break;
                }
            }
            c.hm[pz * PAD_W + px] = h;
        }
    }
    // maxY muss auch bei Spalten gelten, in denen nur nicht-lichtblockierende Bloecke (Wasser, Glas) liegen
    for (int pz = 1; pz <= CHUNK_W; ++pz)
        for (int px = 1; px <= CHUNK_W; ++px)
            for (int y = CHUNK_H - 1; y > c.maxY; --y)
                if (c.p[padIndex(px, y, pz)] != AIR) {
                    c.maxY = y;
                    break;
                }
}

bool shouldDrawFace(BlockId b, BlockId nb) {
    const BlockInfo& bi = blockInfo(b);
    const BlockInfo& ni = blockInfo(nb);
    if (ni.opaque) return false;
    if (bi.liquid) return true;  // Hoehenvergleiche folgen in liquidSpan()
    if (bi.cullSame && nb == b) return false;
    return true;
}

// Obere Kante der Fluessigkeit in der Zelle (Wasser darueber = volle Hoehe)
float liquidTop(const Ctx& c, int x, int y, int z) {
    BlockId b = c.at(x, y, z);
    if (isLiquid(c.at(x, y + 1, z))) return 1.0f;
    return liquidHeight(b);
}

// Sichtbarer Hoehenbereich [lo, hi] der Flaeche f eines Wasserblocks, false = nichts zu zeichnen
bool liquidSpan(const Ctx& c, int x, int y, int z, int f, float& lo, float& hi) {
    const FaceDef& fd = faceDef(f);
    BlockId nb = c.at(x + fd.n[0], y + fd.n[1], z + fd.n[2]);
    hi = liquidTop(c, x, y, z);
    lo = 0.0f;
    if (f == 2) return !isLiquid(nb) && !isOpaque(nb);  // Oberflaeche
    if (f == 3) return !isLiquid(nb) && !isOpaque(nb);  // Unterseite ueber Luft
    if (isOpaque(nb)) return false;
    if (isLiquid(nb)) {
        lo = liquidTop(c, x + fd.n[0], y, z + fd.n[2]);
        return lo < hi - 1e-4f;
    }
    return true;
}

void emitFace(const Ctx& c, int x, int y, int z, int f, BlockId b, float lo, float hi, std::vector<Vertex>& out) {
    const FaceDef& fd = faceDef(f);
    const BlockInfo& bi = blockInfo(b);
    const bool liquid = bi.liquid;
    const Tile tile = f == 2 ? bi.top : (f == 3 ? bi.bottom : bi.side);
    const UVRect uv = tileUV(tile);

    const float topH = hi;

    const int fx = x + fd.n[0], fy = y + fd.n[1], fz = z + fd.n[2];

    Vertex v[4];
    float aoSum[4];
    for (int k = 0; k < 4; ++k) {
        const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
        float off[3];
        for (int a = 0; a < 3; ++a) off[a] = static_cast<float>((fd.n[a] > 0 ? 1 : 0) + i * fd.t1[a] + j * fd.t2[a]);

        float uu, vv;
        faceUV(f, i, j, uu, vv);

        float py = off[1];
        if (liquid) {
            if (f == 2) py = topH;                           // Oberflaeche
            else if (f != 3) {                               // Seitenflaeche: Bereich [lo, hi]
                py = off[1] > 0.5f ? topH : lo;
                vv = py;                                     // Textur laeuft mit der Hoehe
            }
        }

        v[k].x = static_cast<float>(x) + off[0];
        v[k].y = static_cast<float>(y) + py;
        v[k].z = static_cast<float>(z) + off[2];
        v[k].u = uv.u0 + (uv.u1 - uv.u0) * uu;
        v[k].v = uv.v0 + (uv.v1 - uv.v0) * vv;

        // Ambient Occlusion + Himmelslicht (geglaettet ueber die 4 Zellen vor dem Vertex)
        const int s1x = (i ? 1 : -1) * fd.t1[0], s1y = (i ? 1 : -1) * fd.t1[1], s1z = (i ? 1 : -1) * fd.t1[2];
        const int s2x = (j ? 1 : -1) * fd.t2[0], s2y = (j ? 1 : -1) * fd.t2[1], s2z = (j ? 1 : -1) * fd.t2[2];

        int ao = 3;
        if (!liquid) {
            bool o1 = c.opaqueAt(fx + s1x, fy + s1y, fz + s1z);
            bool o2 = c.opaqueAt(fx + s2x, fy + s2y, fz + s2z);
            bool oc = c.opaqueAt(fx + s1x + s2x, fy + s1y + s2y, fz + s1z + s2z);
            ao = (o1 && o2) ? 0 : 3 - (static_cast<int>(o1) + static_cast<int>(o2) + static_cast<int>(oc));
        }

        float skySum = c.skyAt(fx, fy, fz);
        float lightSum = c.lightAt(fx, fy, fz);
        int skyN = 1;
        const int cx4[3][3] = {{s1x, s1y, s1z}, {s2x, s2y, s2z}, {s1x + s2x, s1y + s2y, s1z + s2z}};
        for (auto& o : cx4) {
            if (!c.opaqueAt(fx + o[0], fy + o[1], fz + o[2])) {
                skySum += c.skyAt(fx + o[0], fy + o[1], fz + o[2]);
                lightSum += c.lightAt(fx + o[0], fy + o[1], fz + o[2]);
                ++skyN;
            }
        }
        v[k].sky = skySum / static_cast<float>(skyN);
        v[k].light = lightSum / static_cast<float>(skyN) / static_cast<float>(MAX_LIGHT);
        v[k].shade = fd.shade * AO_CURVE[ao];
        if (isLava(b) || bi.emissive) {  // leuchtende Bloecke: unabhaengig von Tageszeit und Schatten
            v[k].sky = 2.0f;
            v[k].shade = 1.0f;
            v[k].light = 1.0f;
        }
        aoSum[k] = static_cast<float>(ao);
    }

    // Diagonale so waehlen, dass der AO-Verlauf keine Artefakte zeigt
    int order[4] = {0, 1, 2, 3};
    if (aoSum[0] + aoSum[2] < aoSum[1] + aoSum[3]) {
        order[0] = 1;
        order[1] = 2;
        order[2] = 3;
        order[3] = 0;
    }
    for (int k = 0; k < 4; ++k) out.push_back(v[order[k]]);
}

// Fackel: duenner Stab (2 Pixel breit, 13 Pixel hoch inklusive Flamme), an Waenden etwas schraeg
void emitTorch(int x, int y, int z, BlockId b, std::vector<Vertex>& out) {
    const UVRect uv = tileUV(TILE_TORCH);
    int dx = 0, dy = 0, dz = 0;
    torchSupportOffset(b, dx, dy, dz);
    const float h = 1.0f / 16.0f, height = 13.0f / 16.0f;
    float cx = 0.5f, cz = 0.5f, base = 0.0f, tiltX = 0.0f, tiltZ = 0.0f;
    if (b != TORCH) {  // zur Wand verschoben und von ihr weg geneigt
        cx += static_cast<float>(dx) * 0.3f;
        cz += static_cast<float>(dz) * 0.3f;
        base = 0.2f;
        tiltX = -static_cast<float>(dx) * 0.22f;
        tiltZ = -static_cast<float>(dz) * 0.22f;
    }
    for (int f = 0; f < 6; ++f) {
        if (f == 3) continue;  // Unterseite sieht man nie
        const FaceDef& fd = faceDef(f);
        Vertex v[4];
        for (int k = 0; k < 4; ++k) {
            const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
            float p[3];
            for (int a = 0; a < 3; ++a) {
                const float s = static_cast<float>(fd.n[a]) + (i ? 1.0f : -1.0f) * static_cast<float>(fd.t1[a]) +
                                (j ? 1.0f : -1.0f) * static_cast<float>(fd.t2[a]);
                p[a] = a == 1 ? (s > 0 ? height : 0.0f) : h * s;
            }
            const float shear = p[1] / height;
            v[k].x = static_cast<float>(x) + cx + p[0] + tiltX * shear;
            v[k].y = static_cast<float>(y) + base + p[1];
            v[k].z = static_cast<float>(z) + cz + p[2] + tiltZ * shear;
            float uu, vv;
            faceUV(f, i, j, uu, vv);
            // Kachel: mittlere zwei Pixel in u; Seiten zeigen Stab und Flamme (Zeilen 0..13), die Oberseite die Flammenspitze
            const float tu = 7.0f / 16.0f + uu * 2.0f / 16.0f;
            const float tv = f == 2 ? 12.0f / 16.0f + vv * 1.0f / 16.0f : vv * height;
            v[k].u = uv.u0 + (uv.u1 - uv.u0) * tu;
            v[k].v = uv.v0 + (uv.v1 - uv.v0) * tv;
            v[k].shade = 1.0f;
            v[k].sky = 2.0f;  // leuchtet
            v[k].light = 1.0f;
        }
        for (int k = 0; k < 4; ++k) out.push_back(v[k]);
    }
}

// Pflanze: zwei gekreuzte Flaechen (beidseitig sichtbar), leicht zufaellig versetzt wie in Minecraft
void emitPlant(const Ctx& c, int x, int y, int z, BlockId b, std::vector<Vertex>& out) {
    const UVRect uv = tileUV(blockInfo(b).side);
    const uint32_t h = hash3(x, y, z, 0x91A47u);
    // Zuckerrohr waechst gerade nach oben: ohne Versatz, sonst sitzen die Stuecke nicht uebereinander
    const float jitter = (b == SUGAR_CANE) ? 0.0f : 0.22f;
    const float ox = (static_cast<float>(h & 15u) / 15.0f - 0.5f) * jitter;
    const float oz = (static_cast<float>((h >> 4) & 15u) / 15.0f - 0.5f) * jitter;
    const float height = (b == SUGAR_CANE) ? 1.0f : (b == TALL_GRASS ? 0.9f : 0.85f);
    const float sky = c.skyAt(x, y, z);
    const float light = c.lightAt(x, y, z) / static_cast<float>(MAX_LIGHT);
    const float a = 0.12f, bb = 0.88f;
    // Ebene A: (a,a) -> (bb,bb), Ebene B: (bb,a) -> (a,bb)
    const float starts[2][2] = {{a, a}, {bb, a}};
    const float ends[2][2] = {{bb, bb}, {a, bb}};
    for (int pl = 0; pl < 2; ++pl) {
        Vertex v[4];
        for (int k = 0; k < 4; ++k) {
            const bool end = k == 1 || k == 2;
            const bool top = k >= 2;
            const float* p = end ? ends[pl] : starts[pl];
            v[k].x = static_cast<float>(x) + p[0] + ox;
            v[k].y = static_cast<float>(y) + (top ? height : 0.0f);
            v[k].z = static_cast<float>(z) + p[1] + oz;
            v[k].u = uv.u0 + (uv.u1 - uv.u0) * (end ? 1.0f : 0.0f);
            v[k].v = uv.v0 + (uv.v1 - uv.v0) * (top ? 1.0f : 0.0f);
            v[k].shade = 0.92f;
            v[k].sky = sky;
            v[k].light = light;
        }
        for (int k = 0; k < 4; ++k) out.push_back(v[k]);
        const int back[4] = {1, 0, 3, 2};  // Rueckseite: umgekehrter Umlaufsinn
        for (int k = 0; k < 4; ++k) out.push_back(v[back[k]]);
    }
}

// Bauteile mit Teilform: Treppen, Platten, Zaeune, Tueren, Tore, Laternen, Acker ...
void emitShaped(const Ctx& c, int x, int y, int z, BlockId b, std::vector<Vertex>& out) {
    const BlockInfo& bi = blockInfo(b);
    Box boxes[MAX_BOXES];
    auto get = [&](int gx, int gy, int gz) { return c.at(gx, gy, gz); };
    const int n = blockBoxes(b, x, y, z, get, true, boxes);
    // Teilbloecke, die selbst Himmelslicht blockieren (Treppen, Platten), nehmen das Licht der Zelle darueber
    const float sky = bi.emissive ? 2.0f : std::max(c.skyAt(x, y, z), c.skyAt(x, y + 1, z));
    const float light = bi.emissive ? 1.0f : std::max(c.lightAt(x, y, z), c.lightAt(x, y + 1, z)) / static_cast<float>(MAX_LIGHT);
    for (int bx = 0; bx < n; ++bx) {
        const Box& bb = boxes[bx];
        const float lo[3] = {bb.x0, bb.y0, bb.z0}, hi[3] = {bb.x1, bb.y1, bb.z1};
        for (int f = 0; f < 6; ++f) {
            const FaceDef& fd = faceDef(f);
            // Flaeche auf der Zellgrenze, vom Nachbarn verdeckt?
            int na = 0;
            for (int a = 0; a < 3; ++a)
                if (fd.n[a] != 0) na = a;
            const bool onEdge = fd.n[na] > 0 ? hi[na] >= 1.0f - 1e-4f : lo[na] <= 1e-4f;
            if (onEdge && c.opaqueAt(x + fd.n[0], y + fd.n[1], z + fd.n[2])) continue;
            const Tile tile = f == 2 ? bi.top : (f == 3 ? bi.bottom : bi.side);
            const UVRect uv = tileUV(tile);
            Vertex v[4];
            for (int k = 0; k < 4; ++k) {
                const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
                float p[3];
                for (int a = 0; a < 3; ++a) {
                    if (fd.n[a] != 0) p[a] = fd.n[a] > 0 ? hi[a] : lo[a];
                    else if (fd.t1[a] != 0) p[a] = i ? hi[a] : lo[a];
                    else p[a] = j ? hi[a] : lo[a];
                }
                float uu, vv;
                switch (f) {
                    case 0: uu = 1.0f - p[2]; vv = p[1]; break;
                    case 1: uu = p[2]; vv = p[1]; break;
                    case 2: uu = p[0]; vv = p[2]; break;
                    case 3: uu = p[0]; vv = p[2]; break;
                    case 4: uu = p[0]; vv = p[1]; break;
                    default: uu = 1.0f - p[0]; vv = p[1]; break;
                }
                v[k].x = static_cast<float>(x) + p[0];
                v[k].y = static_cast<float>(y) + p[1];
                v[k].z = static_cast<float>(z) + p[2];
                v[k].u = uv.u0 + (uv.u1 - uv.u0) * uu;
                v[k].v = uv.v0 + (uv.v1 - uv.v0) * vv;
                v[k].shade = bi.emissive ? 1.0f : fd.shade;
                v[k].sky = sky;
                v[k].light = light;
            }
            for (int k = 0; k < 4; ++k) out.push_back(v[k]);
        }
    }
}

// Dachkeil: glatte Schraege. a: Lage entlang der Richtung der hohen Seite (0 niedrig, 1 hoch), s: seitlich, h: Hoehe.
void emitWedge(const Ctx& c, int x, int y, int z, BlockId b, std::vector<Vertex>& out) {
    const BlockInfo& bi = blockInfo(b);
    const int f = wedgeFacing(b);
    const float sky = std::max(c.skyAt(x, y, z), c.skyAt(x, y + 1, z));
    const float light = std::max(c.lightAt(x, y, z), c.lightAt(x, y + 1, z)) / static_cast<float>(MAX_LIGHT);
    auto P = [&](float a, float s, float h) -> glm::vec3 {
        switch (f) {
            case 0: return {a, h, s};
            case 1: return {1.0f - a, h, s};
            case 2: return {s, h, a};
            default: return {s, h, 1.0f - a};
        }
    };
    static const int DX[4] = {1, -1, 0, 0}, DZ[4] = {0, 0, 1, -1};
    const glm::vec3 dir(static_cast<float>(DX[f]), 0.0f, static_cast<float>(DZ[f]));
    const glm::vec3 side(static_cast<float>(f >= 2 ? 1 : 0), 0.0f, static_cast<float>(f >= 2 ? 0 : 1));  // Richtung s -> 1
    // Eine Flaeche aus vier Punkten (a, s, h) mit Texturkoordinaten; die Ecken werden so geordnet, dass sie von aussen gegen den Uhrzeigersinn laufen
    auto quad = [&](const float pts[4][3], const float uv[4][2], const glm::vec3& outward, float shade, const UVRect& r) {
        glm::vec3 p[4];
        for (int k = 0; k < 4; ++k) p[k] = P(pts[k][0], pts[k][1], pts[k][2]);
        int order[4] = {0, 1, 2, 3};
        if (glm::dot(glm::cross(p[1] - p[0], p[3] - p[0]), outward) < 0.0f) {
            order[1] = 3;
            order[3] = 1;
        }
        for (int k = 0; k < 4; ++k) {
            const int i = order[k];
            Vertex v;
            v.x = static_cast<float>(x) + p[i].x;
            v.y = static_cast<float>(y) + p[i].y;
            v.z = static_cast<float>(z) + p[i].z;
            v.u = r.u0 + (r.u1 - r.u0) * uv[i][0];
            v.v = r.v0 + (r.v1 - r.v0) * uv[i][1];
            v.shade = shade;
            v.sky = sky;
            v.light = light;
            out.push_back(v);
        }
    };
    const UVRect top = tileUV(bi.top), sideTile = tileUV(bi.side);
    // unten
    if (!c.opaqueAt(x, y - 1, z)) {
        const float pts[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        quad(pts, uv, glm::vec3(0, -1, 0), faceDef(3).shade, tileUV(bi.bottom));
    }
    // hohe Rueckseite
    if (!c.opaqueAt(x + DX[f], y, z + DZ[f])) {
        const float pts[4][3] = {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}};
        const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        quad(pts, uv, dir, faceDef(f < 2 ? 0 : 4).shade, sideTile);
    }
    // Schraege
    {
        const float pts[4][3] = {{0, 0, 0}, {0, 1, 0}, {1, 1, 1}, {1, 0, 1}};
        const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        quad(pts, uv, glm::normalize(glm::vec3(0, 1, 0) - dir), faceDef(2).shade * 0.94f, top);
    }
    // zwei dreieckige Seiten (viertes Eck doppelt)
    for (int sd = 0; sd < 2; ++sd) {
        const int nx = sd == 0 ? -static_cast<int>(side.x) : static_cast<int>(side.x), nz = sd == 0 ? -static_cast<int>(side.z) : static_cast<int>(side.z);
        if (c.opaqueAt(x + nx, y, z + nz)) continue;
        const float s = static_cast<float>(sd);
        const float pts[4][3] = {{0, s, 0}, {1, s, 0}, {1, s, 1}, {1, s, 1}};
        const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {1, 1}};
        quad(pts, uv, glm::vec3(static_cast<float>(nx), 0.0f, static_cast<float>(nz)), faceDef(f < 2 ? 4 : 0).shade, sideTile);
    }
}

}  // namespace

void buildChunkMesh(const BlockId* padded, const uint8_t* light, MeshOutput& out) {
    out.clear();
    Ctx c;
    c.p = padded;
    c.light = light;
    computeHeightmap(c);

    for (int y = 0; y <= c.maxY; ++y) {
        for (int z = 0; z < CHUNK_W; ++z) {
            for (int x = 0; x < CHUNK_W; ++x) {
                BlockId b = padded[padIndex(x + 1, y, z + 1)];
                if (b == AIR) continue;
                if (isTorch(b)) {
                    emitTorch(x, y, z, b, out.opaque);
                    continue;
                }
                if (isPlant(b)) {
                    emitPlant(c, x, y, z, b, out.opaque);
                    continue;
                }
                if (blockInfo(b).shape == SH_WEDGE) {
                    emitWedge(c, x, y, z, b, out.opaque);
                    continue;
                }
                if (blockInfo(b).shape != SH_FULL) {
                    emitShaped(c, x, y, z, b, out.opaque);
                    continue;
                }
                const bool liquid = blockInfo(b).liquid;
                std::vector<Vertex>& dst = (liquid && !isLava(b)) ? out.water : out.opaque;
                for (int f = 0; f < 6; ++f) {
                    const FaceDef& fd = faceDef(f);
                    BlockId nb = c.at(x + fd.n[0], y + fd.n[1], z + fd.n[2]);
                    if (!shouldDrawFace(b, nb)) continue;
                    float lo = 0.0f, hi = 1.0f;
                    if (liquid && !liquidSpan(c, x, y, z, f, lo, hi)) continue;
                    emitFace(c, x, y, z, f, b, lo, hi, dst);
                }
            }
        }
    }
}

}  // namespace oc
