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

void stoneBase(TileCanvas& c, int tile) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float r = rnd(x, y, tile, 31);
            int base = 124 + static_cast<int>((r - 0.5f) * 22);
            c.set(x, y, {base, base, base, 255});
        }
}

// Erzklumpen: kleine Gruppen aus Pixeln in der Erzfarbe
void drawOre(TileCanvas& c, int tile, Rgba ore, Rgba light) {
    stoneBase(c, tile);
    for (int k = 0; k < 7; ++k) {
        int cx = 1 + static_cast<int>(rnd(k, 1, tile, 41) * 13), cy = 1 + static_cast<int>(rnd(k, 2, tile, 42) * 13);
        const int offs[][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {-1, 0}};
        int n = 2 + static_cast<int>(rnd(k, 3, tile, 43) * 3);
        for (int i = 0; i < n; ++i) {
            const int px = cx + offs[i][0], py = cy + offs[i][1];
            if (px < 0 || py < 0 || px >= TILE_PX || py >= TILE_PX) continue;
            c.set(px, py, (i == 0) ? light : ore);
        }
    }
}
void drawCoalOre(TileCanvas& c) { drawOre(c, TILE_COAL_ORE, {32, 32, 36, 255}, {64, 64, 70, 255}); }
void drawIronOre(TileCanvas& c) { drawOre(c, TILE_IRON_ORE, {214, 160, 130, 255}, {240, 196, 170, 255}); }
void drawGoldOre(TileCanvas& c) { drawOre(c, TILE_GOLD_ORE, {244, 200, 40, 255}, {255, 240, 150, 255}); }
void drawDiamondOre(TileCanvas& c) { drawOre(c, TILE_DIAMOND_ORE, {70, 220, 226, 255}, {190, 255, 255, 255}); }

void drawFurnaceBody(TileCanvas& c, int tile) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int base = 112 + static_cast<int>((rnd(x, y, tile) - 0.5f) * 26);
            if ((x + (y / 4) * 3) % 8 == 0 || y % 4 == 3) base -= 26;  // Fugen wie Bruchstein
            c.set(x, y, {base, base, base, 255});
        }
}

void drawFurnaceSide(TileCanvas& c) {
    drawFurnaceBody(c, TILE_FURNACE);
    for (int y = 3; y <= 9; ++y)
        for (int x = 4; x <= 11; ++x) c.set(x, y, {26, 26, 28, 255});  // Feueroeffnung
    for (int x = 4; x <= 11; x += 2)
        for (int y = 3; y <= 9; ++y) c.set(x, y, {70, 70, 74, 255});  // Gitter
    for (int x = 5; x <= 10; ++x) c.set(x, 12, {60, 60, 64, 255});
}

void drawFurnaceLit(TileCanvas& c) {
    drawFurnaceBody(c, TILE_FURNACE_LIT);
    for (int y = 3; y <= 9; ++y)
        for (int x = 4; x <= 11; ++x) {
            int hot = 140 + static_cast<int>(rnd(x, y, TILE_FURNACE_LIT, 3) * 100);
            c.set(x, y, {255, hot, 20 + static_cast<int>(rnd(x, y, 7, 4) * 30), 255});
        }
    for (int x = 4; x <= 11; x += 2)
        for (int y = 3; y <= 9; ++y) c.set(x, y, {120, 50, 10, 255});
    for (int x = 5; x <= 10; ++x) c.set(x, 12, {60, 60, 64, 255});
}

void drawFurnaceTop(TileCanvas& c) {
    drawFurnaceBody(c, TILE_FURNACE_TOP);
    for (int y = 4; y <= 11; ++y)
        for (int x = 4; x <= 11; ++x) {
            float dx = x - 7.5f, dy = y - 7.5f;
            if (dx * dx + dy * dy < 14.0f) c.set(x, y, {48, 48, 52, 255});
        }
}

void drawCoalItem(TileCanvas& c) {
    drawToolBase(c);
    for (int y = 3; y <= 12; ++y)
        for (int x = 3; x <= 12; ++x) {
            float dx = (x - 7.5f) / 4.7f, dy = (y - 7.5f) / 4.2f;
            if (dx * dx + dy * dy < 1.0f + (rnd(x, y, TILE_COAL, 5) - 0.5f) * 0.5f) {
                int v = 34 + static_cast<int>(rnd(x, y, TILE_COAL) * 22);
                if ((x + y) % 5 == 0) v += 36;
                c.set(x, y, {v, v, v + 4, 255});
            }
        }
}

