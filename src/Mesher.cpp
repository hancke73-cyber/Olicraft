#include "Mesher.h"

#include <cstdint>

#include "Atlas.h"
#include "Faces.h"

namespace oc {

namespace {

const float AO_CURVE[4] = {0.50f, 0.68f, 0.84f, 1.00f};

struct Ctx {
    const BlockId* p;
    int16_t hm[PAD_W * PAD_W];  // hoechster lichtblockierender Block je Spalte (-1 = keiner)
    int maxY;                   // hoechster nicht-leerer Block im Chunk

    BlockId at(int x, int y, int z) const {
        if (y < 0) return BEDROCK;
        if (y >= CHUNK_H) return AIR;
        return p[padIndex(x + 1, y, z + 1)];
    }
    bool opaqueAt(int x, int y, int z) const { return isOpaque(at(x, y, z)); }
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
        int skyN = 1;
        const int cx4[3][3] = {{s1x, s1y, s1z}, {s2x, s2y, s2z}, {s1x + s2x, s1y + s2y, s1z + s2z}};
        for (auto& o : cx4) {
            if (!c.opaqueAt(fx + o[0], fy + o[1], fz + o[2])) {
                skySum += c.skyAt(fx + o[0], fy + o[1], fz + o[2]);
                ++skyN;
            }
        }
        v[k].sky = skySum / static_cast<float>(skyN);
        v[k].shade = fd.shade * AO_CURVE[ao];
        if (isLava(b) || bi.emissive) {  // leuchtende Bloecke: unabhaengig von Tageszeit und Schatten
            v[k].sky = 2.0f;
            v[k].shade = 1.0f;
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

}  // namespace

void buildChunkMesh(const BlockId* padded, MeshOutput& out) {
    out.clear();
    Ctx c;
    c.p = padded;
    computeHeightmap(c);

    for (int y = 0; y <= c.maxY; ++y) {
        for (int z = 0; z < CHUNK_W; ++z) {
            for (int x = 0; x < CHUNK_W; ++x) {
                BlockId b = padded[padIndex(x + 1, y, z + 1)];
                if (b == AIR) continue;
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
