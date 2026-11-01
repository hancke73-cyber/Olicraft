#include "Atlas.h"

#include <algorithm>
#include <cmath>

#include "Noise.h"

namespace oc {

namespace {

struct Rgba {
    int r, g, b, a;
};

class TileCanvas {
public:
    TileCanvas(std::vector<uint8_t>& atlas, int tile) : atlas_(atlas), tile_(tile) {}

    // x nach rechts, y nach oben (v-Richtung)
    void set(int x, int y, Rgba c) {
        int tx = (tile_ % ATLAS_TILES) * TILE_PX + x;
        int ty = (tile_ / ATLAS_TILES) * TILE_PX + y;
        size_t i = (static_cast<size_t>(ty) * ATLAS_PX + tx) * 4;
        atlas_[i + 0] = clamp8(c.r);
        atlas_[i + 1] = clamp8(c.g);
        atlas_[i + 2] = clamp8(c.b);
        atlas_[i + 3] = clamp8(c.a);
    }

private:
    static uint8_t clamp8(int v) { return static_cast<uint8_t>(std::max(0, std::min(255, v))); }
    std::vector<uint8_t>& atlas_;
    int tile_;
};

// Zufallswert [0,1) pro Pixel
float rnd(int x, int y, int tile, uint32_t salt = 0) {
    return (hash3(x, y, tile, 0xC0FFEEu + salt) & 0xFFFFu) / 65536.0f;
}

Rgba shade(Rgba base, int delta) { return {base.r + delta, base.g + delta, base.b + delta, base.a}; }

void drawNoisy(TileCanvas& c, int tile, Rgba base, int amp) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, tile) - 0.5f) * 2.0f * amp);
            c.set(x, y, shade(base, d));
        }
}

void drawGrassTop(TileCanvas& c) {
    drawNoisy(c, TILE_GRASS_TOP, {96, 160, 54, 255}, 14);
}

void drawGrassSide(TileCanvas& c) {
    drawNoisy(c, TILE_GRASS_SIDE, {134, 96, 67, 255}, 12);
    for (int x = 0; x < TILE_PX; ++x) {
        // Gras-Rand oben (y = 15 ist oben), unregelmaessig lang
        int depth = 3 + static_cast<int>(rnd(x, 0, TILE_GRASS_SIDE, 7) * 3.0f);
        for (int k = 0; k < depth; ++k) {
            int y = TILE_PX - 1 - k;
            int d = static_cast<int>((rnd(x, y, TILE_GRASS_SIDE, 3) - 0.5f) * 28);
            c.set(x, y, shade({96, 160, 54, 255}, d));
        }
    }
}

void drawStone(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float r = rnd(x, y, TILE_STONE);
            int base = 124 + static_cast<int>((r - 0.5f) * 22);
            if (rnd(x, y, TILE_STONE, 9) > 0.93f) base -= 22;
            c.set(x, y, {base, base, base, 255});
        }
}

void drawSand(TileCanvas& c) {
    drawNoisy(c, TILE_SAND, {219, 207, 142, 255}, 10);
}

void drawWater(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x / 2, y / 2, TILE_WATER) - 0.5f) * 22);
            c.set(x, y, {40 + d, 90 + d, 200 + d / 2, 170});
        }
}

void drawLogSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float streak = rnd(x, 0, TILE_LOG_SIDE, 5);  // vertikale Rillen
            int d = static_cast<int>((streak - 0.5f) * 34 + (rnd(x, y, TILE_LOG_SIDE) - 0.5f) * 10);
            c.set(x, y, shade({104, 82, 50, 255}, d));
        }
}

void drawLogTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float dx = x - 7.5f, dy = y - 7.5f;
            float r = std::sqrt(dx * dx + dy * dy);
            bool ring = (static_cast<int>(r * 1.2f) % 2) == 0;
            Rgba base = ring ? Rgba{172, 138, 84, 255} : Rgba{150, 116, 68, 255};
            if (std::max(std::fabs(dx), std::fabs(dy)) > 6.9f) base = {104, 82, 50, 255};  // Rinde am Rand
            int d = static_cast<int>((rnd(x, y, TILE_LOG_TOP) - 0.5f) * 10);
            c.set(x, y, shade(base, d));
        }
}

void drawLeaves(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float r = rnd(x, y, TILE_LEAVES);
            int d = static_cast<int>((rnd(x, y, TILE_LEAVES, 4) - 0.5f) * 44);
            Rgba col = shade({54, 128, 40, 255}, d);
            if (r > 0.88f) col.a = 0;  // Luecken im Laub
            c.set(x, y, col);
        }
}