void drawIronIngot(TileCanvas& c) {
    drawToolBase(c);
    for (int y = 5; y <= 10; ++y)
        for (int x = 2; x <= 13; ++x) {
            const int slant = (10 - y) / 2;  // Parallelogramm
            if (x < 2 + slant || x > 11 + slant) continue;
            int v = 206 - (y - 5) * 7 + static_cast<int>((rnd(x, y, TILE_IRON_INGOT) - 0.5f) * 14);
            c.set(x, y, {v, v, v + 6, 255});
        }
    for (int x = 4; x <= 11; ++x) c.set(x, 10, {120, 120, 128, 255});
}

void drawTorch(TileCanvas& c) {
    drawToolBase(c);
    for (int y = 0; y <= 9; ++y) {
        c.set(7, y, {138, 98, 52, 255});
        c.set(8, y, {96, 66, 34, 255});
    }
    const Rgba flame[3][4] = {{{255, 170, 30, 255}, {255, 210, 60, 255}, {255, 210, 60, 255}, {255, 150, 20, 255}},
                              {{255, 190, 40, 255}, {255, 245, 160, 255}, {255, 240, 140, 255}, {255, 170, 30, 255}},
                              {{0, 0, 0, 0}, {255, 230, 100, 255}, {255, 215, 80, 255}, {0, 0, 0, 0}}};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col)
            if (flame[row][col].a) c.set(6 + col, 10 + row, flame[row][col]);
    c.set(7, 13, {255, 240, 150, 255});
    c.set(8, 13, {255, 200, 70, 255});
}

void drawGoldIngot(TileCanvas& c) {
    drawToolBase(c);
    for (int y = 5; y <= 10; ++y)
        for (int x = 2; x <= 13; ++x) {
            const int slant = (10 - y) / 2;
            if (x < 2 + slant || x > 11 + slant) continue;
            int v = static_cast<int>((rnd(x, y, TILE_GOLD_INGOT) - 0.5f) * 20);
            c.set(x, y, {246 + v / 4 - (y - 5) * 4, 200 + v - (y - 5) * 8, 40 + v / 2, 255});
        }
    for (int x = 4; x <= 11; ++x) c.set(x, 10, {160, 118, 20, 255});
}

void drawDiamondItem(TileCanvas& c) {
    drawToolBase(c);
    for (int y = 2; y <= 13; ++y)
        for (int x = 2; x <= 13; ++x) {
            const float dx = std::fabs(x - 7.5f), dy = std::fabs(y - 7.5f);
            if (dx + dy * 0.9f < 5.6f) {
                int v = 170 + static_cast<int>((rnd(x, y, TILE_DIAMOND) * 70.0f));
                Rgba col = {60 + (v - 170) / 2, v, 235, 255};
                if (dx + dy * 0.9f > 4.6f) col = {40, 150, 180, 255};
                c.set(x, y, col);
            }
        }
    c.set(5, 5, {240, 255, 255, 255});
    c.set(6, 4, {240, 255, 255, 255});
}

void drawCookedMeat(TileCanvas& c) {
    drawToolBase(c);
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            float dx = (x - 9.0f) / 5.8f, dy = (y - 8.5f) / 4.8f;
            float d = dx * dx + dy * dy;
            if (d < 1.0f) {
                int n = static_cast<int>((rnd(x, y, TILE_COOKED_MEAT) - 0.5f) * 28);
                Rgba col = d > 0.62f ? Rgba{92, 52, 28, 255} : Rgba{156, 98, 52, 255};
                c.set(x, y, shade(col, n));
            }
        }
    const int bone[][2] = {{4, 5}, {3, 4}, {2, 3}, {2, 2}, {3, 2}, {1, 3}, {3, 1}};
    for (auto& b : bone) c.set(b[0], b[1], {232, 226, 208, 255});
}


// ---------------------------------------------------------------------------------------------------------------
// Neue Bausteine, Pflanzen und Gegenstaende
// ---------------------------------------------------------------------------------------------------------------
void clearTile(TileCanvas& c) { drawToolBase(c); }

void drawBricks(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const int row = y / 4;
            const int off = (row % 2) * 4;
            const bool mortarH = y % 4 == 3;
            const bool mortarV = (x + off) % 8 == 7;
            int d = static_cast<int>((rnd(x, y, TILE_BRICKS) - 0.5f) * 20);
            if (mortarH || mortarV) c.set(x, y, shade({186, 180, 170, 255}, d / 2));
            else c.set(x, y, shade({156, 78, 60, 255}, d));
        }
}

