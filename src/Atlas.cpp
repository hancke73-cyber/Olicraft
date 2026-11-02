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

void drawMeat(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) c.set(x, y, {0, 0, 0, 0});
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float dx = (x - 9.0f) / 5.8f, dy = (y - 8.5f) / 4.8f;
            float d = dx * dx + dy * dy;
            if (d < 1.0f) {
                int n = static_cast<int>((rnd(x, y, TILE_MEAT) - 0.5f) * 30);
                Rgba col = d > 0.62f ? Rgba{150, 44, 52, 255} : Rgba{206, 78, 88, 255};
                col = shade(col, n);
                if (d < 0.5f && rnd(x, y, TILE_MEAT, 5) > 0.82f) col = {240, 178, 176, 255};  // Fett
                c.set(x, y, col);
            }
        }
    // Knochen unten links
    const int bone[][2] = {{4, 5}, {3, 4}, {2, 3}, {2, 2}, {3, 2}, {1, 3}, {3, 1}};
    for (auto& b : bone) c.set(b[0], b[1], {232, 226, 208, 255});
}

void drawPig(TileCanvas& c) { drawNoisy(c, TILE_PIG, {236, 152, 162, 255}, 10); }

void drawPigFace(TileCanvas& c) {
    drawNoisy(c, TILE_PIG_FACE, {236, 152, 162, 255}, 8);
    for (int y = 3; y <= 7; ++y)
        for (int x = 5; x <= 10; ++x) c.set(x, y, {246, 180, 186, 255});  // Schnauze
    c.set(6, 5, {150, 70, 80, 255});
    c.set(9, 5, {150, 70, 80, 255});
    for (int ex : {3, 12})
        for (int y = 9; y <= 10; ++y) c.set(ex, y, {30, 20, 24, 255});
}

void drawCow(TileCanvas& c) {
    Noise n(31);
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float v = n.fbm2(x * 0.22f + 5.0f, y * 0.22f + 9.0f, 2);
            int d = static_cast<int>((rnd(x, y, TILE_COW) - 0.5f) * 14);
            if (v > 0.12f) c.set(x, y, shade({58, 44, 40, 255}, d));
            else c.set(x, y, shade({228, 226, 222, 255}, d));
        }
}

void drawCowFace(TileCanvas& c) {
    drawNoisy(c, TILE_COW_FACE, {80, 56, 44, 255}, 8);
    for (int y = 1; y <= 5; ++y)
        for (int x = 4; x <= 11; ++x) c.set(x, y, {206, 190, 176, 255});  // Maul
    c.set(6, 3, {110, 80, 70, 255});
    c.set(9, 3, {110, 80, 70, 255});
    for (int ex : {2, 13})
        for (int y = 9; y <= 10; ++y) c.set(ex, y, {20, 16, 16, 255});
}

void drawSheepWool(TileCanvas& c) { drawNoisy(c, TILE_SHEEP_WOOL, {232, 228, 218, 255}, 16); }

void drawSheepFace(TileCanvas& c) {
    drawNoisy(c, TILE_SHEEP_FACE, {128, 108, 92, 255}, 8);
    for (int ex : {3, 12}) {
        c.set(ex, 9, {240, 240, 240, 255});
        c.set(ex, 10, {20, 16, 16, 255});
    }
    for (int x = 6; x <= 9; ++x) c.set(x, 3, {90, 70, 62, 255});
}

void drawSheepLeg(TileCanvas& c) { drawNoisy(c, TILE_SHEEP_LEG, {178, 158, 138, 255}, 10); }

void drawZombieSkin(TileCanvas& c) { drawNoisy(c, TILE_ZOMBIE_SKIN, {88, 134, 68, 255}, 12); }

void drawZombieFace(TileCanvas& c) {
    drawNoisy(c, TILE_ZOMBIE_FACE, {88, 134, 68, 255}, 10);
    for (int y = 9; y <= 10; ++y)
        for (int x : {3, 4, 5, 10, 11, 12}) c.set(x, y, {22, 28, 24, 255});
    for (int x = 5; x <= 10; ++x) c.set(x, 4, {44, 66, 38, 255});
    c.set(5, 5, {44, 66, 38, 255});
    c.set(10, 5, {44, 66, 38, 255});
}

