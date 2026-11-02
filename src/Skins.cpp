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
    {"Dorfbewohner Bauer", "skin_villager.png", 64, 32},
    {"Dorfbewohner Bibliothekar", "skin_villager_librarian.png", 64, 32},
    {"Dorfbewohner Schmied", "skin_villager_smith.png", 64, 32},
    {"Dorfbewohner Steinmetz", "skin_villager_mason.png", 64, 32},
    {"Dorfbewohner Schaefer", "skin_villager_shepherd.png", 64, 32},
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

struct HumanPalette {
    const char* name;
    Rgb skin, hair, shirt, pants, eye;
};

const HumanPalette PLAYER_PALETTES[PLAYER_SKIN_SLOTS] = {
    {"Standard", {198, 146, 110}, {92, 60, 36}, {50, 168, 170}, {58, 62, 150}, {60, 80, 190}},
    {"Rot", {198, 146, 110}, {40, 30, 24}, {190, 50, 50}, {50, 50, 60}, {70, 120, 70}},
    {"Gruen", {190, 140, 104}, {200, 170, 80}, {60, 150, 70}, {90, 70, 50}, {60, 80, 190}},
    {"Violett", {198, 146, 110}, {30, 30, 30}, {130, 70, 170}, {50, 50, 90}, {120, 80, 40}},
    {"Orange", {206, 156, 120}, {150, 50, 30}, {230, 140, 40}, {60, 90, 140}, {70, 110, 60}},
    {"Hell", {235, 190, 160}, {230, 200, 110}, {240, 240, 240}, {80, 110, 170}, {60, 130, 200}},
    {"Dunkel", {120, 80, 56}, {20, 16, 14}, {40, 40, 48}, {30, 30, 36}, {90, 60, 30}},
    {"Rosa", {214, 160, 128}, {110, 70, 50}, {236, 130, 170}, {120, 90, 160}, {60, 80, 190}},
};