void drawStoneBricks(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const int row = y / 8;
            const bool jointH = y % 8 == 7;
            const bool jointV = (x + row * 4) % 8 == 7;
            int base = 128 + static_cast<int>((rnd(x, y, TILE_STONE_BRICKS) - 0.5f) * 20);
            if (jointH || jointV) base = 88 + static_cast<int>(rnd(x, y, TILE_STONE_BRICKS, 3) * 10);
            c.set(x, y, {base, base, base + 2, 255});
        }
}

void drawSandstoneTop(TileCanvas& c) { drawNoisy(c, TILE_SANDSTONE_TOP, {214, 198, 140, 255}, 8); }
void drawSandstoneSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_SANDSTONE_SIDE) - 0.5f) * 14);
            Rgba col = shade({218, 202, 146, 255}, d);
            if (y == 3 || y == 12) col = shade({184, 166, 108, 255}, d);
            if (y > 3 && y < 12 && (y == 7 || y == 8)) col = shade({206, 188, 128, 255}, d);
            c.set(x, y, col);
        }
}

void drawGravel(TileCanvas& c) {
    static const int greys[3] = {96, 128, 156};
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const int cell = static_cast<int>(hash3(x / 3, y / 3, TILE_GRAVEL, 21u) % 3);
            const int edge = (x % 3 == 2 || y % 3 == 2) ? -26 : 0;
            const int v = greys[cell] + edge + static_cast<int>((rnd(x, y, TILE_GRAVEL) - 0.5f) * 14);
            c.set(x, y, {v, v - 2, v - 4, 255});
        }
}

void drawClay(TileCanvas& c) { drawNoisy(c, TILE_CLAY, {160, 166, 182, 255}, 6); }

void drawWool(TileCanvas& c, int tile, Rgba base) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const int weave = ((x / 2 + y / 2) % 2) ? 7 : -5;
            const int d = weave + static_cast<int>((rnd(x, y, tile) - 0.5f) * 10);
            c.set(x, y, shade(base, d));
        }
}

void drawBookshelf(TileCanvas& c) {
    static const Rgba book[6] = {{150, 40, 40, 255}, {50, 80, 150, 255}, {60, 120, 60, 255}, {170, 130, 50, 255}, {110, 60, 110, 255}, {200, 190, 170, 255}};
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_BOOKSHELF) - 0.5f) * 14);
            Rgba col = shade({160, 124, 72, 255}, d);  // Brett
            const bool shelf = y <= 1 || y >= 14 || y == 7 || y == 8;
            if (!shelf) {
                const int idx = static_cast<int>(hash3(x / 2, y < 7 ? 0 : 1, TILE_BOOKSHELF, 33u) % 6);
                col = shade(book[idx], d + ((x % 2) ? -14 : 0));
                if (y == 6 || y == 9 || y == 13 || y == 2) col = shade(col, -30);  // Schatten unter dem Brett
            }
            c.set(x, y, col);
        }
}

void drawMetalBlock(TileCanvas& c, int tile, Rgba base, Rgba light, Rgba dark) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const bool edge = x == 0 || y == 0 || x == TILE_PX - 1 || y == TILE_PX - 1;
            const bool inner = x == 1 || y == 1 || x == TILE_PX - 2 || y == TILE_PX - 2;
            int d = static_cast<int>((rnd(x, y, tile) - 0.5f) * 10);
            Rgba col = base;
            if (edge) col = dark;
            else if (inner) col = (x + y < 16) ? light : shade(base, -16);
            c.set(x, y, shade(col, d));
        }
}

void drawIronBlock(TileCanvas& c) { drawMetalBlock(c, TILE_IRON_BLOCK, {214, 214, 220, 255}, {240, 240, 244, 255}, {150, 150, 158, 255}); }
void drawGoldBlock(TileCanvas& c) { drawMetalBlock(c, TILE_GOLD_BLOCK, {246, 208, 64, 255}, {255, 240, 150, 255}, {176, 136, 24, 255}); }
void drawDiamondBlock(TileCanvas& c) { drawMetalBlock(c, TILE_DIAMOND_BLOCK, {80, 214, 220, 255}, {190, 255, 255, 255}, {30, 130, 140, 255}); }
void drawCoalBlock(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int v = 30 + static_cast<int>(rnd(x, y, TILE_COAL_BLOCK) * 22);
            if (rnd(x, y, TILE_COAL_BLOCK, 9) > 0.92f) v += 30;
            if (x == 0 || y == 0 || x == TILE_PX - 1 || y == TILE_PX - 1) v -= 14;
            c.set(x, y, {v, v, v + 4, 255});
        }
}