void drawZombieShirt(TileCanvas& c) { drawNoisy(c, TILE_ZOMBIE_SHIRT, {52, 150, 168, 255}, 12); }
void drawZombiePants(TileCanvas& c) { drawNoisy(c, TILE_ZOMBIE_PANTS, {52, 54, 148, 255}, 10); }

void drawLava(TileCanvas& c) {
    Noise n(57);
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            // periodisch (nahtlos): Rauschen ueber Torus-Koordinaten
            float v = 0.0f;
            for (int ox = 0; ox <= 1; ++ox)
                for (int oy = 0; oy <= 1; ++oy) {
                    float wx = (ox ? x - TILE_PX : x) * 0.28f, wy = (oy ? y - TILE_PX : y) * 0.28f;
                    float wgt = (ox ? static_cast<float>(x) / TILE_PX : 1.0f - static_cast<float>(x) / TILE_PX) *
                                (oy ? static_cast<float>(y) / TILE_PX : 1.0f - static_cast<float>(y) / TILE_PX);
                    v += wgt * n.fbm2(wx + 3.0f, wy + 7.0f, 2);
                }
            int r = 235, g = 95 + static_cast<int>((v + 0.5f) * 150), b = 12 + static_cast<int>(std::max(0.0f, v + 0.2f) * 60);
            c.set(x, y, {r, g, b, 255});
        }
}

void drawObsidian(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int base = 18 + static_cast<int>(rnd(x, y, TILE_OBSIDIAN) * 22);
            int r = base, g = base - 4, b = base + 18;
            if (rnd(x, y, TILE_OBSIDIAN, 6) > 0.92f) { r += 40; g += 16; b += 70; }
            c.set(x, y, {r, g, b, 255});
        }
}

void drawCraftTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_CRAFT_TOP) - 0.5f) * 14);
            Rgba col = shade({160, 128, 76, 255}, d);
            if (x == 0 || y == 0 || x == TILE_PX - 1 || y == TILE_PX - 1) col = shade({96, 70, 40, 255}, d);
            else if (x % 5 == 0 || y % 5 == 0) col = shade({96, 70, 40, 255}, d);  // Raster
            c.set(x, y, col);
        }
}

void drawCraftSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_CRAFT_SIDE) - 0.5f) * 14);
            Rgba col = shade({150, 118, 70, 255}, d);
            if (y >= 12) col = shade({92, 66, 38, 255}, d);                 // Tischplatte (oben)
            if (y < 2) col = shade({110, 80, 46, 255}, d);
            // Werkzeuge an der Seite: Saege und Hammer als Farbflaechen
            if (x >= 3 && x <= 6 && y >= 4 && y <= 9) col = {190, 190, 195, 255};
            if (x >= 9 && x <= 12 && y >= 5 && y <= 8) col = {95, 95, 100, 255};
            if (x >= 10 && x <= 11 && y >= 2 && y <= 4) col = {110, 70, 40, 255};
            c.set(x, y, col);
        }
}

// Werkzeuge: Griff als Diagonale, Kopf je nach Art, in Materialfarbe
void drawToolBase(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) c.set(x, y, {0, 0, 0, 0});
}
void put(TileCanvas& c, int x, int y, Rgba col) {
    if (x >= 0 && x < TILE_PX && y >= 0 && y < TILE_PX) c.set(x, y, col);
}
void handle(TileCanvas& c, int from, int to) {
    for (int i = from; i <= to; ++i) {
        put(c, i, i, {120, 84, 44, 255});
        put(c, i + 1, i, {86, 58, 30, 255});
    }
}

void drawStick(TileCanvas& c) {
    drawToolBase(c);
    handle(c, 2, 13);
}