void paintHumanoid(Img& img, SkinId id, bool zombie, const HumanPalette& pal = PLAYER_PALETTES[0]) {
    const Rgb skin = zombie ? Rgb{88, 134, 68} : pal.skin;
    const Rgb shirt = zombie ? Rgb{52, 150, 168} : pal.shirt;
    const Rgb pants = zombie ? Rgb{52, 54, 148} : pal.pants;
    const ModelPart& head = part(id, 0);
    const ModelPart& body = part(id, 1);
    const ModelPart& arm = part(id, 2);
    const ModelPart& leg = part(id, 4);

    fillPart(img, head, skin, 8, 100);
    if (!zombie) {
        const Rgb hair = pal.hair;
        fill(img, skinNetRect(head, 2), hair, 10, 110);            // oben
        fill(img, skinNetRect(head, 1), hair, 10, 111);            // hinten
        for (int f : {0, 4, 5}) fill(img, skinNetRect(head, f), hair, 10, 112 + static_cast<uint32_t>(f), 0, f == 0 ? 2 : 3);
    }
    const NetRect fr = skinNetRect(head, 0);  // Gesicht
    const Rgb eyeWhite = {240, 240, 240}, pupil = zombie ? Rgb{20, 26, 22} : pal.eye;
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

void paintVillager(Img& img, SkinId id) {
    struct Look {
        Rgb robe, robeDark, belt, hat, skin;
    };
    static const Look looks[5] = {
        {{112, 80, 52}, {88, 62, 40}, {70, 50, 34}, {200, 170, 90}, {190, 140, 108}},    // Bauer: braun, Strohhut
        {{232, 230, 222}, {196, 194, 188}, {70, 90, 160}, {170, 170, 176}, {196, 150, 118}},  // Bibliothekar: weiss, blauer Guertel
        {{84, 84, 92}, {60, 60, 68}, {40, 36, 34}, {36, 32, 30}, {180, 130, 100}},       // Schmied: dunkle Schuerze
        {{136, 136, 142}, {104, 104, 110}, {90, 80, 70}, {118, 118, 124}, {188, 142, 112}},  // Steinmetz: grau
        {{100, 124, 66}, {78, 98, 52}, {120, 90, 56}, {214, 208, 190}, {194, 148, 116}},  // Schaefer: gruen, helle Kappe
    };
    const Look& L = looks[std::max(0, std::min(4, static_cast<int>(id) - static_cast<int>(SKIN_VILLAGER)))];
    const ModelPart& head = part(SKIN_VILLAGER, 0);
    const ModelPart& body = part(SKIN_VILLAGER, 1);
    const ModelPart& arm = part(SKIN_VILLAGER, 2);
    const ModelPart& leg = part(SKIN_VILLAGER, 4);
    const ModelPart& nose = part(SKIN_VILLAGER, 6);
    fillPart(img, head, L.skin, 8, 600);
    // Kopfbedeckung: Oberseite und Hinterkopf, vorn ein Streifen
    fill(img, skinNetRect(head, 2), L.hat, 8, 604);
    fill(img, skinNetRect(head, 1), L.hat, 8, 605);
    fill(img, skinNetRect(head, 0), L.hat, 8, 606, 0, 2);
    fill(img, skinNetRect(head, 4), L.hat, 8, 607, 0, 2);
    fill(img, skinNetRect(head, 5), L.hat, 8, 608, 0, 2);
    fillPart(img, nose, {static_cast<int>(L.skin.r * 0.87), static_cast<int>(L.skin.g * 0.83), static_cast<int>(L.skin.b * 0.8)}, 6, 610);
    fillPart(img, body, L.robe, 10, 620);
    fillPart(img, arm, L.robe, 10, 630);
    fillPart(img, leg, L.robeDark, 8, 640);
    for (int f : {0, 1, 4, 5}) fill(img, skinNetRect(body, f), L.belt, 6, 650 + static_cast<uint32_t>(f), 8, 10);
    if (id == SKIN_VILLAGER_SMITH) {  // Schuerze: dunkle Flaeche vorn
        fill(img, skinNetRect(body, 0), L.belt, 6, 660, 2, 12);
        fill(img, skinNetRect(arm, 0), L.robeDark, 6, 661, 8, 12);
    }
    if (id == SKIN_VILLAGER_LIBRARIAN) {  // Stola
        const NetRect fr = skinNetRect(body, 0);
        for (int y = 0; y < fr.h; ++y) px(img, fr, fr.w / 2, y, L.belt);
    }
    const NetRect fr = skinNetRect(head, 0);
    const Rgb white = {240, 240, 240}, green = {40, 120, 70}, brow = {70, 50, 40};
    for (int x = 0; x < 8; ++x) px(img, fr, x, 2, brow);
    for (int x : {1, 2, 5, 6}) px(img, fr, x, 3, x == 2 || x == 5 ? green : white);
    for (int x : {1, 2, 5, 6}) px(img, fr, x, 4, x == 2 || x == 5 ? green : white);
    for (int x = 2; x <= 5; ++x) px(img, fr, x, 7, {140, 96, 76});
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

const char* playerSkinName(int slot) { return PLAYER_PALETTES[std::max(0, std::min(PLAYER_SKIN_SLOTS - 1, slot))].name; }

std::vector<uint8_t> generatePlayerSkinPixels(int slot) {
    Img img(64, 32);
    paintHumanoid(img, SKIN_PLAYER, false, PLAYER_PALETTES[std::max(0, std::min(PLAYER_SKIN_SLOTS - 1, slot))]);
    return img.px;
}

std::vector<uint8_t> generateSkinPixels(SkinId id) {
    const SkinInfo& si = skinInfo(id);
    Img img(si.width, si.height);
    switch (id) {
        case SKIN_PLAYER: paintHumanoid(img, id, false); break;
        case SKIN_ZOMBIE: paintHumanoid(img, id, true); break;
        case SKIN_VILLAGER:
        case SKIN_VILLAGER_LIBRARIAN:
        case SKIN_VILLAGER_SMITH:
        case SKIN_VILLAGER_MASON:
        case SKIN_VILLAGER_SHEPHERD: paintVillager(img, id); break;
        case SKIN_PIG: paintPig(img); break;
        case SKIN_COW: paintCow(img); break;
        case SKIN_SHEEP: paintSheep(img); break;
        default: break;
    }
    return img.px;
}

}  // namespace oc