void drawLamp(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const bool frame = x <= 1 || y <= 1 || x >= TILE_PX - 2 || y >= TILE_PX - 2;
            const bool bar = x == 7 || x == 8 || y == 7 || y == 8;
            int d = static_cast<int>((rnd(x, y, TILE_LAMP) - 0.5f) * 14);
            if (frame || bar) c.set(x, y, shade({120, 84, 44, 255}, d));
            else {
                const float dx = x - 7.5f, dy = y - 7.5f;
                const int glow = static_cast<int>(40 - std::sqrt(dx * dx + dy * dy) * 4.0f);
                c.set(x, y, {255, 224 + glow / 2 + d / 2, 140 + glow + d, 255});
            }
        }
}

void drawPumpkinTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_PUMPKIN_TOP) - 0.5f) * 16);
            Rgba col = shade({198, 118, 24, 255}, d);
            if (x >= 6 && x <= 9 && y >= 6 && y <= 9) col = shade({88, 112, 40, 255}, d);
            c.set(x, y, col);
        }
}
void drawPumpkinSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_PUMPKIN_SIDE) - 0.5f) * 14);
            const int ridge = (x % 4 == 0) ? -26 : ((x % 4 == 1) ? 8 : 0);
            c.set(x, y, shade({206, 124, 26, 255}, d + ridge));
        }
}

void drawCactusTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_CACTUS_TOP) - 0.5f) * 14);
            Rgba col = shade({66, 128, 52, 255}, d);
            if (x == 0 || y == 0 || x == TILE_PX - 1 || y == TILE_PX - 1) col = shade({40, 90, 36, 255}, d);
            if (x >= 6 && x <= 9 && y >= 6 && y <= 9) col = shade({110, 160, 80, 255}, d);
            c.set(x, y, col);
        }
}
void drawCactusSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_CACTUS_SIDE) - 0.5f) * 12);
            const int ridge = (x % 4 == 0 || x % 4 == 3) ? -20 : 8;
            Rgba col = shade({70, 136, 54, 255}, d + ridge);
            if ((x % 4 == 2) && (y % 4 == 1)) col = {230, 226, 190, 255};  // Stacheln
            c.set(x, y, col);
        }
}

void drawBirchSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_BIRCH_LOG_SIDE) - 0.5f) * 14);
            Rgba col = shade({224, 222, 212, 255}, d);
            const uint32_t h = hash3(x / 3, y, TILE_BIRCH_LOG_SIDE, 41u) % 11u;
            if (h < 2 && (x % 3 != 1)) col = shade({40, 38, 36, 255}, d);  // dunkle Rindenmarken
            c.set(x, y, col);
        }
}
void drawBirchTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const float dx = x - 7.5f, dy = y - 7.5f;
            const float r = std::sqrt(dx * dx + dy * dy);
            Rgba base = (static_cast<int>(r * 1.2f) % 2 == 0) ? Rgba{210, 190, 138, 255} : Rgba{192, 170, 118, 255};
            if (std::max(std::fabs(dx), std::fabs(dy)) > 6.9f) base = {224, 222, 212, 255};
            c.set(x, y, shade(base, static_cast<int>((rnd(x, y, TILE_BIRCH_LOG_TOP) - 0.5f) * 10)));
        }
}
void drawLeavesVariant(TileCanvas& c, int tile, Rgba base, float gap) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const float r = rnd(x, y, tile);
            const int d = static_cast<int>((rnd(x, y, tile, 4) - 0.5f) * 44);
            Rgba col = shade(base, d);
            if (r > gap) col.a = 0;
            c.set(x, y, col);
        }
}
void drawBirchLeaves(TileCanvas& c) { drawLeavesVariant(c, TILE_BIRCH_LEAVES, {104, 156, 54, 255}, 0.88f); }
void drawSpruceLeaves(TileCanvas& c) { drawLeavesVariant(c, TILE_SPRUCE_LEAVES, {40, 92, 56, 255}, 0.86f); }
void drawSpruceSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const float streak = rnd(x, 0, TILE_SPRUCE_LOG_SIDE, 5);
            const int d = static_cast<int>((streak - 0.5f) * 34 + (rnd(x, y, TILE_SPRUCE_LOG_SIDE) - 0.5f) * 10);
            c.set(x, y, shade({72, 52, 32, 255}, d));
        }
}
void drawSpruceTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const float dx = x - 7.5f, dy = y - 7.5f;
            const float r = std::sqrt(dx * dx + dy * dy);
            Rgba base = (static_cast<int>(r * 1.2f) % 2 == 0) ? Rgba{140, 108, 64, 255} : Rgba{120, 90, 52, 255};
            if (std::max(std::fabs(dx), std::fabs(dy)) > 6.9f) base = {72, 52, 32, 255};
            c.set(x, y, shade(base, static_cast<int>((rnd(x, y, TILE_SPRUCE_LOG_TOP) - 0.5f) * 10)));
        }
}

void drawChestSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_CHEST_SIDE) - 0.5f) * 14);
            Rgba col = shade({152, 108, 56, 255}, d);
            const bool border = x == 0 || x == TILE_PX - 1 || y == 0 || y == TILE_PX - 1;
            if (border) col = shade({96, 64, 30, 255}, d);
            if (y == 9) col = shade({84, 56, 26, 255}, d);  // Fuge zwischen Deckel und Korpus
            if (x >= 7 && x <= 8 && y >= 7 && y <= 10) col = shade({210, 200, 120, 255}, d);  // Schloss
            c.set(x, y, col);
        }
}
void drawChestTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_CHEST_TOP) - 0.5f) * 14);
            Rgba col = shade({160, 116, 62, 255}, d);
            if (x <= 1 || x >= TILE_PX - 2 || y <= 1 || y >= TILE_PX - 2) col = shade({100, 68, 32, 255}, d);
            c.set(x, y, col);
        }
}

void drawBedTop(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_BED_TOP) - 0.5f) * 10);
            Rgba col = shade({176, 44, 44, 255}, d);
            if (y >= 11) col = shade({238, 236, 228, 255}, d);  // Kissen
            if (y == 10) col = shade({130, 30, 30, 255}, d);
            c.set(x, y, col);
        }
}
void drawBedSide(TileCanvas& c) {
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_BED_SIDE) - 0.5f) * 12);
            Rgba col = shade({150, 108, 60, 255}, d);
            if (y >= 8) col = shade({176, 44, 44, 255}, d);
            if (y == 8) col = shade({120, 28, 28, 255}, d);
            c.set(x, y, col);
        }
}