void drawTool(TileCanvas& c, int kind, Rgba m) {  // kind: 0 Spitzhacke, 1 Axt, 2 Schaufel, 3 Schwert
    drawToolBase(c);
    const Rgba dark = {m.r * 7 / 10, m.g * 7 / 10, m.b * 7 / 10, 255};
    if (kind == 0) {
        handle(c, 2, 11);
        const int pts[][2] = {{5, 13}, {6, 13}, {7, 13}, {8, 13}, {9, 13}, {10, 13}, {11, 12}, {12, 11}, {13, 10},
                              {13, 9},  {4, 12}, {3, 11}, {10, 12}, {11, 11}, {12, 10}};
        for (auto& p : pts) put(c, p[0], p[1], m);
        for (auto& p : {std::pair<int, int>{4, 11}, {12, 9}}) put(c, p.first, p.second, dark);
    } else if (kind == 1) {
        handle(c, 2, 11);
        const int pts[][2] = {{9, 13}, {10, 13}, {11, 13}, {12, 13}, {9, 12}, {10, 12}, {11, 12}, {12, 12}, {13, 12},
                              {10, 11}, {11, 11}, {12, 11}, {11, 10}};
        for (auto& p : pts) put(c, p[0], p[1], m);
        for (auto& p : {std::pair<int, int>{13, 12}, {12, 13}}) put(c, p.first, p.second, dark);
    } else if (kind == 2) {
        handle(c, 2, 10);
        const int pts[][2] = {{11, 13}, {12, 13}, {10, 12}, {11, 12}, {12, 12}, {13, 12}, {10, 11}, {11, 11},
                              {12, 11}, {13, 11}, {11, 10}, {12, 10}};
        for (auto& p : pts) put(c, p[0], p[1], m);
        for (auto& p : {std::pair<int, int>{13, 11}, {12, 10}}) put(c, p.first, p.second, dark);
    } else {
        handle(c, 2, 4);
        for (int i = 6; i <= 13; ++i) {
            put(c, i, i, m);
            put(c, i + 1, i, dark);
            put(c, i, i + 1, m);
        }
        put(c, 5, 7, {120, 84, 44, 255});  // Parierstange
        put(c, 6, 6, {120, 84, 44, 255});
        put(c, 7, 5, {120, 84, 44, 255});
        put(c, 4, 8, {120, 84, 44, 255});
        put(c, 8, 4, {120, 84, 44, 255});
    }
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
    make(TILE_MEAT, drawMeat);
    make(TILE_PIG, drawPig);
    make(TILE_PIG_FACE, drawPigFace);
    make(TILE_COW, drawCow);
    make(TILE_COW_FACE, drawCowFace);
    make(TILE_SHEEP_WOOL, drawSheepWool);
    make(TILE_SHEEP_FACE, drawSheepFace);
    make(TILE_SHEEP_LEG, drawSheepLeg);
    make(TILE_ZOMBIE_SKIN, drawZombieSkin);
    make(TILE_ZOMBIE_FACE, drawZombieFace);
    make(TILE_ZOMBIE_SHIRT, drawZombieShirt);
    make(TILE_ZOMBIE_PANTS, drawZombiePants);
    make(TILE_LAVA, drawLava);
    make(TILE_OBSIDIAN, drawObsidian);
    make(TILE_CRAFT_TOP, drawCraftTop);
    make(TILE_CRAFT_SIDE, drawCraftSide);
    make(TILE_STICK, drawStick);
    {
        const Rgba wood = {214, 176, 102, 255}, stone = {150, 150, 158, 255};
        const int tiles[2][4] = {{TILE_PICKAXE_WOOD, TILE_AXE_WOOD, TILE_SHOVEL_WOOD, TILE_SWORD_WOOD},
                                 {TILE_PICKAXE_STONE, TILE_AXE_STONE, TILE_SHOVEL_STONE, TILE_SWORD_STONE}};
        for (int mat = 0; mat < 2; ++mat)
            for (int kind = 0; kind < 4; ++kind) {
                TileCanvas cv(px, tiles[mat][kind]);
                drawTool(cv, kind, mat == 0 ? wood : stone);
            }
    }
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