void drawPlanks(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int board = y / 4;
            int d = static_cast<int>((rnd(x, board, TILE_PLANKS, 2) - 0.5f) * 16 + (rnd(x, y, TILE_PLANKS) - 0.5f) * 8);
            Rgba col = shade({162, 130, 78, 255}, d);
            if (y % 4 == 3) col = shade(col, -34);
            // versetzte Stossfugen
            int joint = (board % 2 == 0) ? 5 : 11;
            if (x == joint && y % 4 != 3) col = shade(col, -24);
            c.set(x, y, col);
        }
}

void drawCobble(TileCanvas& c) {
    // Voronoi-aehnliche Steine
    const int N = 9;
    float px[N], py[N];
    for (int i = 0; i < N; ++i) {
        px[i] = rnd(i, 1, TILE_COBBLE, 21) * TILE_PX;
        py[i] = rnd(i, 2, TILE_COBBLE, 22) * TILE_PX;
    }
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float best = 1e9f, second = 1e9f;
            int bi = 0;
            for (int i = 0; i < N; ++i) {
                for (int ox = -1; ox <= 1; ++ox)
                    for (int oy = -1; oy <= 1; ++oy) {
                        float dx = x - (px[i] + ox * TILE_PX), dy = y - (py[i] + oy * TILE_PX);
                        float d = dx * dx + dy * dy;
                        if (d < best) {
                            second = best;
                            best = d;
                            bi = i;
                        } else if (d < second) {
                            second = d;
                        }
                    }
            }
            int base = 108 + static_cast<int>(rnd(bi, 3, TILE_COBBLE, 23) * 50);
            base += static_cast<int>((rnd(x, y, TILE_COBBLE) - 0.5f) * 12);
            if (std::sqrt(second) - std::sqrt(best) < 1.1f) base -= 42;  // Fugen
            c.set(x, y, {base, base, base, 255});
        }
}

void drawBedrock(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int base = 40 + static_cast<int>(rnd(x, y, TILE_BEDROCK) * 60);
            if (rnd(x, y, TILE_BEDROCK, 8) > 0.8f) base += 25;
            c.set(x, y, {base, base, base, 255});
        }
}

void drawGlass(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            bool edge = x == 0 || y == 0 || x == TILE_PX - 1 || y == TILE_PX - 1;
            bool streak = (x == y && x > 2 && x < 7) || (x == y + 1 && x > 3 && x < 7) || (x == 11 && y == 10);
            if (edge) {
                c.set(x, y, {200, 230, 240, 255});
            } else if (streak) {
                c.set(x, y, {235, 245, 250, 255});
            } else {
                c.set(x, y, {200, 230, 240, 0});
            }
        }
}

void drawSnow(TileCanvas& c) {
    drawNoisy(c, TILE_SNOW, {242, 246, 250, 255}, 6);
}

}  // namespace

std::vector<uint8_t> generateAtlasPixels() {
    std::vector<uint8_t> px(static_cast<size_t>(ATLAS_PX) * ATLAS_PX * 4, 0);
    auto make = [&](int tile, void (*fn)(TileCanvas&)) {
        TileCanvas c(px, tile);
        fn(c);
    };
    make(TILE_GRASS_TOP, drawGrassTop);
    make(TILE_GRASS_SIDE, drawGrassSide);
    {
        TileCanvas c(px, TILE_DIRT);
        drawNoisy(c, TILE_DIRT, {134, 96, 67, 255}, 14);
    }
    make(TILE_STONE, drawStone);
    make(TILE_SAND, drawSand);
    make(TILE_WATER, drawWater);
    make(TILE_LOG_SIDE, drawLogSide);
    make(TILE_LOG_TOP, drawLogTop);
    make(TILE_LEAVES, drawLeaves);
    make(TILE_PLANKS, drawPlanks);
    make(TILE_COBBLE, drawCobble);
    make(TILE_BEDROCK, drawBedrock);
    make(TILE_GLASS, drawGlass);
    make(TILE_SNOW, drawSnow);
    return px;
}

UVRect tileUV(int tile) {
    const float inset = 0.04f;  // Pixelbruchteil gegen Bluten zwischen Kacheln
    float tx = static_cast<float>(tile % ATLAS_TILES) * TILE_PX;
    float ty = static_cast<float>(tile / ATLAS_TILES) * TILE_PX;
    return {(tx + inset) / ATLAS_PX, (ty + inset) / ATLAS_PX, (tx + TILE_PX - inset) / ATLAS_PX,
            (ty + TILE_PX - inset) / ATLAS_PX};
}

}  // namespace oc