// --- Pflanzen: transparenter Hintergrund, y = 0 ist der Boden
void drawTallGrass(TileCanvas& c) {
    clearTile(c);
    for (int x = 1; x < 15; ++x) {
        const uint32_t h = hash3(x, 5, TILE_TALL_GRASS, 71u);
        if (h % 100 < 30) continue;
        const int hgt = 5 + static_cast<int>(h % 9u);
        for (int y = 0; y < hgt; ++y) {
            const int sway = (y > hgt * 2 / 3) ? ((x % 2) ? 1 : -1) : 0;
            const int g = 96 + y * 6 + static_cast<int>(rnd(x, y, TILE_TALL_GRASS) * 24);
            put(c, x + sway, y, {50 + y * 2, g, 38, 255});
        }
    }
}
void drawFern(TileCanvas& c) {
    clearTile(c);
    for (int y = 0; y < 13; ++y) put(c, 7, y, {44, 112, 44, 255});
    for (int y = 2; y < 13; y += 2) {
        const int len = 6 - y / 3;
        for (int k = 1; k <= len; ++k) {
            put(c, 7 - k, y + k / 2 + 1, {52 + k * 4, 128 + k * 3, 52, 255});
            put(c, 8 + k - 1, y + k / 2 + 1, {52 + k * 4, 128 + k * 3, 52, 255});
        }
    }
}
void flowerBase(TileCanvas& c) {
    clearTile(c);
    for (int y = 0; y < 9; ++y) put(c, 7, y, {54, 130, 48, 255});
    put(c, 6, 3, {54, 130, 48, 255});
    put(c, 8, 5, {54, 130, 48, 255});
    put(c, 5, 4, {70, 150, 56, 255});
    put(c, 9, 6, {70, 150, 56, 255});
}
void flowerHead(TileCanvas& c, Rgba petal, Rgba center, int cx = 7, int cy = 10) {
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            if (std::abs(dx) == 2 && std::abs(dy) == 2) continue;
            put(c, cx + dx, cy + dy, petal);
        }
    put(c, cx, cy, center);
    put(c, cx + 1, cy, center);
}
void drawFlowerRed(TileCanvas& c) { flowerBase(c); flowerHead(c, {212, 36, 36, 255}, {40, 30, 30, 255}); }
void drawFlowerYellow(TileCanvas& c) { flowerBase(c); flowerHead(c, {244, 214, 40, 255}, {230, 140, 20, 255}); }
void drawFlowerBlue(TileCanvas& c) { flowerBase(c); flowerHead(c, {70, 96, 226, 255}, {240, 240, 250, 255}); }
void drawFlowerWhite(TileCanvas& c) { flowerBase(c); flowerHead(c, {244, 244, 244, 255}, {240, 200, 40, 255}); }
void mushroom(TileCanvas& c, Rgba cap, Rgba spot, bool dots) {
    clearTile(c);
    for (int y = 0; y < 6; ++y) {
        put(c, 7, y, {226, 214, 190, 255});
        put(c, 8, y, {204, 192, 168, 255});
    }
    for (int y = 6; y < 10; ++y) {
        const int w = y == 6 ? 4 : (y == 7 ? 5 : (y == 8 ? 4 : 2));
        for (int x = 8 - w; x < 8 + w; ++x) put(c, x, y, cap);
    }
    if (dots) {
        put(c, 5, 7, spot);
        put(c, 9, 8, spot);
        put(c, 7, 8, spot);
        put(c, 10, 7, spot);
    }
}
void drawMushroomRed(TileCanvas& c) { mushroom(c, {200, 36, 36, 255}, {244, 244, 236, 255}, true); }
void drawMushroomBrown(TileCanvas& c) { mushroom(c, {150, 104, 64, 255}, {190, 150, 104, 255}, false); }
void drawDeadBush(TileCanvas& c) {
    clearTile(c);
    const Rgba wood = {120, 88, 48, 255}, dark = {92, 64, 34, 255};
    for (int y = 0; y < 6; ++y) put(c, 7, y, y % 2 ? wood : dark);
    for (int i = 0; i < 6; ++i) {
        put(c, 7 - i, 5 + i, wood);
        put(c, 8 + i / 1, 5 + i, i % 2 ? dark : wood);
        if (i > 1 && i < 5) put(c, 7 - i + 2, 5 + i - 1, dark);
    }
    put(c, 3, 8, dark);
    put(c, 11, 9, dark);
}
void drawSugarCane(TileCanvas& c) {
    clearTile(c);
    static const int xs[3] = {3, 8, 12};
    static const int hs[3] = {13, 15, 11};
    for (int k = 0; k < 3; ++k)
        for (int y = 0; y < hs[k]; ++y) {
            const bool node = y % 5 == 4;
            put(c, xs[k], y, node ? Rgba{96, 150, 60, 255} : Rgba{138, 196, 92, 255});
            put(c, xs[k] + 1, y, node ? Rgba{76, 120, 48, 255} : Rgba{110, 166, 72, 255});
            if (y == hs[k] - 1 || y == hs[k] - 2) put(c, xs[k] - 1 + (k % 2) * 3, y, {110, 170, 74, 255});
        }
}
void drawBerryBush(TileCanvas& c) {
    clearTile(c);
    for (int y = 0; y < 11; ++y)
        for (int x = 2; x < 14; ++x) {
            const float dx = (x - 7.5f) / 5.6f, dy = (y - 4.5f) / 5.6f;
            if (dx * dx + dy * dy > 1.0f) continue;
            if (rnd(x, y, TILE_BERRY_BUSH) > 0.9f) continue;
            const int g = 96 + static_cast<int>(rnd(x, y, TILE_BERRY_BUSH, 3) * 50);
            put(c, x, y, {40, g, 36, 255});
        }
    for (const auto& b : {std::pair<int, int>{4, 5}, {7, 7}, {10, 4}, {9, 8}, {5, 2}, {12, 6}}) {
        put(c, b.first, b.second, {196, 28, 52, 255});
        put(c, b.first + 1, b.second, {150, 20, 42, 255});
    }
}

