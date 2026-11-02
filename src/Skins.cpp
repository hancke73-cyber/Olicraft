#include "Skins.h"

#include <algorithm>
#include <cmath>

#include "Models.h"
#include "Noise.h"

namespace oc {

namespace {

const SkinInfo SKINS[SKIN_COUNT] = {
    {"Spieler", "skin_player.png", 64, 32},
    {"Schwein", "skin_pig.png", 64, 64},
    {"Kuh", "skin_cow.png", 64, 64},
    {"Schaf", "skin_sheep.png", 64, 64},
    {"Zombie", "skin_zombie.png", 64, 32},
};

struct Rgb {
    int r, g, b;
};

struct Img {
    std::vector<uint8_t> px;
    int w, h;
    Img(int w_, int h_) : px(static_cast<size_t>(w_) * h_ * 4, 0), w(w_), h(h_) {}
    void set(int x, int y, Rgb c, int a = 255) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        size_t i = (static_cast<size_t>(y) * w + x) * 4;
        px[i] = static_cast<uint8_t>(std::max(0, std::min(255, c.r)));
        px[i + 1] = static_cast<uint8_t>(std::max(0, std::min(255, c.g)));
        px[i + 2] = static_cast<uint8_t>(std::max(0, std::min(255, c.b)));
        px[i + 3] = static_cast<uint8_t>(a);
    }
};

float rnd(int x, int y, uint32_t salt) { return (hash3(x, y, 0x5A1D, salt) & 0xFFFFu) / 65536.0f; }

// Rechteck (Teil der Zeilen y0..y1 relativ zur Flaeche) mit leichtem Rauschen fuellen
void fill(Img& img, const NetRect& r, Rgb base, int amp, uint32_t salt, int y0 = 0, int y1 = 1 << 20) {
    for (int y = r.y + std::max(0, y0); y < r.y + std::min(r.h, y1); ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            int d = static_cast<int>((rnd(x, y, salt) - 0.5f) * 2.0f * amp);
            img.set(x, y, {base.r + d, base.g + d, base.b + d});
        }
}

void fillPart(Img& img, const ModelPart& p, Rgb base, int amp, uint32_t salt) {
    for (int f = 0; f < 6; ++f) fill(img, skinNetRect(p, f), base, amp, salt + static_cast<uint32_t>(f));
}

void px(Img& img, const NetRect& r, int x, int y, Rgb c) { img.set(r.x + x, r.y + y, c); }

const ModelPart& part(SkinId id, int index) { return modelParts(id)[static_cast<size_t>(index)]; }

void paintHumanoid(Img& img, SkinId id, bool zombie) {
    const Rgb skin = zombie ? Rgb{88, 134, 68} : Rgb{198, 146, 110};
    const Rgb shirt = zombie ? Rgb{52, 150, 168} : Rgb{50, 168, 170};
    const Rgb pants = zombie ? Rgb{52, 54, 148} : Rgb{58, 62, 150};
    const ModelPart& head = part(id, 0);
    const ModelPart& body = part(id, 1);
    const ModelPart& arm = part(id, 2);
    const ModelPart& leg = part(id, 4);

    fillPart(img, head, skin, 8, 100);
    if (!zombie) {
        const Rgb hair = {92, 60, 36};
        fill(img, skinNetRect(head, 2), hair, 10, 110);            // oben
        fill(img, skinNetRect(head, 1), hair, 10, 111);            // hinten
        for (int f : {0, 4, 5}) fill(img, skinNetRect(head, f), hair, 10, 112 + static_cast<uint32_t>(f), 0, f == 0 ? 2 : 3);
    }
    const NetRect fr = skinNetRect(head, 0);  // Gesicht
    const Rgb eyeWhite = {240, 240, 240}, pupil = zombie ? Rgb{20, 26, 22} : Rgb{60, 80, 190};
    if (zombie) {
        for (int x : {1, 2, 5, 6}) {
            px(img, fr, x, 4, pupil);
            px(img, fr, x, 5, pupil);
        }
        for (int x = 2; x <= 5; ++x) px(img, fr, x, 6, {44, 66, 38});
        px(img, fr, 3, 5, {64, 100, 50});
        px(img, fr, 4, 5, {64, 100, 50});
    } else {
        px(img, fr, 1, 4, eyeWhite);
        px(img, fr, 2, 4, pupil);
        px(img, fr, 5, 4, pupil);
        px(img, fr, 6, 4, eyeWhite);
        px(img, fr, 3, 5, {170, 120, 90});
        px(img, fr, 4, 5, {170, 120, 90});
        px(img, fr, 3, 6, {150, 86, 78});
        px(img, fr, 4, 6, {150, 86, 78});
    }

    fillPart(img, body, shirt, 10, 120);
    fillPart(img, arm, skin, 8, 130);
    for (int f : {0, 1, 4, 5}) fill(img, skinNetRect(arm, f), shirt, 8, 140 + static_cast<uint32_t>(f), 0, 4);  // Aermel
    fill(img, skinNetRect(arm, 2), shirt, 8, 150);
    fillPart(img, leg, pants, 8, 160);
    for (int f : {0, 1, 4, 5}) fill(img, skinNetRect(leg, f), {72, 72, 78}, 6, 170 + static_cast<uint32_t>(f), 10);  // Schuhe
    fill(img, skinNetRect(leg, 3), {72, 72, 78}, 6, 180);
}

void paintPig(Img& img) {
    const Rgb pink = {236, 152, 162};
    fillPart(img, part(SKIN_PIG, 0), pink, 9, 200);                 // Koerper
    fillPart(img, part(SKIN_PIG, 1), pink, 9, 210);                 // Kopf
    fillPart(img, part(SKIN_PIG, 2), {222, 138, 148}, 9, 220);      // Beine
    const ModelPart& head = part(SKIN_PIG, 1);
    const NetRect fr = skinNetRect(head, 0);
    for (int y = 4; y <= 6; ++y)
        for (int x = 2; x <= 5; ++x) px(img, fr, x, y, {246, 180, 186});  // Schnauze
    px(img, fr, 3, 5, {150, 70, 80});
    px(img, fr, 4, 5, {150, 70, 80});
    px(img, fr, 1, 2, {30, 20, 24});
    px(img, fr, 6, 2, {30, 20, 24});
}

void paintCow(Img& img) {
    Noise n(31);
    auto patchy = [&](const ModelPart& p) {
        for (int f = 0; f < 6; ++f) {
            const NetRect r = skinNetRect(p, f);
            for (int y = r.y; y < r.y + r.h; ++y)
                for (int x = r.x; x < r.x + r.w; ++x) {
                    float v = n.fbm2(x * 0.22f + 5.0f, y * 0.22f + 9.0f, 2);
                    int d = static_cast<int>((rnd(x, y, 300) - 0.5f) * 14);
                    if (v > 0.12f) img.set(x, y, {58 + d, 44 + d, 40 + d});
                    else img.set(x, y, {228 + d, 226 + d, 222 + d});
                }
        }
    };
    patchy(part(SKIN_COW, 0));  // Koerper
    patchy(part(SKIN_COW, 4));  // Beine
    const ModelPart& head = part(SKIN_COW, 1);
    fillPart(img, head, {80, 56, 44}, 8, 310);
    const NetRect fr = skinNetRect(head, 0);
    for (int y = 5; y <= 7; ++y)
        for (int x = 1; x <= 6; ++x) px(img, fr, x, y, {206, 190, 176});  // Maul
    px(img, fr, 2, 6, {110, 80, 70});
    px(img, fr, 5, 6, {110, 80, 70});
    px(img, fr, 1, 3, {20, 16, 16});
    px(img, fr, 6, 3, {20, 16, 16});
    fillPart(img, part(SKIN_COW, 2), {206, 196, 170}, 6, 320);  // Hoerner
}

void paintSheep(Img& img) {
    fillPart(img, part(SKIN_SHEEP, 0), {232, 228, 218}, 16, 400);  // Wolle
    fillPart(img, part(SKIN_SHEEP, 2), {232, 228, 218}, 16, 410);  // Wollkappe
    const ModelPart& head = part(SKIN_SHEEP, 1);
    fillPart(img, head, {128, 108, 92}, 8, 420);
    const NetRect fr = skinNetRect(head, 0);
    px(img, fr, 1, 3, {240, 240, 240});
    px(img, fr, 1, 4, {20, 16, 16});
    px(img, fr, 4, 3, {240, 240, 240});
    px(img, fr, 4, 4, {20, 16, 16});
    for (int x = 2; x <= 3; ++x) px(img, fr, x, 5, {90, 70, 62});
    fillPart(img, part(SKIN_SHEEP, 3), {178, 158, 138}, 10, 430);  // Beine
}

}  // namespace

const SkinInfo& skinInfo(SkinId id) { return SKINS[id]; }

std::vector<uint8_t> generateSkinPixels(SkinId id) {
    const SkinInfo& si = skinInfo(id);
    Img img(si.width, si.height);
    switch (id) {
        case SKIN_PLAYER: paintHumanoid(img, id, false); break;
        case SKIN_ZOMBIE: paintHumanoid(img, id, true); break;
        case SKIN_PIG: paintPig(img); break;
        case SKIN_COW: paintCow(img); break;
        case SKIN_SHEEP: paintSheep(img); break;
        default: break;
    }
    return img.px;
}

}  // namespace oc