// --- Gegenstaende
void drawBrickItem(TileCanvas& c) {
    clearTile(c);
    for (int y = 4; y < 11; ++y)
        for (int x = 2; x < 14; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_BRICK) - 0.5f) * 16);
            Rgba col = shade({166, 84, 64, 255}, d);
            if (y == 10 || x == 13) col = shade({112, 52, 40, 255}, d);
            if (y == 4 || x == 2) col = shade({196, 112, 88, 255}, d);
            put(c, x, y, col);
        }
}
void drawClayBall(TileCanvas& c) {
    clearTile(c);
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const float dx = x - 7.5f, dy = y - 7.5f;
            if (dx * dx + dy * dy < 20.0f) {
                int v = static_cast<int>((rnd(x, y, TILE_CLAY_BALL) - 0.5f) * 14) - static_cast<int>(dy * 3);
                put(c, x, y, shade({166, 172, 190, 255}, v));
            }
        }
}
void drawPaper(TileCanvas& c) {
    clearTile(c);
    for (int y = 2; y < 14; ++y)
        for (int x = 3; x < 13; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_PAPER) - 0.5f) * 10);
            Rgba col = shade({240, 238, 228, 255}, d);
            if (y == 2 || x == 3) col = shade({255, 255, 250, 255}, d);
            if (y == 13 || x == 12) col = shade({200, 198, 188, 255}, d);
            put(c, x, y, col);
        }
    for (int x = 5; x < 11; x += 1)
        if (x % 3 != 0) put(c, x, 9, {150, 150, 150, 255});
    for (int x = 5; x < 10; x += 1) put(c, x, 6, {170, 170, 170, 255});
}
void drawBook(TileCanvas& c) {
    clearTile(c);
    for (int y = 2; y < 14; ++y)
        for (int x = 3; x < 13; ++x) {
            int d = static_cast<int>((rnd(x, y, TILE_BOOK) - 0.5f) * 10);
            Rgba col = shade({120, 60, 36, 255}, d);
            if (x == 3) col = shade({84, 38, 22, 255}, d);
            if (x == 12) col = shade({236, 230, 210, 255}, d);  // Seiten
            put(c, x, y, col);
        }
    for (int y = 6; y < 10; ++y) put(c, 7, y, {214, 180, 70, 255});
}
void drawDye(TileCanvas& c, int tile, Rgba col) {
    clearTile(c);
    for (int y = 0; y < TILE_PX; ++y)
        for (int x = 0; x < TILE_PX; ++x) {
            const float dx = x - 7.5f, dy = y - 7.0f;
            if (dx * dx + dy * dy < 17.0f) {
                int v = static_cast<int>((rnd(x, y, tile) - 0.5f) * 12) - static_cast<int>(dy * 4);
                put(c, x, y, shade(col, v));
            }
        }
}
void drawBerries(TileCanvas& c) {
    clearTile(c);
    for (const auto& b : {std::pair<int, int>{5, 5}, {9, 6}, {7, 9}, {10, 10}, {4, 9}}) {
        for (int dy = 0; dy < 3; ++dy)
            for (int dx = 0; dx < 3; ++dx) {
                if (dx == 2 && dy == 2) continue;
                put(c, b.first + dx, b.second + dy, dx == 0 && dy == 2 ? Rgba{130, 16, 40, 255} : Rgba{200, 30, 56, 255});
            }
        put(c, b.first + 1, b.second + 2, {255, 140, 150, 255});
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
    make(TILE_LAVA, drawLava);
    make(TILE_OBSIDIAN, drawObsidian);
    make(TILE_CRAFT_TOP, drawCraftTop);
    make(TILE_CRAFT_SIDE, drawCraftSide);
    make(TILE_STICK, drawStick);
    {
        const Rgba mats[5] = {{214, 176, 102, 255}, {150, 150, 158, 255}, {226, 226, 232, 255}, {90, 232, 238, 255}, {250, 214, 60, 255}};
        const int tiles[5][4] = {{TILE_PICKAXE_WOOD, TILE_AXE_WOOD, TILE_SHOVEL_WOOD, TILE_SWORD_WOOD},
                                 {TILE_PICKAXE_STONE, TILE_AXE_STONE, TILE_SHOVEL_STONE, TILE_SWORD_STONE},
                                 {TILE_PICKAXE_IRON, TILE_AXE_IRON, TILE_SHOVEL_IRON, TILE_SWORD_IRON},
                                 {TILE_PICKAXE_DIAMOND, TILE_AXE_DIAMOND, TILE_SHOVEL_DIAMOND, TILE_SWORD_DIAMOND},
                                 {TILE_PICKAXE_GOLD, TILE_AXE_GOLD, TILE_SHOVEL_GOLD, TILE_SWORD_GOLD}};
        for (int mat = 0; mat < 5; ++mat)
            for (int kind = 0; kind < 4; ++kind) {
                TileCanvas cv(px, tiles[mat][kind]);
                drawTool(cv, kind, mats[mat]);
            }
    }
    make(TILE_COAL_ORE, drawCoalOre);
    make(TILE_IRON_ORE, drawIronOre);
    make(TILE_DIAMOND_ORE, drawDiamondOre);
    make(TILE_GOLD_ORE, drawGoldOre);
    make(TILE_GOLD_INGOT, drawGoldIngot);
    make(TILE_TORCH, drawTorch);
    make(TILE_FURNACE, drawFurnaceSide);
    make(TILE_FURNACE_LIT, drawFurnaceLit);
    make(TILE_FURNACE_TOP, drawFurnaceTop);
    make(TILE_COAL, drawCoalItem);
    make(TILE_IRON_INGOT, drawIronIngot);
    make(TILE_DIAMOND, drawDiamondItem);
    make(TILE_COOKED_MEAT, drawCookedMeat);
    make(TILE_BRICKS, drawBricks);
    make(TILE_STONE_BRICKS, drawStoneBricks);
    make(TILE_SANDSTONE_TOP, drawSandstoneTop);
    make(TILE_SANDSTONE_SIDE, drawSandstoneSide);
    make(TILE_GRAVEL, drawGravel);
    make(TILE_CLAY, drawClay);
    {
        const Rgba wools[6] = {{236, 236, 230, 255}, {178, 46, 40, 255}, {54, 74, 176, 255}, {86, 124, 40, 255}, {242, 202, 44, 255}, {30, 30, 34, 255}};
        const int tiles[6] = {TILE_WOOL_WHITE, TILE_WOOL_RED, TILE_WOOL_BLUE, TILE_WOOL_GREEN, TILE_WOOL_YELLOW, TILE_WOOL_BLACK};
        for (int i = 0; i < 6; ++i) {
            TileCanvas cv(px, tiles[i]);
            drawWool(cv, tiles[i], wools[i]);
        }
    }
    make(TILE_BOOKSHELF, drawBookshelf);
    make(TILE_IRON_BLOCK, drawIronBlock);
    make(TILE_GOLD_BLOCK, drawGoldBlock);
    make(TILE_DIAMOND_BLOCK, drawDiamondBlock);
    make(TILE_COAL_BLOCK, drawCoalBlock);
    make(TILE_LAMP, drawLamp);
    make(TILE_PUMPKIN_TOP, drawPumpkinTop);
    make(TILE_PUMPKIN_SIDE, drawPumpkinSide);
    make(TILE_CACTUS_TOP, drawCactusTop);
    make(TILE_CACTUS_SIDE, drawCactusSide);
    make(TILE_BIRCH_LOG_SIDE, drawBirchSide);
    make(TILE_BIRCH_LOG_TOP, drawBirchTop);
    make(TILE_BIRCH_LEAVES, drawBirchLeaves);
    make(TILE_SPRUCE_LOG_SIDE, drawSpruceSide);
    make(TILE_SPRUCE_LOG_TOP, drawSpruceTop);
    make(TILE_SPRUCE_LEAVES, drawSpruceLeaves);
    make(TILE_CHEST_TOP, drawChestTop);
    make(TILE_CHEST_SIDE, drawChestSide);
    make(TILE_BED_TOP, drawBedTop);
    make(TILE_BED_SIDE, drawBedSide);
    make(TILE_TALL_GRASS, drawTallGrass);
    make(TILE_FERN, drawFern);
    make(TILE_FLOWER_RED, drawFlowerRed);
    make(TILE_FLOWER_YELLOW, drawFlowerYellow);
    make(TILE_FLOWER_BLUE, drawFlowerBlue);
    make(TILE_FLOWER_WHITE, drawFlowerWhite);
    make(TILE_MUSHROOM_RED, drawMushroomRed);
    make(TILE_MUSHROOM_BROWN, drawMushroomBrown);
    make(TILE_DEAD_BUSH, drawDeadBush);
    make(TILE_SUGAR_CANE, drawSugarCane);
    make(TILE_BERRY_BUSH, drawBerryBush);
    make(TILE_BRICK, drawBrickItem);
    make(TILE_CLAY_BALL, drawClayBall);
    make(TILE_PAPER, drawPaper);
    make(TILE_BOOK, drawBook);
    {
        const Rgba dyes[5] = {{204, 40, 40, 255}, {240, 208, 40, 255}, {64, 96, 220, 255}, {70, 130, 40, 255}, {36, 36, 44, 255}};
        const int tiles[5] = {TILE_DYE_RED, TILE_DYE_YELLOW, TILE_DYE_BLUE, TILE_DYE_GREEN, TILE_DYE_BLACK};
        for (int i = 0; i < 5; ++i) {
            TileCanvas cv(px, tiles[i]);
            drawDye(cv, tiles[i], dyes[i]);
        }
    }
    make(TILE_BERRIES, drawBerries);
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
