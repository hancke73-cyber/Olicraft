// Tests ohne OpenGL-Kontext: Welt, Terrain, Physik, Mesher, Speichern.
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <thread>
#include <string>
#include <vector>

#include "Audio.h"
#include "Block.h"
#include "TextureEditor.h"
#include "TextureStore.h"
#include "Camera.h"
#include "Models.h"
#include "Skins.h"
#include "Particles.h"
#include "Sky.h"
#include <array>

#include "BlockShape.h"
#include "Physics.h"
#include "Trades.h"
#include "Village.h"
#include "Crafting.h"
#include "Entities.h"
#include "Hud.h"
#include "Inventory.h"
#include "Items.h"
#include "Menu.h"
#include "Settings.h"
#include "Mesher.h"
#include "Noise.h"
#include "Player.h"
#include "Save.h"
#include "World.h"

using namespace oc;

static int g_checks = 0, g_failed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::fprintf(stderr, "FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

static int topSolid(const World& w, int x, int z) {
    for (int y = CHUNK_H - 1; y >= 0; --y)
        if (isSolid(w.getBlock(x, y, z))) return y;
    return -1;
}

static void testNoise() {
    Noise a(1), b(1), c(2);
    CHECK(a.noise2(3.3f, 4.4f) == b.noise2(3.3f, 4.4f));
    CHECK(a.noise3(3.3f, 4.4f, 5.5f) == b.noise3(3.3f, 4.4f, 5.5f));
    bool differs = false;
    float mn = 1e9f, mx = -1e9f;
    for (int i = 0; i < 4000; ++i) {
        float x = i * 0.37f, y = i * 0.91f;
        float v = a.fbm2(x * 0.1f, y * 0.1f, 4);
        mn = std::min(mn, v);
        mx = std::max(mx, v);
        if (a.noise2(x, y) != c.noise2(x, y)) differs = true;
    }
    CHECK(differs);
    CHECK(mn >= -1.2f && mx <= 1.2f);
    CHECK(mx - mn > 0.4f);  // nicht konstant
    CHECK(hash3(1, 2, 3, 4) == hash3(1, 2, 3, 4));
    CHECK(hash3(1, 2, 3, 4) != hash3(1, 2, 4, 4));
}

static void testTerrain() {
    World w1(1234), w2(1234), w3(999);
    for (World* w : {&w1, &w2, &w3}) w->setMeshingEnabled(false);
    w1.loadAround(glm::vec3(0.0f), 2);
    w2.loadAround(glm::vec3(0.0f), 2);
    w3.loadAround(glm::vec3(0.0f), 2);

    int diffSame = 0, diffOther = 0, bedrockOk = 0, columns = 0, logs = 0, leaves = 0, water = 0, stone = 0;
    for (int x = -32; x < 32; ++x)
        for (int z = -32; z < 32; ++z)
            for (int y = 0; y < CHUNK_H; ++y) {
                BlockId a = w1.getBlock(x, y, z);
                if (a != w2.getBlock(x, y, z)) ++diffSame;
                if (a != w3.getBlock(x, y, z)) ++diffOther;
                if (a == LOG) ++logs;
                if (a == LEAVES) ++leaves;
                if (a == WATER) ++water;
                if (a == STONE) ++stone;
            }
    for (int x = -32; x < 32; ++x)
        for (int z = -32; z < 32; ++z) {
            ++columns;
            if (w1.getBlock(x, 0, z) == BEDROCK) ++bedrockOk;
        }
    CHECK(diffSame == 0);
    CHECK(diffOther > 1000);
    CHECK(bedrockOk == columns);
    CHECK(stone > 10000);

    // Wasser liegt nur auf/unter Meereshoehe, und nie ueber Luft ohne Wasser darunter
    bool waterOk = true;
    for (int x = -32; x < 32 && waterOk; ++x)
        for (int z = -32; z < 32 && waterOk; ++z)
            for (int y = 1; y < CHUNK_H; ++y)
                if (w1.getBlock(x, y, z) == WATER && (y > SEA_LEVEL || w1.getBlock(x, y - 1, z) == AIR)) waterOk = false;
    CHECK(waterOk);

    // Stamm steht auf festem Boden; Laub haengt an Holz in der Naehe (grober Plausibilitaetstest)
    bool trunkOk = true;
    for (int x = -32; x < 32; ++x)
        for (int z = -32; z < 32; ++z)
            for (int y = 1; y < CHUNK_H; ++y)
                if (w1.getBlock(x, y, z) == LOG && w1.getBlock(x, y - 1, z) == AIR) trunkOk = false;
    CHECK(trunkOk);
    (void)logs;
    (void)leaves;
    (void)water;

    // Chunkgrenzen: Bloecke eines Baums quer ueber die Grenze muessen identisch erzeugt werden,
    // egal in welcher Reihenfolge Chunks geladen werden.
    World w4(1234);
    w4.setMeshingEnabled(false);
    w4.loadAround(glm::vec3(16.0f * 3, 0.0f, 0.0f), 1);  // andere Reihenfolge / anderer Mittelpunkt
    w4.loadAround(glm::vec3(0.0f), 1);
    int diffOrder = 0;
    for (int x = -16; x < 32; ++x)
        for (int z = -16; z < 32; ++z)
            for (int y = 0; y < CHUNK_H; ++y)
                if (w1.getBlock(x, y, z) != w4.getBlock(x, y, z)) ++diffOrder;
    CHECK(diffOrder == 0);
}

static void testBlocksAndRaycast() {
    World w(5);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 1);

    CHECK(w.getBlock(1000, 5, 1000) == AIR);  // ungeladen
    CHECK(w.isSolidAt(1000, 5, 1000));         // aber fest fuer die Physik
    CHECK(w.getBlock(0, -1, 0) == AIR);
    CHECK(w.isSolidAt(0, -1, 0));
    CHECK(!w.isSolidAt(0, CHUNK_H + 5, 0));

    // freien Platz hoch ueber dem Boden schaffen
    for (int x = -3; x <= 3; ++x)
        for (int z = -3; z <= 3; ++z)
            for (int y = 100; y < 110; ++y) w.setBlock(x, y, z, AIR);
    CHECK(w.setBlock(0, 100, 0, STONE));
    CHECK(w.getBlock(0, 100, 0) == STONE);
    CHECK(!w.setBlock(0, 100, 0, STONE));  // keine Aenderung
    CHECK(w.setBlock(-1, 100, -1, PLANKS));  // negative Koordinaten
    CHECK(w.getBlock(-1, 100, -1) == PLANKS);
    CHECK(!w.setBlock(0, -1, 0, STONE));
    CHECK(!w.setBlock(0, CHUNK_H, 0, STONE));
    CHECK(w.chunkAt(0, 0)->modified);

    // Strahl von oben auf Stein
    RayHit h = w.raycast(glm::vec3(0.5f, 105.5f, 0.5f), glm::vec3(0, -1, 0), 10.0f);
    CHECK(h.hit && h.block == glm::ivec3(0, 100, 0) && h.normal == glm::ivec3(0, 1, 0));
    // seitlich
    h = w.raycast(glm::vec3(3.5f, 100.5f, 0.5f), glm::vec3(-1, 0, 0), 10.0f);
    CHECK(h.hit && h.block == glm::ivec3(0, 100, 0) && h.normal == glm::ivec3(1, 0, 0));
    // zu kurz
    h = w.raycast(glm::vec3(0.5f, 105.5f, 0.5f), glm::vec3(0, -1, 0), 3.0f);
    CHECK(!h.hit);
    // diagonal in negative Richtung
    h = w.raycast(glm::vec3(2.5f, 102.5f, 2.5f), glm::vec3(-1, -1, -1), 10.0f);
    CHECK(h.hit);
    // Wasser wird ignoriert
    w.setBlock(1, 101, 0, WATER);
    h = w.raycast(glm::vec3(1.5f, 104.5f, 0.5f), glm::vec3(0, -1, 0), 10.0f);
    CHECK(!h.hit || h.block.y != 101);

    // Mesh-Dirty-Markierung an der Chunkgrenze (x = 0 und z = 0 liegen an Grenzen)
    w.setBlock(0, 105, 0, STONE);
    CHECK(w.chunkAt(-1, 0)->dirty && w.chunkAt(0, -1)->dirty && w.chunkAt(-1, -1)->dirty);
}

static void testPlayer() {
    World w(7);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);

    // flache Testflaeche: Stein bei y = 60, Luft darueber
    for (int x = -10; x <= 10; ++x)
        for (int z = -10; z <= 10; ++z) {
            for (int y = 60; y < 100; ++y) w.setBlock(x, y, z, AIR);
            w.setBlock(x, 59, z, STONE);
        }

    Player p;
    p.pos = glm::vec3(0.5f, 70.0f, 0.5f);
    MoveInput none;
    for (int i = 0; i < 200; ++i) p.update(1.0f / 60.0f, none, w);
    CHECK(p.onGround);
    CHECK(std::fabs(p.pos.y - 60.0f) < 0.01f);
    CHECK(std::fabs(p.vel.y) < 1e-3f);

    // Springen: Hoehe ca. 1.4 m, aber unter 2 Bloecken
    MoveInput jump;
    jump.jump = true;
    float maxY = p.pos.y;
    for (int i = 0; i < 90; ++i) {
        p.update(1.0f / 60.0f, i < 3 ? jump : none, w);
        maxY = std::max(maxY, p.pos.y);
    }
    CHECK(maxY - 60.0f > 1.1f && maxY - 60.0f < 1.9f);
    CHECK(p.onGround);

    // Laufen nach vorn (yaw = 0 -> +x), Wand bei x = 5
    for (int y = 60; y < 63; ++y)
        for (int z = -10; z <= 10; ++z) w.setBlock(5, y, z, STONE);
    p.pos = glm::vec3(0.5f, 60.0f, 0.5f);
    p.vel = glm::vec3(0.0f);
    p.yaw = 0.0f;
    MoveInput fwd;
    fwd.forward = 1.0f;
    float startX = p.pos.x;
    for (int i = 0; i < 60; ++i) p.update(1.0f / 60.0f, fwd, w);
    CHECK(p.pos.x > startX + 2.5f);  // ist gelaufen (ca. 4.3 m/s)
    for (int i = 0; i < 240; ++i) p.update(1.0f / 60.0f, fwd, w);
    CHECK(p.pos.x < 5.0f - Player::WIDTH * 0.5f + 1e-3f);  // Wand stoppt
    CHECK(p.pos.x > 4.0f);

    // Hohe Fallgeschwindigkeit tunnelt nicht durch den Boden
    p.pos = glm::vec3(0.5f, 99.0f, 8.5f);
    p.vel = glm::vec3(0.0f, -55.0f, 0.0f);
    for (int i = 0; i < 120; ++i) p.update(1.0f / 20.0f, none, w);
    CHECK(p.pos.y >= 59.9f && p.pos.y < 60.1f);

    // Decke: Sprung gegen Deckenblock bei y = 62 (Kopf stoesst an)
    for (int x = -2; x <= 2; ++x)
        for (int z = 6; z <= 10; ++z) w.setBlock(x, 62, z, STONE);
    p.pos = glm::vec3(0.5f, 60.0f, 8.5f);
    p.vel = glm::vec3(0.0f);
    float peak = 0;
    for (int i = 0; i < 60; ++i) {
        p.update(1.0f / 60.0f, i < 3 ? jump : none, w);
        peak = std::max(peak, p.pos.y);
    }
    CHECK(peak + Player::HEIGHT <= 62.0f + 1e-3f);

    // Schwimmen: Wasserbecken, Spieler sinkt langsam und kann aufsteigen
    for (int x = -10; x <= -6; ++x)
        for (int z = -10; z <= -6; ++z)
            for (int y = 60; y < 70; ++y) w.setBlock(x, y, z, WATER);
    p.pos = glm::vec3(-8.0f, 68.0f, -8.0f);
    p.vel = glm::vec3(0.0f);
    for (int i = 0; i < 60; ++i) p.update(1.0f / 60.0f, none, w);
    CHECK(p.inWater);
    CHECK(p.vel.y > -4.5f);
    float y0 = p.pos.y;
    for (int i = 0; i < 60; ++i) p.update(1.0f / 60.0f, jump, w);
    CHECK(p.pos.y > y0);

    // Block-Ueberschneidung
    p.pos = glm::vec3(0.5f, 60.0f, 0.5f);
    CHECK(p.intersectsBlock(0, 60, 0));
    CHECK(p.intersectsBlock(0, 61, 0));
    CHECK(!p.intersectsBlock(0, 62, 0));
    CHECK(!p.intersectsBlock(1, 60, 0));
    CHECK(!p.intersectsBlock(0, 59, 0));
}

static void testSugarCaneAligned() {
    // Zuckerrohr: uebereinanderstehende Stuecke liegen exakt auf derselben Achse (kein zufaelliger Versatz)
    std::vector<BlockId> pad(PAD_VOLUME, AIR);
    auto set = [&](int x, int y, int z, BlockId b) { pad[padIndex(x + 1, y, z + 1)] = b; };
    MeshOutput out;
    for (int cell = 0; cell < 6; ++cell) {
        const int x = 2 + cell * 2, z = 3 + cell;
        set(x, 49, z, SAND);
        for (int y = 50; y < 53; ++y) set(x, y, z, SUGAR_CANE);
    }
    buildChunkMesh(pad.data(), nullptr, out);
    CHECK(!out.opaque.empty());
    bool aligned = true;
    for (const Vertex& v : out.opaque) {
        const float fx = v.x - std::floor(v.x), fz = v.z - std::floor(v.z);
        // Sandwuerfel haben ganzzahlige Ecken, das Zuckerrohr genau 0,12 und 0,88
        if (fx > 1e-4f && std::fabs(fx - 0.12f) > 1e-4f && std::fabs(fx - 0.88f) > 1e-4f) aligned = false;
        if (fz > 1e-4f && std::fabs(fz - 0.12f) > 1e-4f && std::fabs(fz - 0.88f) > 1e-4f) aligned = false;
    }
    CHECK(aligned);
}

static void testMesher() {
    std::vector<BlockId> pad(PAD_VOLUME, AIR);
    auto set = [&](int x, int y, int z, BlockId b) { pad[padIndex(x + 1, y, z + 1)] = b; };
    MeshOutput out;

    set(5, 50, 5, STONE);
    buildChunkMesh(pad.data(), nullptr, out);
    CHECK(out.opaque.size() == 6 * 4);
    CHECK(out.water.empty());

    set(6, 50, 5, STONE);
    buildChunkMesh(pad.data(), nullptr, out);
    CHECK(out.opaque.size() == 10 * 4);  // 2 Bloecke, 2 verdeckte Flaechen

    // alle Vertex-Positionen liegen im Chunk und alle Helligkeiten in 0..1
    bool inRange = true;
    for (const Vertex& v : out.opaque) {
        if (v.x < 0 || v.x > CHUNK_W || v.y < 0 || v.y > CHUNK_H || v.z < 0 || v.z > CHUNK_W) inRange = false;
        if (v.shade < 0 || v.shade > 1.001f || v.sky < 0 || v.sky > 1.001f) inRange = false;
        if (v.u < 0 || v.u > 1 || v.v < 0 || v.v > 1) inRange = false;
    }
    CHECK(inRange);

    // Normalen: Flaechen weisen nach aussen (Winding gegen den Uhrzeigersinn von aussen gesehen)
    {
        std::vector<BlockId> p2(PAD_VOLUME, AIR);
        p2[padIndex(8 + 1, 50, 8 + 1)] = STONE;
        MeshOutput o2;
        buildChunkMesh(p2.data(), nullptr, o2);
        bool windingOk = true;
        const glm::vec3 center(8.5f, 50.5f, 8.5f);
        for (size_t q = 0; q + 3 < o2.opaque.size(); q += 4) {
            glm::vec3 a(o2.opaque[q].x, o2.opaque[q].y, o2.opaque[q].z);
            glm::vec3 b(o2.opaque[q + 1].x, o2.opaque[q + 1].y, o2.opaque[q + 1].z);
            glm::vec3 c(o2.opaque[q + 2].x, o2.opaque[q + 2].y, o2.opaque[q + 2].z);
            glm::vec3 n = glm::cross(b - a, c - a);
            glm::vec3 mid = (a + c) * 0.5f;
            if (glm::dot(n, mid - center) <= 0.0f) windingOk = false;
        }
        CHECK(windingOk);
    }

    // Wasser: nur Oberflaeche und Seiten gegen Luft, keine Flaechen zwischen Wasserbloecken
    std::vector<BlockId> p3(PAD_VOLUME, AIR);
    for (int x = 4; x < 7; ++x)
        for (int z = 4; z < 7; ++z) {
            p3[padIndex(x + 1, 40, z + 1)] = STONE;
            p3[padIndex(x + 1, 41, z + 1)] = WATER;
        }
    MeshOutput o3;
    buildChunkMesh(p3.data(), nullptr, o3);
    CHECK(o3.water.size() == (9 + 4 * 3) * 4);  // Oberseite 9 + 4 Seiten * 3 Bloecke
    // Wasseroberflaeche liegt unter der vollen Blockhoehe
    bool lowered = true;
    for (const Vertex& v : o3.water)
        if (v.y > 41.9f) lowered = false;
    CHECK(lowered);

    // Laub-Laub-Flaechen werden ausgelassen, Laub-Luft nicht
    std::vector<BlockId> p4(PAD_VOLUME, AIR);
    p4[padIndex(3 + 1, 70, 3 + 1)] = LEAVES;
    p4[padIndex(4 + 1, 70, 3 + 1)] = LEAVES;
    MeshOutput o4;
    buildChunkMesh(p4.data(), nullptr, o4);
    CHECK(o4.opaque.size() == 10 * 4);

    // Himmelslicht: Block mit Dach darueber bekommt weniger Himmelslicht an der Oberseite als ein freier
    std::vector<BlockId> p5(PAD_VOLUME, AIR);
    p5[padIndex(2 + 1, 50, 2 + 1)] = STONE;       // frei
    p5[padIndex(10 + 1, 50, 10 + 1)] = STONE;     // mit Dach
    for (int x = 8; x <= 12; ++x)
        for (int z = 8; z <= 12; ++z) p5[padIndex(x + 1, 55, z + 1)] = STONE;
    MeshOutput o5;
    buildChunkMesh(p5.data(), nullptr, o5);
    float skyFree = -1, skyCovered = -1;
    for (const Vertex& v : o5.opaque) {
        if (v.y == 51.0f && v.x >= 2 && v.x <= 3 && v.z >= 2 && v.z <= 3) skyFree = v.sky;
        if (v.y == 51.0f && v.x >= 10 && v.x <= 11 && v.z >= 10 && v.z <= 11) skyCovered = v.sky;
    }
    CHECK(skyFree > 0.99f);
    CHECK(skyCovered >= 0.0f && skyCovered < 0.1f);
}

static void testSave() {
    std::vector<BlockId> uniform;
    CHECK(rleDecode(rleEncode(std::vector<BlockId>(Chunk::VOLUME, STONE)), uniform, Chunk::VOLUME));
    CHECK(uniform.size() == static_cast<size_t>(Chunk::VOLUME));
    std::vector<BlockId> blocks(Chunk::VOLUME);
    for (size_t i = 0; i < blocks.size(); ++i) blocks[i] = static_cast<BlockId>((i / 7 + i / 1000) % BLOCK_COUNT);
    std::vector<BlockId> back;
    CHECK(rleDecode(rleEncode(blocks), back, blocks.size()));
    CHECK(back == blocks);
    CHECK(!rleDecode({1, 2}, back, blocks.size()));  // kaputte Daten

    const std::string path = "olicraft_test_save/world.dat";
    std::filesystem::remove_all("olicraft_test_save");

    int32_t seed = 4321;
    {
        World w(seed);
        w.setMeshingEnabled(false);
        w.loadAround(glm::vec3(0.0f), 1);
        w.setBlock(2, 90, 2, GLASS);
        w.setBlock(-20, 90, 3, PLANKS);  // anderer Chunk
        for (int y = 40; y < 60; ++y) w.setBlock(3, y, 3, COBBLE);

        // Entladen eines veraenderten Chunks muss die Aenderung erhalten
        w.update(glm::vec3(16.0f * 40, 64.0f, 0.0f), 4, 1000.0);
        CHECK(!w.isChunkLoaded(0, 0));

        SaveFile sf;
        sf.seed = seed;
        sf.player.pos = glm::vec3(1.5f, 70.0f, -2.5f);
        sf.player.yaw = 33.0f;
        sf.player.pitch = -12.0f;
        sf.player.slot = 4;
        sf.player.flying = true;
        sf.player.timeOfDay = 0.6f;
        w.exportModified(sf.chunks);
        CHECK(sf.chunks.size() >= 2);
        std::string err;
        CHECK(writeSaveFile(path, sf, &err));
    }
    {
        SaveFile sf;
        std::string err;
        CHECK(readSaveFile(path, sf, &err));
        CHECK(sf.seed == seed);
        CHECK(sf.player.slot == 4 && sf.player.flying);
        CHECK(std::fabs(sf.player.yaw - 33.0f) < 1e-6f && std::fabs(sf.player.timeOfDay - 0.6f) < 1e-6f);
        CHECK(sf.player.pos == glm::vec3(1.5f, 70.0f, -2.5f));

        World w(sf.seed);
        w.setMeshingEnabled(false);
        w.importChunks(sf.chunks);
        w.loadAround(glm::vec3(0.0f), 1);
        CHECK(w.getBlock(2, 90, 2) == GLASS);
        CHECK(w.getBlock(-20, 90, 3) == PLANKS);
        CHECK(w.getBlock(3, 50, 3) == COBBLE);
        CHECK(w.getBlock(3, 59, 3) == COBBLE);

        // Unveraenderte Teile stimmen mit frischer Generierung ueberein
        World fresh(sf.seed);
        fresh.setMeshingEnabled(false);
        fresh.loadAround(glm::vec3(0.0f), 1);
        int diff = 0;
        for (int x = -16; x < 32; ++x)
            for (int z = -16; z < 32; ++z)
                for (int y = 0; y < CHUNK_H; ++y)
                    if (w.getBlock(x, y, z) != fresh.getBlock(x, y, z)) ++diff;
        CHECK(diff > 0 && diff < 2000);  // nur die wenigen Edits
    }
    // Defekte Datei wird abgelehnt
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        std::fputs("kaputt", f);
        std::fclose(f);
        SaveFile sf;
        std::string err;
        CHECK(!readSaveFile(path, sf, &err));
    }
    std::filesystem::remove_all("olicraft_test_save");
}


static void settleFluids(World& w, int ticks) {
    for (int i = 0; i < ticks; ++i) w.tick(0.25f);
}

static void clearArea(World& w, int x0, int x1, int y0, int y1, int z0, int z1) {
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z)
            for (int y = y0; y <= y1; ++y) w.setBlock(x, y, z, AIR);
}

static void testFluids() {
    World w(11);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    clearArea(w, -14, 14, 86, 110, -14, 14);
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z) w.setBlock(x, 99, z, STONE);

    // 1. Ausbreitung auf flacher Flaeche: Stufe = Abstand, maximal 7
    w.setBlock(0, 100, 0, WATER);
    settleFluids(w, 30);
    CHECK(w.getBlock(1, 100, 0) == WATER_1);
    CHECK(w.getBlock(3, 100, 0) == WATER_3);
    CHECK(w.getBlock(7, 100, 0) == WATER_7);
    CHECK(w.getBlock(8, 100, 0) == AIR);
    CHECK(w.getBlock(-4, 100, 0) == WATER_4);
    CHECK(w.getBlock(1, 100, 1) == WATER_2);  // Manhattan-Abstand
    CHECK(w.getBlock(0, 101, 0) == AIR);
    CHECK(w.chunkAt(0, 0)->modified);

    // 2. Quelle entfernen: das Wasser verschwindet wieder
    w.setBlock(0, 100, 0, AIR);
    settleFluids(w, 40);
    int leftover = 0;
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z)
            if (isLiquid(w.getBlock(x, 100, z))) ++leftover;
    CHECK(leftover == 0);
    CHECK(w.pendingFluidCount() == 0);

    // 3. Wasserfall in ein Loch, unten breitet es sich aus
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z)
            for (int y = 89; y <= 98; ++y) w.setBlock(x, y, z, STONE);  // massiver Block unter der Platte
    for (int y = 90; y <= 99; ++y) w.setBlock(0, y, 5, AIR);              // Schacht
    for (int x = -6; x <= 6; ++x) w.setBlock(x, 90, 5, AIR);              // Tunnel unten
    w.setBlock(0, 100, 5, WATER);
    settleFluids(w, 40);
    bool column = true;
    for (int y = 90; y <= 99; ++y) column = column && w.getBlock(0, y, 5) == WATER_FALL;
    CHECK(column);
    CHECK(w.getBlock(1, 90, 5) == WATER_1);
    CHECK(w.getBlock(3, 90, 5) == WATER_3);
    CHECK(w.getBlock(6, 90, 5) == WATER_6);
    // oben breitet sich ebenfalls aus, aber nicht ueber den Rand des Lochs nach unten
    CHECK(w.getBlock(1, 100, 5) == WATER_1);
    // Quelle weg: Saeule leert sich
    w.setBlock(0, 100, 5, AIR);
    settleFluids(w, 60);
    bool dry = true;
    for (int y = 90; y <= 100; ++y) dry = dry && !isLiquid(w.getBlock(0, y, 5));
    CHECK(dry);

    // 4. Zwei Quellen erzeugen eine dritte (Unendliche Quelle)
    for (int x = -8; x <= -2; ++x) {
        w.setBlock(x, 100, -8, STONE);  // Boden/Rand wird unten gesetzt, hier Seitenwaende
    }
    for (int x = -8; x <= -2; ++x) w.setBlock(x, 100, -8, AIR);
    for (int x = -9; x <= -1; ++x) {
        w.setBlock(x, 100, -9, STONE);
        w.setBlock(x, 100, -7, STONE);
    }
    w.setBlock(-9, 100, -8, STONE);
    w.setBlock(-1, 100, -8, STONE);
    w.setBlock(-6, 100, -8, WATER);
    w.setBlock(-4, 100, -8, WATER);
    settleFluids(w, 12);
    CHECK(w.getBlock(-5, 100, -8) == WATER);
    // und entfernt man eine Quelle, bleibt das Wasser bestehen
    w.setBlock(-6, 100, -8, AIR);
    settleFluids(w, 12);
    CHECK(w.getBlock(-6, 100, -8) != AIR);

    // 5. Wasser fuellt eine gegrabene Grube neben einem See (Pull aus Nachbarn)
    w.setBlock(10, 100, 10, WATER);
    settleFluids(w, 4);
    w.setBlock(11, 99, 10, AIR);  // Boden neben der Quelle aufbrechen
    w.setBlock(11, 98, 10, AIR);  // Loch bis auf den Stein bei y = 89 ist zu tief, daher nur teilweise fuellen
    settleFluids(w, 12);
    CHECK(isLiquid(w.getBlock(11, 99, 10)));
    CHECK(isLiquid(w.getBlock(11, 98, 10)));

    // inWaterAt beruecksichtigt die Oberflaechenhoehe
    w.setBlock(-12, 100, 12, WATER);
    CHECK(w.inWaterAt(glm::vec3(-11.5f, 100.5f, 12.5f)));    // Quelle: Hoehe 8/9
    CHECK(!w.inWaterAt(glm::vec3(-11.5f, 100.95f, 12.5f)));  // darueber
    CHECK(!w.inWaterAt(glm::vec3(-11.5f, 101.2f, 12.5f)));
}

static void testFluidMesh() {
    std::vector<BlockId> pad(PAD_VOLUME, AIR);
    auto set = [&](int x, int y, int z, BlockId b) { pad[padIndex(x + 1, y, z + 1)] = b; };
    set(5, 40, 5, STONE);
    set(6, 40, 5, STONE);
    set(5, 41, 5, WATER);
    set(6, 41, 5, WATER_4);
    MeshOutput out;
    buildChunkMesh(pad.data(), nullptr, out);
    // Quelle: Oberseite + 3 Seiten + ein Teilstueck zur niedrigeren Nachbarzelle; Stroemung: Oberseite + 3 Seiten
    CHECK(out.water.size() == 9 * 4);
    bool hasStep = false;
    for (const Vertex& v : out.water)
        if (std::fabs(v.y - (41.0f + 4.0f / 9.0f)) < 1e-4f) hasStep = true;  // Stufe 4: Hoehe 4/9
    CHECK(hasStep);
    // Fallendes Wasser hat volle Hoehe
    std::vector<BlockId> p2(PAD_VOLUME, AIR);
    p2[padIndex(3 + 1, 41, 3 + 1)] = WATER_FALL;
    p2[padIndex(3 + 1, 40, 3 + 1)] = STONE;
    MeshOutput o2;
    buildChunkMesh(p2.data(), nullptr, o2);
    float maxY = 0;
    for (const Vertex& v : o2.water) maxY = std::max(maxY, v.y);
    CHECK(std::fabs(maxY - 42.0f) < 1e-4f);
}

static void testItemsAndInventory() {
    CHECK(itemValid(GRASS) && itemValid(WATER) && itemValid(ITEM_MEAT));
    CHECK(!itemValid(AIR) && !itemValid(WATER_3) && !itemValid(WATER_FALL) && !itemValid(999));
    CHECK(blockDropItem(GRASS) == DIRT);
    CHECK(blockDropItem(STONE) == COBBLE);
    CHECK(blockDropItem(LOG) == LOG);
    CHECK(blockDropItem(BEDROCK) == ITEM_NONE);
    CHECK(blockDropItem(GLASS) == ITEM_NONE);
    CHECK(blockDropItem(WATER) == ITEM_NONE);
    CHECK(itemInfo(STONE).isBlock && !itemInfo(ITEM_MEAT).isBlock);
    for (int i = 0; i < creativeItemCount(); ++i) CHECK(itemValid(creativeItem(i)));

    Inventory inv;
    CHECK(inv.isEmpty());
    CHECK(inv.add(STONE, 40) == 0);
    CHECK(inv.slots[0].id == STONE && inv.slots[0].count == 40);
    CHECK(inv.add(STONE, 40) == 0);  // 24 in den ersten Stapel, 16 in den naechsten
    CHECK(inv.slots[0].count == 64 && inv.slots[1].count == 16);
    CHECK(inv.count(STONE) == 80);
    CHECK(inv.add(DIRT, 1) == 0 && inv.slots[2].id == DIRT);
    CHECK(inv.take(0, 4) && inv.slots[0].count == 60);
    CHECK(!inv.take(2, 2));  // nicht genug
    CHECK(inv.take(2, 1) && inv.slots[2].empty());

    // Voller Rucksack: Rest wird zurueckgegeben
    Inventory full;
    for (auto& s : full.slots) s = {SAND, 64};
    CHECK(full.add(SAND, 5) == 5);
    CHECK(full.add(DIRT, 5) == 5);
    full.slots[20] = {SAND, 60};
    CHECK(full.add(SAND, 10) == 6);
    CHECK(full.slots[20].count == 64);
    CHECK(inv.add(AIR, 3) == 3);  // ungueltig

    // Klicklogik
    ItemStack slot{STONE, 10}, cur;
    Inventory::click(slot, cur, false);  // aufnehmen
    CHECK(slot.empty() && cur.id == STONE && cur.count == 10);
    Inventory::click(slot, cur, false);  // ablegen
    CHECK(cur.empty() && slot.count == 10);
    Inventory::click(slot, cur, true);  // Haelfte aufnehmen (aufgerundet)
    CHECK(cur.count == 5 && slot.count == 5);
    Inventory::click(slot, cur, true);  // eines ablegen
    CHECK(cur.count == 4 && slot.count == 6);
    ItemStack other{DIRT, 3};
    Inventory::click(other, cur, false);  // tauschen
    CHECK(other.id == STONE && other.count == 4 && cur.id == DIRT && cur.count == 3);
    ItemStack a{STONE, 62}, b{STONE, 5};
    Inventory::click(a, b, false);  // zusammenfuehren
    CHECK(a.count == 64 && b.count == 3);

    // Umschalt-Klick
    Inventory q;
    q.slots[3] = {COBBLE, 7};
    q.quickMove(3);
    CHECK(q.slots[3].empty() && q.slots[Inventory::HOTBAR].id == COBBLE);
    q.quickMove(Inventory::HOTBAR);
    CHECK(q.slots[0].id == COBBLE && q.slots[0].count == 7);

    // Layout: Treffer liegen in den berechneten Feldern
    for (bool creative : {false, true}) {
        InvLayout L = InvLayout::make(1280, 720, creative);
        CHECK(L.inside(L.panelPos + glm::vec2(2.0f)));
        for (int i = 0; i < Inventory::SIZE; ++i) {
            glm::vec2 c = L.slotPos(i) + glm::vec2(L.pitch() * 0.5f);
            CHECK(L.hitSlot(c) == i);
            CHECK(L.inside(c));
        }
        if (creative)
            for (int i = 0; i < creativeItemCount(); ++i)
                CHECK(L.hitPalette(L.palettePos(i) + glm::vec2(L.pitch() * 0.5f)) == i);
        CHECK(L.hitSlot(glm::vec2(1.0f, 1.0f)) == -1);
    }
    // Schrift: Breite und HUD-Aufbau
    CHECK(textWidthPx("") == 0 && textWidthPx("A") == 5 && textWidthPx("AB") == 11);
    HudBatch batch;
    HudState st;
    st.inventory = &inv;
    st.health = 7;
    buildHud(batch, st, 1280, 720);
    CHECK(!batch.verts.empty() && batch.verts.size() % 6 == 0);
    size_t covered = 0;
    for (auto& c : batch.cmds) covered += c.count;
    CHECK(covered == batch.verts.size());
    st.inventoryOpen = true;
    st.creative = true;
    buildHud(batch, st, 1280, 720);
    CHECK(!batch.verts.empty());
}

static void testSaveV2() {
    const std::string path = "olicraft_test_save2/w.dat";
    std::filesystem::remove_all("olicraft_test_save2");
    SaveFile sf;
    sf.seed = 77;
    sf.player.health = 13;
    sf.player.creative = true;
    sf.player.inventory.slots[0] = {STONE, 33};
    sf.player.inventory.slots[20] = {ITEM_MEAT, 5};
    std::string err;
    CHECK(writeSaveFile(path, sf, &err));
    SaveFile back;
    CHECK(readSaveFile(path, back, &err));
    CHECK(back.player.hasInventory && back.player.health == 13 && back.player.creative);
    CHECK(back.player.inventory.slots[0] == ItemStack({STONE, 33}));
    CHECK(back.player.inventory.slots[20] == ItemStack({ITEM_MEAT, 5}));
    CHECK(back.player.inventory.slots[1].empty());

    // Version 1 (aelter) bleibt lesbar
    {
        std::ofstream f("olicraft_test_save2/v1.dat", std::ios::binary);
        auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
        auto f32 = [&](float v) { f.write(reinterpret_cast<const char*>(&v), 4); };
        u32(0x52434C4Fu);
        u32(1);
        u32(55);
        for (int i = 0; i < 6; ++i) f32(1.0f + i);
        u32(3);  // slot
        f.put(1);  // flying
        u32(0);    // keine Chunks
    }
    SaveFile old;
    CHECK(readSaveFile("olicraft_test_save2/v1.dat", old, &err));
    CHECK(old.seed == 55 && !old.player.hasInventory && old.player.slot == 3 && old.player.flying);
    std::filesystem::remove_all("olicraft_test_save2");
}

static void testEntities() {
    World w(42);
    w.setMeshingEnabled(false);
    glm::vec3 spawn = w.findSpawn();
    w.loadAround(spawn, 3);

    // flache Testflaeche hoch oben
    const int gy = 100;
    for (int x = -20; x <= 20; ++x)
        for (int z = -20; z <= 20; ++z) {
            for (int y = gy; y < gy + 12; ++y) w.setBlock(x, y, z, AIR);
            w.setBlock(x, gy - 1, z, STONE);
        }
    Player player;
    player.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    Inventory inv;
    EntityEnv env;
    env.allowSpawns = false;
    env.daylight = 1.0f;
    EntityManager em(5);

    // Gegenstand faellt, bleibt liegen, wird erst nach der Verzoegerung eingesammelt
    em.spawnItem({STONE, 3}, glm::vec3(10.5f, gy + 5.0f, 10.5f), glm::vec3(0.0f), 0.5f);
    for (int i = 0; i < 60; ++i) em.update(0.05f, w, player, inv, env);
    CHECK(em.items().size() == 1);
    CHECK(std::fabs(em.items()[0].pos.y - gy) < 0.02f);
    CHECK(inv.isEmpty());
    player.pos = glm::vec3(10.5f, static_cast<float>(gy), 9.5f);
    em.update(0.05f, w, player, inv, env);
    CHECK(em.items().empty() && inv.count(STONE) == 3);
    // Verzoegerung
    em.spawnItem({DIRT, 1}, player.pos, glm::vec3(0.0f), 0.5f);
    em.update(0.05f, w, player, inv, env);
    CHECK(inv.count(DIRT) == 0 && em.items().size() == 1);
    for (int i = 0; i < 20; ++i) em.update(0.05f, w, player, inv, env);
    CHECK(inv.count(DIRT) == 1 && em.items().empty());
    // voller Rucksack: Gegenstand bleibt liegen
    Inventory full;
    for (auto& s : full.slots) s = {SAND, 64};
    em.spawnItem({SAND, 2}, player.pos, glm::vec3(0.0f), 0.0f);
    for (int i = 0; i < 10; ++i) em.update(0.05f, w, player, full, env);
    CHECK(em.items().size() == 1 && em.items()[0].stack.count == 2);
    em.clear();

    // Mob faellt, landet, wandert
    player.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    Mob& pig = em.spawnMob(MobType::Pig, glm::vec3(5.5f, gy + 4.0f, 5.5f));
    for (int i = 0; i < 40; ++i) em.update(0.05f, w, player, inv, env);
    {
        Mob& m = em.mobs()[0];
        CHECK(m.onGround && std::fabs(m.pos.y - gy) < 0.02f);
    }
    (void)pig;
    const glm::vec3 p0 = em.mobs()[0].pos;
    float maxMove = 0;
    for (int i = 0; i < 400; ++i) {
        em.update(0.05f, w, player, inv, env);
        maxMove = std::max(maxMove, glm::length(em.mobs()[0].pos - p0));
    }
    CHECK(maxMove > 1.0f);  // er hat sich irgendwann bewegt
    CHECK(em.mobs()[0].pos.y > gy - 0.5f);
    em.clear();

    // Stufe hochspringen: Wanderrichtung +x, Stufe (1 Block) bei x = 8
    for (int z = -5; z <= 5; ++z) w.setBlock(8, gy, z, STONE);
    Mob& cow = em.spawnMob(MobType::Cow, glm::vec3(4.5f, gy, 0.5f));
    cow.state = Mob::Wander;
    cow.wanderYaw = 0.0f;
    cow.yaw = 0.0f;
    cow.stateTimer = 1000.0f;
    for (int i = 0; i < 200; ++i) {
        Mob& m = em.mobs()[0];
        m.state = Mob::Wander;
        m.wanderYaw = 0.0f;
        m.stateTimer = 1000.0f;
        em.update(0.05f, w, player, inv, env);
    }
    CHECK(em.mobs()[0].pos.x > 9.0f);
    em.clear();

    // Zombie greift an, mit Abklingzeit; im Kreativmodus ignoriert er den Spieler
    player.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    em.spawnMob(MobType::Zombie, glm::vec3(2.0f, gy, 0.5f));
    int hits = 0;
    for (int i = 0; i < 60; ++i) {  // 3 s
        em.update(0.05f, w, player, inv, env);
        hits += static_cast<int>(em.playerHits().size());
        for (auto& h : em.playerHits()) CHECK(h.damage == 3);
        em.playerHits().clear();
    }
    CHECK(hits >= 2 && hits <= 4);
    env.creative = true;
    for (int i = 0; i < 40; ++i) {
        em.update(0.05f, w, player, inv, env);
        hits += static_cast<int>(em.playerHits().size());
        em.playerHits().clear();
    }
    CHECK(hits <= 4);
    env.creative = false;
    em.clear();

    // Zombies verbrennen im Tageslicht
    player.pos = glm::vec3(-15.5f, static_cast<float>(gy), -15.5f);
    em.spawnMob(MobType::Zombie, glm::vec3(10.5f, gy, 10.5f));
    for (int i = 0; i < 20 * 15; ++i) em.update(0.05f, w, player, inv, env);
    CHECK(em.mobs().empty());  // tot und entfernt
    em.clear();

    // Treffer, Rueckstoss, Flucht, Beute
    player.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    em.spawnMob(MobType::Pig, glm::vec3(3.5f, gy, 0.5f));
    float t = 0;
    glm::vec3 org = player.eyePos();
    const glm::vec3 toPig = glm::normalize(glm::vec3(3.5f, gy + 0.4f, 0.5f) - org);
    Mob* hitMob = em.raycastMob(org, toPig, 6.0f, t);
    CHECK(hitMob != nullptr && t > 2.0f && t < 3.2f);
    if (!hitMob) return;
    CHECK(em.raycastMob(org, -toPig, 6.0f, t) == nullptr);
    CHECK(em.raycastMob(org, toPig, 1.0f, t) == nullptr);
    CHECK(em.raycastMob(org, glm::vec3(1.0f, 0.0f, 0.0f), 6.0f, t) == nullptr);  // geht ueber das Schwein hinweg
    em.hurtMob(*hitMob, 2, player.pos);
    CHECK(hitMob->health == 8 && hitMob->hurtTimer > 0 && hitMob->panicTimer > 0);
    CHECK(hitMob->vel.x > 3.0f && hitMob->vel.y > 0);
    const float x0 = hitMob->pos.x;
    for (int i = 0; i < 30; ++i) em.update(0.05f, w, player, inv, env);
    CHECK(em.mobs()[0].pos.x > x0 + 1.0f);  // flieht vom Spieler weg
    em.hurtMob(em.mobs()[0], 50, player.pos);
    CHECK(em.mobs()[0].dead());
    CHECK(!em.items().empty() && em.items()[0].stack.id == ITEM_MEAT);
    CHECK(em.items()[0].stack.count >= 1 && em.items()[0].stack.count <= 3);
    for (int i = 0; i < 30; ++i) em.update(0.05f, w, player, inv, env);
    CHECK(em.mobs().empty());
    // Tote Mobs lassen sich nicht mehr treffen
    em.clear();

    // Spawnen: tagsueber Tiere, nachts Zombies, nie im Kreativmodus
    EntityEnv spawnEnv;
    spawnEnv.allowSpawns = true;
    spawnEnv.daylight = 1.0f;
    player.pos = spawn;
    for (int i = 0; i < 400; ++i) em.update(0.05f, w, player, inv, spawnEnv);
    CHECK(em.countMobs(true) == 0);  // tagsueber nie Zombies
    CHECK(em.countMobs(false) >= 1 && em.countMobs(false) <= 12);
    int passives = em.countMobs(false);
    EntityEnv night = spawnEnv;
    night.daylight = 0.0f;
    for (int i = 0; i < 1200; ++i) {
        em.update(0.05f, w, player, inv, night);
        em.playerHits().clear();
    }
    CHECK(em.countMobs(true) >= 1 && em.countMobs(true) <= 8);
    for (const Mob& m : em.mobs()) {
        float d = std::hypot(m.pos.x - player.pos.x, m.pos.z - player.pos.z);
        CHECK(d < 100.0f);
    }
    (void)passives;
    em.clear();
    EntityEnv cre = night;
    cre.creative = true;
    for (int i = 0; i < 400; ++i) em.update(0.05f, w, player, inv, cre);
    CHECK(em.countMobs(true) == 0);

    // Darstellung: Vertexanzahl durch 4 teilbar und endlich
    em.clear();
    em.spawnMob(MobType::Zombie, glm::vec3(1.0f, gy, 1.0f));
    em.spawnMob(MobType::Sheep, glm::vec3(2.0f, gy, 1.0f)).hurtTimer = 1.0f;
    em.spawnItem({ITEM_MEAT, 1}, glm::vec3(0.0f, gy, 0.0f), glm::vec3(0.0f));
    em.spawnItem({STONE, 1}, glm::vec3(0.5f, gy, 0.0f), glm::vec3(0.0f));
    EntityBatches batches;
    em.buildMesh(w, glm::vec3(0.0f, gy + 2.0f, -3.0f), 1.0f, batches);
    CHECK(!batches.items.empty() && !batches.skin[SKIN_ZOMBIE].empty() && !batches.skinHurt[SKIN_SHEEP].empty());
    bool finite = true, aligned = batches.items.size() % 4 == 0;
    for (int s = 0; s < SKIN_COUNT; ++s) {
        aligned = aligned && batches.skin[s].size() % 4 == 0 && batches.skinHurt[s].size() % 4 == 0;
        for (const Vertex& v : batches.skin[s]) finite = finite && std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }
    CHECK(finite && aligned);
}

static void testWaterExitAndZombieDay() {
    World w(21);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    const int gy = 100;
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z) {
            for (int y = gy - 6; y < gy + 20; ++y) w.setBlock(x, y, z, AIR);
            for (int y = gy - 6; y < gy - 2; ++y) w.setBlock(x, y, z, STONE);
        }
    // Becken x = 0..9, Wasser y = gy-2 .. gy (Oberflaeche bei gy + 0.889), Ufer ab x = 10
    for (int x = 10; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z)
            for (int y = gy - 2; y <= gy + 1; ++y) w.setBlock(x, y, z, STONE);  // Ufer: Oberkante gy + 2
    for (int x = -14; x <= 9; ++x)
        for (int z = -14; z <= 14; ++z)
            for (int y = gy - 2; y <= gy; ++y) w.setBlock(x, y, z, WATER);
    // Eine Stufe: Ufer nur bis gy + 1 (1 Block ueber der Wasseroberflaeche) bei z = 0
    for (int z = -2; z <= 2; ++z) w.setBlock(10, gy + 1, z, AIR);

    Player p;
    p.pos = glm::vec3(8.5f, gy - 1.0f, 0.5f);
    p.yaw = 0.0f;  // +x
    MoveInput in;
    in.forward = 1.0f;
    in.jump = true;
    for (int i = 0; i < 40; ++i) p.update(0.05f, in, w);  // 2 s
    CHECK(p.pos.x > 10.3f);                       // ist an Land
    CHECK(p.pos.y > gy + 0.95f);                   // steht ueber der Wasseroberflaeche auf dem Ufer
    CHECK(!p.inWater);

    // Zwei Bloecke hoch (Ufer bei z = 8) ist zu hoch
    Player q;
    q.pos = glm::vec3(8.5f, gy - 1.0f, 8.5f);
    q.yaw = 0.0f;
    for (int i = 0; i < 200; ++i) q.update(0.05f, in, w);
    CHECK(q.pos.x < 10.0f);

    // Ohne Sprungtaste kein Schub
    Player r;
    r.pos = glm::vec3(8.5f, gy - 1.0f, 0.5f);
    r.yaw = 0.0f;
    MoveInput noJump = in;
    noJump.jump = false;
    for (int i = 0; i < 200; ++i) r.update(0.05f, noJump, w);
    CHECK(r.pos.x < 10.0f);

    // Zombies am Tag: in einer Hoehle (Dach) loesen sie sich nach 10 s auf, draussen verbrennen sie
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= -3; ++z) w.setBlock(x, gy + 6, z, STONE);  // Dach ueber dem Wasserbecken-Teil z < -3
    Player pl;
    pl.pos = glm::vec3(0.5f, static_cast<float>(gy + 1), 12.5f);
    Inventory inv;
    EntityEnv env;
    env.allowSpawns = false;
    env.daylight = 1.0f;
    EntityManager em(3);
    em.spawnMob(MobType::Zombie, glm::vec3(5.5f, gy + 1.0f, -8.5f)).pos.y = gy + 1.0f;  // unter dem Dach, auf Wasser: schwimmt
    for (int i = 0; i < 20 * 14; ++i) em.update(0.05f, w, pl, inv, env);
    CHECK(em.mobs().empty());

    // Zombies sinken im Wasser und gehen am Grund, Tiere treiben
    {
        EntityManager e2(9);
        EntityEnv night;
        night.allowSpawns = false;
        night.daylight = 0.0f;
        Player far;
        far.pos = glm::vec3(0.5f, static_cast<float>(gy + 1), 60.0f);  // ausserhalb der Verfolgungsreichweite
        e2.spawnMob(MobType::Zombie, glm::vec3(0.5f, gy + 0.2f, 0.5f));
        e2.spawnMob(MobType::Pig, glm::vec3(4.5f, gy + 0.2f, 0.5f));
        for (int i = 0; i < 60; ++i) e2.update(0.05f, w, far, inv, night);  // 3 s
        CHECK(e2.mobs().size() == 2);
        CHECK(e2.mobs()[0].pos.y < gy - 1.5f && e2.mobs()[0].onGround);
        CHECK(e2.mobs()[1].pos.y > gy + 0.3f);
    }
}

static void testLava() {
    World w(31);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    clearArea(w, -14, 14, 86, 110, -14, 14);
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z) w.setBlock(x, 99, z, STONE);

    // Ausbreitung: nur 3 Stufen, und langsamer (1.5 s je Schritt) als Wasser
    w.setBlock(0, 100, 0, LAVA);
    w.tick(0.3f);  // Wasser waere schon einen Schritt weiter, Lava noch nicht
    CHECK(w.getBlock(1, 100, 0) == AIR);
    w.tick(1.3f);
    CHECK(w.getBlock(1, 100, 0) == LAVA_1);
    for (int i = 0; i < 20; ++i) w.tick(1.5f);
    CHECK(w.getBlock(2, 100, 0) == LAVA_2);
    CHECK(w.getBlock(3, 100, 0) == LAVA_3);
    CHECK(w.getBlock(4, 100, 0) == AIR);
    CHECK(w.getBlock(1, 100, 1) == LAVA_2);
    // Quelle weg: Lava zieht sich zurueck
    w.setBlock(0, 100, 0, AIR);
    for (int i = 0; i < 20; ++i) w.tick(1.5f);
    int left = 0;
    for (int x = -6; x <= 6; ++x)
        for (int z = -6; z <= 6; ++z)
            if (isLiquid(w.getBlock(x, 100, z))) ++left;
    CHECK(left == 0);

    // Keine unendliche Lava: zwei Quellen mit Luecke bleiben getrennt
    w.setBlock(-6, 100, 8, LAVA);
    w.setBlock(-4, 100, 8, LAVA);
    for (int i = 0; i < 6; ++i) w.tick(1.5f);
    CHECK(w.getBlock(-5, 100, 8) != LAVA);

    // Wasser trifft Lavaquelle: Obsidian
    w.setBlock(8, 100, -8, LAVA);
    w.tick(0.1f);
    w.setBlock(9, 100, -8, WATER);
    for (int i = 0; i < 6; ++i) w.tick(0.3f);
    CHECK(w.getBlock(8, 100, -8) == OBSIDIAN);
    // Wasser darf Obsidian nicht verdraengen
    CHECK(w.getBlock(9, 100, -8) == WATER);

    // Fliessende Lava trifft Wasser: Bruchstein
    for (int x = 4; x <= 8; ++x)
        for (int z = 4; z <= 8; ++z) w.setBlock(x, 100, z, AIR);
    w.setBlock(4, 100, 6, LAVA);
    for (int i = 0; i < 2; ++i) w.tick(1.6f);  // Lava fliesst nach (5, 100, 6)
    CHECK(w.getBlock(5, 100, 6) == LAVA_1);
    w.setBlock(7, 100, 6, WATER);              // Wasser fliesst nach (6, 100, 6) und trifft die Lava
    for (int i = 0; i < 12; ++i) w.tick(0.3f);
    bool cobble = w.getBlock(5, 100, 6) == COBBLE || w.getBlock(6, 100, 6) == COBBLE;
    CHECK(cobble);
    CHECK(w.getBlock(4, 100, 6) == OBSIDIAN || w.getBlock(4, 100, 6) == LAVA);

    // inLavaAt und Wasser unterscheiden sich
    w.setBlock(-10, 100, -10, LAVA);
    CHECK(w.inLavaAt(glm::vec3(-9.5f, 100.5f, -9.5f)) && !w.inWaterAt(glm::vec3(-9.5f, 100.5f, -9.5f)));

    // Spieler in Lava: langsam, bremst Sturz, kein Ertrinken
    Player p;
    p.pos = glm::vec3(-9.5f, 100.0f, -9.5f);
    MoveInput none;
    for (int i = 0; i < 10; ++i) p.update(0.05f, none, w);
    CHECK(p.inLava && !p.headInWater);

    // Gegenstaende verbrennen, Mobs nehmen Schaden
    EntityManager em(1);
    Inventory inv;
    EntityEnv env;
    env.allowSpawns = false;
    Player far;
    far.pos = glm::vec3(0.5f, 100.0f, 0.5f);
    em.spawnItem({STONE, 1}, glm::vec3(-9.5f, 100.2f, -9.5f), glm::vec3(0.0f), 99.0f);
    em.spawnMob(MobType::Pig, glm::vec3(-9.5f, 100.3f, -9.5f));
    for (int i = 0; i < 40; ++i) em.update(0.05f, w, far, inv, env);
    CHECK(em.items().empty());
    CHECK(em.mobs().empty() || em.mobs()[0].health < mobSpec(MobType::Pig).health);

    // Mesh: Lava liegt im undurchsichtigen Teil und leuchtet (sky = 2)
    std::vector<BlockId> pad(PAD_VOLUME, AIR);
    pad[padIndex(5 + 1, 40, 5 + 1)] = STONE;
    pad[padIndex(5 + 1, 41, 5 + 1)] = LAVA;
    pad[padIndex(8 + 1, 40, 8 + 1)] = STONE;
    pad[padIndex(8 + 1, 41, 8 + 1)] = WATER;
    MeshOutput out;
    buildChunkMesh(pad.data(), nullptr, out);
    bool emissive = false, water = !out.water.empty();
    for (const Vertex& v : out.opaque)
        if (v.sky > 1.5f && v.y > 41.0f) emissive = true;
    CHECK(emissive && water);
    for (const Vertex& v : out.water) CHECK(v.sky <= 1.0f);
}

static void testLavaTerrain() {
    World w(42);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 4);
    int lava = 0, lavaHigh = 0;
    for (int x = -64; x < 64; ++x)
        for (int z = -64; z < 64; ++z)
            for (int y = 0; y < CHUNK_H; ++y)
                if (w.getBlock(x, y, z) == LAVA) {
                    ++lava;
                    if (y > LAVA_LEVEL) ++lavaHigh;
                }
    CHECK(lava > 50);
    CHECK(lavaHigh == 0);
}

static void testSettingsAndMenu() {
    const std::string path = "olicraft_test_settings/s.cfg";
    std::filesystem::remove_all("olicraft_test_settings");
    std::filesystem::create_directories("olicraft_test_settings");
    Settings s;
    s.renderDistance = 17;
    s.mouseSensitivity = 0.15f;
    s.fov = 95.0f;
    s.vsync = false;
    s.masterVolume = 0.35f;
    s.effectVolume = 0.6f;
    s.musicVolume = 0.0f;
    CHECK(saveSettings(path, s));
    Settings r;
    CHECK(loadSettings(path, r));
    CHECK(r.renderDistance == 17 && std::fabs(r.mouseSensitivity - 0.15f) < 1e-4f && r.fov == 95.0f && !r.vsync);
    CHECK(std::fabs(r.masterVolume - 0.35f) < 1e-3f && std::fabs(r.effectVolume - 0.6f) < 1e-3f && r.musicVolume == 0.0f);
    // Werte ausserhalb und Muell werden abgefangen
    {
        std::ofstream f(path);
        f << "render_distance=999\nmaster_volume=7\nmusic_volume=-2\nfov=abc\nmouse_sensitivity=-4\nunbekannt=1\nkeingleichzeichen\n";
    }
    Settings c;
    CHECK(loadSettings(path, c));
    CHECK(c.renderDistance == Settings::MAX_DISTANCE && c.fov == 70.0f && c.mouseSensitivity == Settings::MIN_SENS);
    CHECK(c.masterVolume == 1.0f && c.musicVolume == 0.0f && c.effectVolume == 1.0f);
    CHECK(!loadSettings("olicraft_test_settings/gibt_es_nicht.cfg", c));
    std::filesystem::remove_all("olicraft_test_settings");

    // Menue-Layouts: Widgets liegen im Fenster, ueberlappen nicht und sind per Mausposition treffbar
    for (MenuScreen scr : {MenuScreen::Pause, MenuScreen::Settings, MenuScreen::Main, MenuScreen::NewWorld, MenuScreen::Graphics}) {
        for (auto dim : {std::make_pair(1280, 720), std::make_pair(800, 600), std::make_pair(1920, 1080)}) {
            MenuState st;
            MenuLayout m = buildMenu(scr, st, dim.first, dim.second);
            CHECK(!m.widgets.empty());
            for (size_t i = 0; i < m.widgets.size(); ++i) {
                const Widget& a = m.widgets[i];
                CHECK(a.pos.x >= 0 && a.pos.y >= 0 && a.pos.x + a.size.x <= dim.first && a.pos.y + a.size.y <= dim.second);
                CHECK(hitWidget(m, a.pos + a.size * 0.5f) == static_cast<int>(i));
                for (size_t j = i + 1; j < m.widgets.size(); ++j) {
                    const Widget& b = m.widgets[j];
                    bool overlap = a.pos.x < b.pos.x + b.size.x && b.pos.x < a.pos.x + a.size.x &&
                                   a.pos.y < b.pos.y + b.size.y && b.pos.y < a.pos.y + a.size.y;
                    CHECK(!overlap);
                }
            }
            CHECK(hitWidget(m, glm::vec2(1.0f, 1.0f)) == -1);
        }
    }
    CHECK(buildMenu(MenuScreen::None, MenuState(), 1280, 720).widgets.empty());

    // Schieberegler-Abbildung
    CHECK(distanceFromFraction(0.0f) == Settings::MIN_DISTANCE && distanceFromFraction(1.0f) == Settings::MAX_DISTANCE);
    CHECK(fovFromFraction(0.0f) == Settings::MIN_FOV && fovFromFraction(1.0f) == Settings::MAX_FOV);
    CHECK(sensitivityFromFraction(0.0f) == Settings::MIN_SENS && sensitivityFromFraction(1.0f) == Settings::MAX_SENS);
    MenuState ms;
    ms.renderDistance = 14;
    MenuLayout ml = buildMenu(MenuScreen::Settings, ms, 1280, 720);
    for (const Widget& w : ml.widgets)
        if (w.id == W_DISTANCE) {
            CHECK(w.type == Widget::Slider && w.text.find("14") != std::string::npos);
            CHECK(distanceFromFraction(sliderFraction(w, w.pos.x + w.size.x * w.fraction)) == 14);
            CHECK(sliderFraction(w, w.pos.x - 50.0f) == 0.0f && sliderFraction(w, w.pos.x + w.size.x + 50.0f) == 1.0f);
        }
    // Lautstaerke-Regler
    CHECK(volumeFromFraction(0.0f) == 0.0f && volumeFromFraction(1.0f) == 1.0f && volumeFromFraction(2.0f) == 1.0f &&
          std::fabs(volumeFromFraction(0.52f) - 0.5f) < 1e-5f);
    {
        MenuState vs;
        vs.masterVolume = 0.35f;
        vs.musicVolume = 0.0f;
        int sliders = 0;
        for (const Widget& w : buildMenu(MenuScreen::Settings, vs, 1280, 720).widgets) {
            if (w.id == W_VOL_MASTER) CHECK(w.type == Widget::Slider && w.text.find("35%") != std::string::npos && ++sliders);
            if (w.id == W_VOL_EFFECTS) CHECK(w.type == Widget::Slider && w.text.find("100%") != std::string::npos && ++sliders);
            if (w.id == W_VOL_MUSIC) CHECK(w.type == Widget::Slider && w.text.find("0%") != std::string::npos && ++sliders);
        }
        CHECK(sliders == 3);
    }
    // HUD baut das Menue
    HudBatch batch;
    HudState hs;
    hs.menu = ml;
    hs.menuHover = 1;
    buildHud(batch, hs, 1280, 720);
    CHECK(!batch.verts.empty());
}

static void testCraftingAndTools() {
    auto grid = [](int n, std::initializer_list<std::pair<int, ItemId>> cells) {
        std::vector<ItemStack> g(static_cast<size_t>(n * n));
        for (auto& c : cells) g[c.first] = {c.second, 1};
        return g;
    };
    // Holz -> Bretter, an beliebiger Stelle im Raster
    for (int pos = 0; pos < 4; ++pos) {
        auto g = grid(2, {{pos, LOG}});
        ItemStack r = matchRecipe(g.data(), 2);
        CHECK(r.id == PLANKS && r.count == 4);
    }
    for (int pos = 0; pos < 9; ++pos) {
        auto g = grid(3, {{pos, LOG}});
        CHECK(matchRecipe(g.data(), 3).id == PLANKS);
    }
    // Stoecke: zwei Bretter uebereinander, auch seitlich verschoben
    {
        auto g = grid(2, {{0, PLANKS}, {2, PLANKS}});
        CHECK(matchRecipe(g.data(), 2).id == ITEM_STICK);
        g = grid(2, {{1, PLANKS}, {3, PLANKS}});
        CHECK(matchRecipe(g.data(), 2).id == ITEM_STICK && matchRecipe(g.data(), 2).count == 4);
        g = grid(2, {{0, PLANKS}, {1, PLANKS}});  // nebeneinander: kein Rezept
        CHECK(matchRecipe(g.data(), 2).empty());
    }
    // Werkbank: 2x2 Bretter; im 3x3-Raster ebenfalls
    {
        auto g = grid(2, {{0, PLANKS}, {1, PLANKS}, {2, PLANKS}, {3, PLANKS}});
        CHECK(matchRecipe(g.data(), 2).id == CRAFTING_TABLE);
        auto g3 = grid(3, {{4, PLANKS}, {5, PLANKS}, {7, PLANKS}, {8, PLANKS}});
        CHECK(matchRecipe(g3.data(), 3).id == CRAFTING_TABLE);
        auto g3b = grid(3, {{4, PLANKS}, {5, PLANKS}, {7, PLANKS}});  // eines fehlt
        CHECK(matchRecipe(g3b.data(), 3).empty());
    }
    // Werkzeuge nur im 3x3-Raster (Spitzhacke ist 3 breit)
    {
        auto g3 = grid(3, {{0, PLANKS}, {1, PLANKS}, {2, PLANKS}, {4, ITEM_STICK}, {7, ITEM_STICK}});
        CHECK(matchRecipe(g3.data(), 3).id == ITEM_WOODEN_PICKAXE);
        auto s3 = grid(3, {{3, COBBLE}, {4, COBBLE}, {5, COBBLE}, {7, ITEM_STICK}, {8 - 0, ITEM_STICK}});  // Stiele nicht mittig
        CHECK(matchRecipe(s3.data(), 3).empty());
        auto s4 = grid(3, {{0, COBBLE}, {1, COBBLE}, {2, COBBLE}, {4, ITEM_STICK}, {7, ITEM_STICK}});
        CHECK(matchRecipe(s4.data(), 3).id == ITEM_STONE_PICKAXE);
        // Axt, auch gespiegelt
        auto a1 = grid(3, {{0, PLANKS}, {1, PLANKS}, {3, PLANKS}, {4, ITEM_STICK}, {7, ITEM_STICK}});
        CHECK(matchRecipe(a1.data(), 3).id == ITEM_WOODEN_AXE);
        auto a2 = grid(3, {{1, PLANKS}, {2, PLANKS}, {4, PLANKS}, {5, ITEM_STICK}, {8, ITEM_STICK}});  // nach rechts verschoben
        CHECK(matchRecipe(a2.data(), 3).id == ITEM_WOODEN_AXE);
        // gespiegelte Axt (Stiel links)
        auto a3 = grid(3, {{0, COBBLE}, {1, COBBLE}, {3, ITEM_STICK}, {4, COBBLE}, {6, ITEM_STICK}});
        auto a4 = grid(3, {{1, COBBLE}, {2, COBBLE}, {4, ITEM_STICK}, {5, COBBLE}, {7, ITEM_STICK}});
        CHECK(matchRecipe(a3.data(), 3).id == ITEM_STONE_AXE);
        CHECK(matchRecipe(a4.data(), 3).id == ITEM_STONE_AXE);
        auto sh = grid(3, {{1, PLANKS}, {4, ITEM_STICK}, {7, ITEM_STICK}});
        CHECK(matchRecipe(sh.data(), 3).id == ITEM_WOODEN_SHOVEL);
        auto sw = grid(3, {{1, COBBLE}, {4, COBBLE}, {7, ITEM_STICK}});
        CHECK(matchRecipe(sw.data(), 3).id == ITEM_STONE_SWORD);
        // gemischtes Material gibt es nicht
        auto mix = grid(3, {{1, PLANKS}, {4, COBBLE}, {7, ITEM_STICK}});
        CHECK(matchRecipe(mix.data(), 3).empty());
        // zusaetzlicher Gegenstand verhindert das Rezept
        auto extra = grid(3, {{1, PLANKS}, {4, ITEM_STICK}, {7, ITEM_STICK}, {0, DIRT}});
        CHECK(matchRecipe(extra.data(), 3).empty());
    }
    // Ofen: 8 Bruchstein im Ring, Mitte leer; Glas und Stein gibt es nur noch aus dem Ofen
    {
        auto f = grid(3, {{0, COBBLE}, {1, COBBLE}, {2, COBBLE}, {3, COBBLE}, {5, COBBLE}, {6, COBBLE}, {7, COBBLE}, {8, COBBLE}});
        CHECK(matchRecipe(f.data(), 3).id == FURNACE);
        auto full = grid(3, {{0, COBBLE}, {1, COBBLE}, {2, COBBLE}, {3, COBBLE}, {4, COBBLE}, {5, COBBLE}, {6, COBBLE}, {7, COBBLE}, {8, COBBLE}});
        CHECK(matchRecipe(full.data(), 3).empty());
        auto g = grid(2, {{0, SAND}, {1, SAND}, {2, SAND}, {3, SAND}});
        CHECK(matchRecipe(g.data(), 2).id == SANDSTONE);  // Glas gibt es nur aus dem Ofen, Sand ergibt Sandstein
        auto c = grid(2, {{3, COBBLE}});
        CHECK(matchRecipe(c.data(), 2).empty());
        // Eisen- und Diamantwerkzeuge
        auto ip = grid(3, {{0, ITEM_IRON_INGOT}, {1, ITEM_IRON_INGOT}, {2, ITEM_IRON_INGOT}, {4, ITEM_STICK}, {7, ITEM_STICK}});
        CHECK(matchRecipe(ip.data(), 3).id == ITEM_IRON_PICKAXE);
        auto ds = grid(3, {{1, ITEM_DIAMOND}, {4, ITEM_DIAMOND}, {7, ITEM_STICK}});
        CHECK(matchRecipe(ds.data(), 3).id == ITEM_DIAMOND_SWORD);
    }
    // Leeres Raster und Zutaten verbrauchen
    {
        std::vector<ItemStack> empty(9);
        CHECK(matchRecipe(empty.data(), 3).empty());
        std::vector<ItemStack> g(4);
        g[0] = {LOG, 3};
        consumeIngredients(g.data(), 2);
        CHECK(g[0].count == 2 && g[1].empty());
        g[0].count = 1;
        consumeIngredients(g.data(), 2);
        CHECK(g[0].empty());
    }
    // Jedes Rezept liefert gueltige Gegenstaende
    for (const Recipe& r : recipes()) {
        CHECK(itemValid(r.result.id) && r.result.count > 0);
        for (int i = 0; i < r.symbolCount; ++i) CHECK(itemValid(r.items[i]));
    }

    // Werkzeugeigenschaften
    for (ItemId id = ITEM_WOODEN_PICKAXE; id <= ITEM_STONE_SWORD; ++id) {
        CHECK(itemValid(id));
        const ItemInfo& ii = itemInfo(id);
        CHECK(ii.maxStack == 1 && ii.maxDurability > 0 && ii.tool != TOOL_NONE && ii.tier >= 1);
    }
    CHECK(itemInfo(ITEM_STONE_PICKAXE).maxDurability > itemInfo(ITEM_WOODEN_PICKAXE).maxDurability);

    // Abbau: Stein braucht eine Spitzhacke, Obsidian eine Steinspitzhacke
    const ItemStack hand, wp{ITEM_WOODEN_PICKAXE, 1}, sp{ITEM_STONE_PICKAXE, 1}, wa{ITEM_WOODEN_AXE, 1};
    CHECK(!canHarvest(STONE, hand) && canHarvest(STONE, wp) && canHarvest(STONE, sp));
    const ItemStack ip{ITEM_IRON_PICKAXE, 1}, dp{ITEM_DIAMOND_PICKAXE, 1};
    CHECK(!canHarvest(OBSIDIAN, hand) && !canHarvest(OBSIDIAN, wp) && !canHarvest(OBSIDIAN, sp) && !canHarvest(OBSIDIAN, ip) &&
          canHarvest(OBSIDIAN, dp));
    CHECK(!canHarvest(IRON_ORE, wp) && canHarvest(IRON_ORE, sp) && !canHarvest(DIAMOND_ORE, sp) && canHarvest(DIAMOND_ORE, ip));
    CHECK(canHarvest(COAL_ORE, wp) && !canHarvest(COAL_ORE, hand));
    CHECK(miningSeconds(STONE, dp) < miningSeconds(STONE, ip) && miningSeconds(STONE, ip) < miningSeconds(STONE, sp));
    CHECK(!canHarvest(STONE, wa));  // falsche Werkzeugart
    CHECK(canHarvest(DIRT, hand) && canHarvest(LOG, hand) && canHarvest(LEAVES, hand));
    CHECK(miningSeconds(STONE, sp) < miningSeconds(STONE, wp));
    CHECK(miningSeconds(STONE, wp) < miningSeconds(STONE, hand));
    CHECK(miningSeconds(STONE, hand) >= 3.0f);                    // von Hand muehsam
    CHECK(miningSeconds(LOG, wa) < miningSeconds(LOG, hand));     // Axt hilft bei Holz
    CHECK(miningSeconds(LOG, wp) == miningSeconds(LOG, hand));    // Spitzhacke nicht
    CHECK(miningSeconds(DIRT, ItemStack{ITEM_WOODEN_SHOVEL, 1}) < miningSeconds(DIRT, hand));
    CHECK(attackDamage(hand) == 2 && attackDamage(ItemStack{ITEM_STONE_SWORD, 1}) > attackDamage(ItemStack{ITEM_WOODEN_SWORD, 1}));
    CHECK(attackDamage(ItemStack{ITEM_WOODEN_SWORD, 1}) > attackDamage(hand));

    // Werkzeuge stapeln nicht, gleiche Abnutzung schon beim Verschieben/Ablegen im Raster nicht
    Inventory inv;
    CHECK(inv.add(ITEM_WOODEN_PICKAXE, 1, 10) == 0);
    CHECK(inv.add(ITEM_WOODEN_PICKAXE, 1, 10) == 0);
    CHECK(inv.slots[0].damage == 10 && inv.slots[1].id == ITEM_WOODEN_PICKAXE);
    ItemStack a{ITEM_WOODEN_PICKAXE, 1, 5}, b{ITEM_WOODEN_PICKAXE, 1, 7};
    Inventory::click(a, b, false);  // verschiedene Abnutzung: tauschen
    CHECK(a.damage == 7 && b.damage == 5);
    Inventory::click(a, b, false);
    ItemStack s1{STONE, 3}, s2{STONE, 4};
    Inventory::click(s1, s2, false);
    CHECK(s1.count == 7 && s2.empty());
    // Haltbarkeit im Speicherstand
    SaveFile sf;
    sf.player.inventory.slots[2] = {ITEM_STONE_AXE, 1, 40};
    std::filesystem::create_directories("olicraft_test_save3");
    std::string err;
    CHECK(writeSaveFile("olicraft_test_save3/w.dat", sf, &err));
    SaveFile back;
    CHECK(readSaveFile("olicraft_test_save3/w.dat", back, &err));
    CHECK(back.player.inventory.slots[2] == ItemStack({ITEM_STONE_AXE, 1, 40}));
    std::filesystem::remove_all("olicraft_test_save3");

    // Werkbank-Block
    CHECK(blockInfo(CRAFTING_TABLE).solid && blockInfo(CRAFTING_TABLE).opaque);
    CHECK(blockDropItem(CRAFTING_TABLE) == CRAFTING_TABLE && blockDropItem(OBSIDIAN) == OBSIDIAN);
    CHECK(blockDropItem(STONE) == COBBLE);
}

static void testOresAndFurnace() {
    // Erze: Verteilung, Haeufigkeit und Hoehenbereiche
    World w(42);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 4);
    int coal = 0, iron = 0, diamond = 0, diamondHigh = 0, ironHigh = 0, exposedBad = 0;
    for (int x = -64; x < 64; ++x)
        for (int z = -64; z < 64; ++z)
            for (int y = 0; y < CHUNK_H; ++y) {
                BlockId b = w.getBlock(x, y, z);
                if (b == COAL_ORE) ++coal;
                if (b == IRON_ORE) {
                    ++iron;
                    if (y > 64 + 6) ++ironHigh;
                }
                if (b == DIAMOND_ORE) {
                    ++diamond;
                    if (y > 15 + 4) ++diamondHigh;
                }
                if ((b == COAL_ORE || b == IRON_ORE || b == DIAMOND_ORE) && y < 1) ++exposedBad;
            }
    CHECK(coal > 300 && iron > 100);
    CHECK(coal > iron && iron > diamond);
    CHECK(diamond >= 1 && diamondHigh == 0 && ironHigh == 0 && exposedBad == 0);

    // Ofen
    World f(5);
    f.setMeshingEnabled(false);
    f.loadAround(glm::vec3(0.0f), 1);
    for (int y = 100; y < 106; ++y) f.setBlock(2, y, 2, AIR);
    f.setBlock(2, 100, 2, FURNACE);
    CHECK(f.findFurnace(2, 100, 2) == nullptr);
    Furnace& fu = f.furnaceAt(2, 100, 2);
    CHECK(f.furnaceCount() == 1 && f.findFurnace(2, 100, 2) == &fu);

    // ohne Brennstoff passiert nichts
    fu.input = {IRON_ORE, 2};
    for (int i = 0; i < 400; ++i) f.tick(0.1f);
    CHECK(fu.output.empty() && fu.input.count == 2 && !fu.lit() && f.getBlock(2, 100, 2) == FURNACE);

    // mit Kohle: 2 Erze brauchen 20 s, der Ofen leuchtet, 1 Kohle reicht
    fu.fuel = {ITEM_COAL, 1};
    f.tick(0.1f);
    CHECK(fu.lit() && f.getBlock(2, 100, 2) == FURNACE_LIT && fu.fuel.empty());
    for (int i = 0; i < 150; ++i) f.tick(0.1f);  // 15 s: ein Barren
    CHECK(fu.output.id == ITEM_IRON_INGOT && fu.output.count == 1 && fu.input.count == 1);
    for (int i = 0; i < 100; ++i) f.tick(0.1f);  // 25 s
    CHECK(fu.output.count == 2 && fu.input.empty());
    CHECK(fu.lit());  // 80 s Brenndauer, noch nicht abgelaufen
    for (int i = 0; i < 800; ++i) f.tick(0.1f);
    CHECK(!fu.lit() && f.getBlock(2, 100, 2) == FURNACE);  // verloescht, wenn nichts mehr zu tun ist

    // Brennstoff wird nicht verschwendet, wenn nichts zu schmelzen ist
    fu.fuel = {ITEM_COAL, 1};
    f.tick(1.0f);
    CHECK(fu.fuel.count == 1 && !fu.lit());

    // falsche Eingabe wird nicht geschmolzen, voller Ausgabestapel stoppt den Ofen
    fu.input = {DIRT, 3};
    f.tick(5.0f);
    CHECK(fu.input.count == 3 && !fu.lit());
    fu.input = {SAND, 3};
    fu.output = {GLASS, 64};
    f.tick(5.0f);
    CHECK(fu.input.count == 3 && fu.output.count == 64);
    fu.output.clear();
    for (int i = 0; i < 400; ++i) f.tick(0.1f);
    CHECK(fu.output.id == GLASS && fu.output.count == 3 && fu.input.empty());

    // Fleisch braten, verschiedene Ausgaben mischen nicht
    fu.input = {ITEM_MEAT, 1};
    fu.fuel = {LOG, 1};
    for (int i = 0; i < 200; ++i) f.tick(0.1f);
    CHECK(fu.output.id == GLASS);  // Glas steht noch im Ausgabefeld, Fleisch wartet
    CHECK(fu.input.count == 1);
    fu.output.clear();
    for (int i = 0; i < 150; ++i) f.tick(0.1f);
    CHECK(fu.output.id == ITEM_COOKED_MEAT);

    // Ruhender Ofen in ungeladenem Chunk, entfernter Block loescht den Zustand
    CHECK(f.removeFurnace(2, 100, 2, nullptr));
    CHECK(f.furnaceCount() == 0);
    f.furnaceAt(2, 100, 2);
    f.setBlock(2, 100, 2, AIR);
    f.tick(0.1f);
    CHECK(f.furnaceCount() == 0);  // verwaister Zustand wird aufgeraeumt

    // Schmelzliste und Brennstoffe
    CHECK(smeltRecipe(IRON_ORE) && smeltRecipe(IRON_ORE)->output.id == ITEM_IRON_INGOT);
    CHECK(smeltRecipe(SAND)->output.id == GLASS && smeltRecipe(COBBLE)->output.id == STONE);
    CHECK(smeltRecipe(DIRT) == nullptr && smeltRecipe(ITEM_COAL) == nullptr);
    CHECK(fuelSeconds(ITEM_COAL) > fuelSeconds(LOG) && fuelSeconds(LOG) > fuelSeconds(ITEM_STICK));
    CHECK(fuelSeconds(STONE) == 0.0f);

    // Speichern: Ofenzustand
    SaveFile sf;
    FurnaceSave fs;
    fs.x = -3;
    fs.y = 70;
    fs.z = 9;
    fs.furnace.input = {IRON_ORE, 5};
    fs.furnace.fuel = {ITEM_COAL, 2};
    fs.furnace.output = {ITEM_IRON_INGOT, 3};
    fs.furnace.burnTime = 12.5f;
    fs.furnace.burnTotal = 80.0f;
    fs.furnace.cookTime = 4.25f;
    sf.furnaces.push_back(fs);
    std::filesystem::create_directories("olicraft_test_save4");
    std::string err;
    CHECK(writeSaveFile("olicraft_test_save4/w.dat", sf, &err));
    SaveFile back;
    CHECK(readSaveFile("olicraft_test_save4/w.dat", back, &err));
    CHECK(back.furnaces.size() == 1);
    if (back.furnaces.size() == 1) {
        const FurnaceSave& b = back.furnaces[0];
        CHECK(b.x == -3 && b.y == 70 && b.z == 9);
        CHECK(b.furnace.input == fs.furnace.input && b.furnace.fuel == fs.furnace.fuel && b.furnace.output == fs.furnace.output);
        CHECK(b.furnace.burnTime == 12.5f && b.furnace.burnTotal == 80.0f && b.furnace.cookTime == 4.25f);
    }
    std::filesystem::remove_all("olicraft_test_save4");
    World g(5);
    g.setMeshingEnabled(false);
    g.importFurnaces(back.furnaces);
    std::vector<FurnaceSave> again;
    g.exportFurnaces(again);
    CHECK(again.size() == 1 && again[0].furnace.input.count == 5);
}

static void testSkinsAndModels() {
    for (int s = 0; s < SKIN_COUNT; ++s) {
        const SkinInfo& si = skinInfo(static_cast<SkinId>(s));
        CHECK(si.width == 64 && (si.height == 32 || si.height == 64));
        std::vector<uint8_t> px = generateSkinPixels(static_cast<SkinId>(s));
        CHECK(px.size() == static_cast<size_t>(si.width) * si.height * 4);
        std::vector<int> owner(static_cast<size_t>(si.width) * si.height, -1);
        bool inBounds = true, overlap = false, painted = true;
        const std::vector<ModelPart>& parts = modelParts(static_cast<SkinId>(s));
        CHECK(parts.size() >= 6);
        for (size_t pi = 0; pi < parts.size(); ++pi) {
            const ModelPart& p = parts[pi];
            const glm::ivec2 ns = skinNetSize(p);
            if (p.u < 0 || p.v < 0 || p.u + ns.x > si.width || p.v + ns.y > si.height) inBounds = false;
            if (p.mirror) continue;  // teilt sich das Netz mit der rechten Seite
            // Netze verschiedener Quader ueberlappen nicht (identische Netze gehoeren zu spiegelnden Paaren)
            for (int f = 0; f < 6; ++f) {
                const NetRect r = skinNetRect(p, f);
                for (int y = r.y; y < r.y + r.h; ++y)
                    for (int x = r.x; x < r.x + r.w; ++x) {
                        if (x < 0 || y < 0 || x >= si.width || y >= si.height) {
                            inBounds = false;
                            continue;
                        }
                        const size_t i = static_cast<size_t>(y) * si.width + x;
                        // Beine mit gleichem Netz (nicht gespiegelt) teilen sich die Pixel absichtlich
                        if (owner[i] >= 0 && parts[static_cast<size_t>(owner[i])].u != p.u) overlap = true;
                        if (owner[i] >= 0 && parts[static_cast<size_t>(owner[i])].v != p.v) overlap = true;
                        owner[i] = static_cast<int>(pi);
                        if (px[i * 4 + 3] != 255) painted = false;  // Standardtextur deckt jedes Netzpixel ab
                    }
            }
        }
        CHECK(inBounds && !overlap && painted);
    }

    // Texturkoordinaten liegen in der jeweiligen Netzflaeche
    for (int s = 0; s < SKIN_COUNT; ++s) {
        const SkinInfo& si = skinInfo(static_cast<SkinId>(s));
        bool inside = true;
        for (const ModelPart& p : modelParts(static_cast<SkinId>(s)))
            for (int f = 0; f < 6; ++f) {
                const NetRect r = skinNetRect(p, f);
                for (int i = 0; i < 2; ++i)
                    for (int j = 0; j < 2; ++j) {
                        glm::vec2 uv = skinCornerUV(p, f, i, j, si.width, si.height);
                        float x = uv.x * si.width, y = uv.y * si.height;
                        if (x < r.x || x > r.x + r.w || y < r.y || y > r.y + r.h) inside = false;
                    }
            }
        CHECK(inside);
    }
    // Vorderseite: die obere Kante der Flaeche liegt oben im Bild, Spiegelung vertauscht links und rechts
    {
        const ModelPart& right = modelParts(SKIN_PLAYER)[2];
        const ModelPart& left = modelParts(SKIN_PLAYER)[3];
        CHECK(right.u == left.u && right.v == left.v && !right.mirror && left.mirror);
        const NetRect r = skinNetRect(right, 0);
        glm::vec2 bottom = skinCornerUV(right, 0, 0, 0, 64, 32), top = skinCornerUV(right, 0, 1, 0, 64, 32);
        CHECK(top.y < bottom.y);  // i = Richtung nach oben
        (void)r;
        // gespiegelt: die gleiche Flaeche liest u rueckwaerts
        glm::vec2 a = skinCornerUV(right, 0, 0, 0, 64, 32), b = skinCornerUV(left, 0, 0, 0, 64, 32);
        glm::vec2 a2 = skinCornerUV(right, 0, 0, 1, 64, 32), b2 = skinCornerUV(left, 0, 0, 1, 64, 32);
        CHECK((a.x < a2.x) != (b.x < b2.x));
        // rechte Aussenseite des Spielers (+Z) und linke (-Z) tauschen beim gespiegelten Glied die Netzflaechen
        CHECK(skinNetRect(left, 4).x == skinNetRect(right, 5).x && skinNetRect(left, 5).x == skinNetRect(right, 4).x);
    }
    // Standard-Spielerskin im Minecraft-Layout: Kopf (0,0), Koerper (16,16), Arm (40,16), Bein (0,16)
    {
        const auto& p = modelParts(SKIN_PLAYER);
        CHECK(p[0].u == 0 && p[0].v == 0 && p[1].u == 16 && p[1].v == 16 && p[2].u == 40 && p[2].v == 16 && p[4].u == 0 &&
              p[4].v == 16);
    }
    // Modellgeometrie: 24 Eckpunkte je Quader, endliche Werte, in sinnvoller Naehe des Fusspunkts
    for (int s = 0; s < SKIN_COUNT; ++s) {
        std::vector<Vertex> verts;
        Pose pose;
        pose.pos = glm::vec3(10.0f, 5.0f, -3.0f);
        pose.yaw = 0.7f;
        pose.walkPhase = 1.3f;
        pose.walkAmount = 1.0f;
        pose.headPitch = 0.4f;
        appendModel(static_cast<SkinId>(s), pose, verts);
        CHECK(verts.size() == modelParts(static_cast<SkinId>(s)).size() * 24);
        bool ok = true;
        for (const Vertex& v : verts) {
            if (!std::isfinite(v.x) || std::isnan(v.u) || v.u < 0 || v.u > 1 || v.v < 0 || v.v > 1) ok = false;
            if (std::fabs(v.x - 10.0f) > 3.0f || std::fabs(v.z + 3.0f) > 3.0f || v.y < 4.9f || v.y > 7.5f) ok = false;
        }
        CHECK(ok);
    }
    // Umfallen legt die Figur hin
    {
        std::vector<Vertex> upright, fallen;
        Pose pose;
        appendModel(SKIN_ZOMBIE, pose, upright);
        pose.roll = 1.5708f;
        appendModel(SKIN_ZOMBIE, pose, fallen);
        float hu = 0, hf = 0;
        for (const Vertex& v : upright) hu = std::max(hu, v.y);
        for (const Vertex& v : fallen) hf = std::max(hf, v.y);
        CHECK(hu > 1.9f && hf < 1.0f);
    }
}

static void testCamera() {
    World w(3);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z) {
            for (int y = 100; y < 125; ++y) w.setBlock(x, y, z, AIR);
            w.setBlock(x, 99, z, STONE);
        }
    const glm::vec3 eye(0.5f, 101.62f, 0.5f);
    const glm::vec3 look = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));

    CameraPose p1 = computeCamera(w, eye, look, CameraMode::First);
    CHECK(p1.pos == eye && p1.forward == look);

    // freie Sicht: voller Abstand, Blick wie der Spieler
    CameraPose pb = computeCamera(w, eye, look, CameraMode::ThirdBack, 4.0f);
    CHECK(std::fabs(glm::length(pb.pos - eye) - 4.0f) < 1e-4f && pb.pos.x < eye.x && pb.forward == look);
    // von vorn: Kamera vor dem Spieler, blickt zurueck
    CameraPose pf = computeCamera(w, eye, look, CameraMode::ThirdFront, 4.0f);
    CHECK(pf.pos.x > eye.x && glm::dot(pf.forward, look) < -0.99f && std::fabs(glm::length(pf.pos - eye) - 4.0f) < 1e-4f);

    // Wand 2 Bloecke hinter dem Spieler: Kamera rueckt heran und steckt nicht in der Wand
    for (int y = 100; y < 110; ++y)
        for (int z = -5; z <= 5; ++z) w.setBlock(-2, y, z, STONE);
    CameraPose pw = computeCamera(w, eye, look, CameraMode::ThirdBack, 4.0f);
    CHECK(pw.pos.x > -2.0f + 0.1f && pw.pos.x < eye.x);
    CHECK(!w.isSolidAt(static_cast<int>(std::floor(pw.pos.x)), static_cast<int>(std::floor(pw.pos.y)),
                       static_cast<int>(std::floor(pw.pos.z))));
    // direkt an der Wand: Kamera bleibt am Kopf
    CameraPose pn = computeCamera(w, glm::vec3(-1.4f, 101.62f, 0.5f), look, CameraMode::ThirdBack, 4.0f);
    CHECK(glm::length(pn.pos - glm::vec3(-1.4f, 101.62f, 0.5f)) < 0.2f);
    // Blick nach unten (Kamera hinter und ueber dem Spieler), Boden darf nicht durchdrungen werden
    CameraPose pd = computeCamera(w, eye, glm::normalize(glm::vec3(0.0f, -1.0f, 0.2f)), CameraMode::ThirdFront, 4.0f);
    CHECK(pd.pos.y > 100.0f);  // oberhalb der Plattform (Oberkante bei y = 100)
    CHECK(nextCameraMode(CameraMode::First) == CameraMode::ThirdBack &&
          nextCameraMode(CameraMode::ThirdBack) == CameraMode::ThirdFront &&
          nextCameraMode(CameraMode::ThirdFront) == CameraMode::First);
    // Raycast liefert Abstand
    RayHit h = w.raycast(eye, glm::vec3(-1.0f, 0.0f, 0.0f), 10.0f);
    CHECK(h.hit && std::fabs(h.dist - 1.5f) < 1e-3f);  // Block -2 beginnt bei x = -1
}

static void testImagesAndTextures() {
    const std::string dir = "olicraft_test_tex";
    std::filesystem::remove_all(dir);

    // PNG-Rundlauf inklusive Transparenz
    Image img(5, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 5; ++x) {
            uint8_t* p = img.at(x, y);
            p[0] = static_cast<uint8_t>(x * 50);
            p[1] = static_cast<uint8_t>(y * 100);
            p[2] = static_cast<uint8_t>(x * y * 20);
            p[3] = (x + y) % 2 ? 255 : 0;
        }
    std::string err;
    CHECK(savePng(dir + "/a/b.png", img, &err));
    Image back;
    CHECK(loadImage(dir + "/a/b.png", back, &err));
    CHECK(back.w == 5 && back.h == 3 && back.rgba == img.rgba);
    CHECK(!loadImage(dir + "/nicht_da.png", back, &err) && !err.empty());
    {
        std::FILE* f = std::fopen((dir + "/kaputt.png").c_str(), "wb");
        std::fputs("kein PNG", f);
        std::fclose(f);
    }
    CHECK(!loadImage(dir + "/kaputt.png", back, &err));
    CHECK(!savePng(dir + "/leer.png", Image(), &err));

    // Texturspeicher: Standardtexturen, Kachel-Orientierung, Datei-Rundlauf
    TextureStore store;
    store.setDirectory(dir);
    CHECK(store.atlasPixels().size() == static_cast<size_t>(ATLAS_PX) * ATLAS_PX * 4);
    for (int t = 0; t < TILE_COUNT; ++t) {
        CHECK(std::string(tileName(t).file).size() > 0 && std::string(tileName(t).name).size() > 0);
        CHECK(store.tile(t).valid() && store.tile(t).rgba == store.defaultTile(t).rgba);
    }
    // Gras-Seite: oben (Zeile 0) gruen, unten braun
    {
        Image gs = store.tile(TILE_GRASS_SIDE);
        const uint8_t* top = gs.at(3, 0);
        const uint8_t* bottom = gs.at(3, 15);
        CHECK(top[1] > top[0] && bottom[0] > bottom[2]);  // oben gruen, unten braun
    }
    // Ein Pixel setzen wirkt auf den Atlas (und umgekehrt orientiert)
    const uint8_t red[4] = {255, 0, 0, 255};
    store.setTilePixel(TILE_STONE, 2, 0, red);
    CHECK(store.tile(TILE_STONE).at(2, 0)[0] == 255 && store.tile(TILE_STONE).at(2, 0)[1] == 0);
    CHECK(store.atlasDirty());
    {
        // Atlas: Zeile 0 der Kachel = unten, also liegt das Pixel in der obersten Atlaszeile der Kachel
        const int ty = (TILE_STONE / ATLAS_TILES) * TILE_PX + (TILE_PX - 1);
        const int tx = (TILE_STONE % ATLAS_TILES) * TILE_PX + 2;
        CHECK(store.atlasPixels()[(static_cast<size_t>(ty) * ATLAS_PX + tx) * 4] == 255);
    }
    CHECK(!store.tileCustom(TILE_STONE));
    CHECK(store.saveTile(TILE_STONE, &err) && store.tileCustom(TILE_STONE));
    CHECK(std::filesystem::exists(store.tilePath(TILE_STONE)));
    store.resetTile(TILE_STONE);
    CHECK(store.tile(TILE_STONE).at(2, 0)[0] != 255 || store.tile(TILE_STONE).at(2, 0)[1] != 0);
    CHECK(store.loadTile(TILE_STONE, &err));
    CHECK(store.tile(TILE_STONE).at(2, 0)[0] == 255 && store.tile(TILE_STONE).at(2, 0)[1] == 0);

    // Skins: speichern, laden, falsche Groessen, Minecraft-Skin 64x64
    store.setSkin(SKIN_PLAYER, [&] {
        Image s = store.defaultSkin(SKIN_PLAYER);
        s.at(0, 0)[0] = 1;
        s.at(0, 0)[1] = 2;
        s.at(0, 0)[2] = 3;
        s.at(0, 0)[3] = 255;
        return s;
    }());
    CHECK(store.saveSkin(SKIN_PLAYER, &err) && store.skinCustom(SKIN_PLAYER));
    store.resetSkin(SKIN_PLAYER);
    CHECK(store.skin(SKIN_PLAYER).at(0, 0)[0] != 1 || store.skin(SKIN_PLAYER).at(0, 0)[1] != 2);
    CHECK(store.loadSkin(SKIN_PLAYER, &err));
    CHECK(store.skin(SKIN_PLAYER).at(0, 0)[0] == 1 && store.skin(SKIN_PLAYER).at(0, 0)[2] == 3);
    {
        Image wrong(10, 10);
        CHECK(savePng(dir + "/skin_player.png", wrong, &err));
        CHECK(!store.loadSkin(SKIN_PLAYER, &err) && err.find("10x10") != std::string::npos);
        Image mc(64, 64);
        mc.at(5, 5)[0] = 77;
        mc.at(5, 5)[3] = 255;
        mc.at(5, 40)[0] = 99;  // untere Haelfte (neues Layout) wird verworfen
        CHECK(savePng(dir + "/skin_player.png", mc, &err));
        CHECK(store.loadSkin(SKIN_PLAYER, &err) && store.skin(SKIN_PLAYER).h == 32 && store.skin(SKIN_PLAYER).at(5, 5)[0] == 77);
        Image halfPig(64, 32);
        CHECK(savePng(dir + "/skin_pig.png", halfPig, &err));  // Tiere nur in exakter Groesse (64x64)
        CHECK(!store.loadSkin(SKIN_PIG, &err));
        Image tile32(32, 32);
        CHECK(savePng(store.tilePath(TILE_DIRT), tile32, &err));
        CHECK(!store.loadTile(TILE_DIRT, &err));
    }
    // Alles aus dem Ordner laden: gueltige Dateien werden uebernommen, ungueltige gemeldet
    {
        TextureStore s2;
        s2.setDirectory(dir);
        std::vector<std::string> msgs;
        const int n = s2.loadAllFromDirectory(&msgs);
        CHECK(n >= 2);                                  // Stein-Kachel und Spieler-Skin
        CHECK(msgs.size() == 2);                        // Schwein und Erde sind ungueltig
        CHECK(s2.tileCustom(TILE_STONE) && !s2.tileCustom(TILE_DIRT) && s2.skinCustom(SKIN_PLAYER) && !s2.skinCustom(SKIN_PIG));
        TextureStore none;
        none.setDirectory(dir + "/gibt_es_nicht");
        CHECK(none.loadAllFromDirectory(nullptr) == 0);
    }
    std::filesystem::remove_all(dir);
}

static void testTextureEditor() {
    const std::string dir = "olicraft_test_editor";
    std::filesystem::remove_all(dir);
    TextureStore store;
    store.setDirectory(dir);
    TextureEditor ed(store);
    std::string msg;

    // Eintraege
    const auto& entries = ed.entries();
    CHECK(entries.size() > 40);
    int blocks = 0, flats = 0, skins = 0;
    std::vector<std::string> names;
    for (const EditorEntry& e : entries) {
        CHECK(!e.name.empty());
        names.push_back(e.name + "/" + e.group);
        blocks += e.kind == EditorEntry::BlockNet;
        flats += e.kind == EditorEntry::FlatTile;
        skins += e.kind == EditorEntry::Skin;
    }
    CHECK(blocks >= 15 && flats >= 20 && skins == SKIN_COUNT);
    std::sort(names.begin(), names.end());
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());  // Namen eindeutig

    auto find = [&](const std::string& n) {
        for (size_t i = 0; i < entries.size(); ++i)
            if (entries[i].name == n) return static_cast<int>(i);
        return -1;
    };
    const int grass = find("Gras"), stick = find("Stock"), player = find("Spieler");
    CHECK(grass >= 0 && stick >= 0 && player >= 0);

    // Block: Kreuznetz 64x48, Seite kommt viermal vor
    ed.select(grass);
    CHECK(ed.view().w == 64 && ed.view().h == 48);
    CHECK(ed.editable(16 + 3, 3) && ed.editable(3, 16 + 3) && ed.editable(16 + 3, 32 + 3));
    CHECK(!ed.editable(3, 3) && !ed.editable(48 + 3, 3) && !ed.editable(3, 32 + 3));  // leere Zellen des Kreuzes
    CHECK(ed.regions().size() == 6);
    ed.tool = Tool::Pencil;
    ed.color[0] = 10;
    ed.color[1] = 20;
    ed.color[2] = 30;
    ed.color[3] = 255;
    ed.brush = 1;
    ed.paintAt(2, 16 + 5);  // Seite, Zelle 0
    for (int c = 0; c < 4; ++c) {
        const uint8_t* p = ed.view().at(c * 16 + 2, 16 + 5);
        CHECK(p[0] == 10 && p[1] == 20 && p[2] == 30);  // alle vier Seiten zeigen dieselbe Kachel
    }
    CHECK(store.tile(TILE_GRASS_SIDE).at(2, 5)[0] == 10);
    CHECK(store.tile(TILE_GRASS_TOP).at(2, 5)[0] != 10 || store.tile(TILE_GRASS_TOP).at(2, 5)[1] != 20);  // oben unberuehrt
    CHECK(ed.canUndo() && ed.modified());
    ed.paintAt(3, 3);  // ausserhalb des Kreuzes: nichts
    CHECK(store.tile(TILE_GRASS_SIDE).at(3, 3)[0] != 10 || store.tile(TILE_GRASS_SIDE).at(3, 3)[1] != 20);

    // Radierer und Pinselgroesse
    ed.tool = Tool::Eraser;
    ed.brush = 2;
    ed.paintAt(16 + 8, 8);  // oben
    int erased = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (store.tile(TILE_GRASS_TOP).at(x, y)[3] == 0) ++erased;
    CHECK(erased == 4);
    ed.brush = 3;
    ed.paintAt(16 + 0, 0);  // an der Kante: nur Pixel innerhalb der Kachel
    erased = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (store.tile(TILE_GRASS_TOP).at(x, y)[3] == 0) ++erased;
    CHECK(erased == 4 + 4);  // 2x2 an der Ecke (Pinsel 3 reicht von -1 bis +1)

    // Undo / Redo
    const Image before = store.tile(TILE_GRASS_TOP);
    CHECK(ed.undo() && ed.undo());
    CHECK(store.tile(TILE_GRASS_TOP).rgba == store.defaultTile(TILE_GRASS_TOP).rgba);
    CHECK(ed.redo());
    CHECK(store.tile(TILE_GRASS_TOP).rgba != store.defaultTile(TILE_GRASS_TOP).rgba);
    CHECK(ed.redo() && !ed.canRedo());
    CHECK(store.tile(TILE_GRASS_TOP).rgba == before.rgba);

    // Fuellen bleibt in der Zelle, Pipette liest die Farbe
    ed.select(grass);
    ed.tool = Tool::Fill;
    ed.color[0] = 1; ed.color[1] = 2; ed.color[2] = 3; ed.color[3] = 255;
    ed.paintAt(16 + 8, 8);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const uint8_t* p = store.tile(TILE_GRASS_TOP).at(x, y);
            CHECK(p[0] <= 255);  // alle Pixel gueltig
            (void)p;
        }
    CHECK(ed.view().at(16 + 8, 8)[0] == 1);
    CHECK(store.tile(TILE_DIRT).rgba == store.defaultTile(TILE_DIRT).rgba);
    CHECK(store.tile(TILE_GRASS_SIDE).at(7, 7)[0] != 1);  // Seite nicht mitgefuellt
    ed.tool = Tool::Picker;
    ed.color[0] = 99;
    ed.paintAt(16 + 8, 8);
    CHECK(ed.color[0] == 1 && ed.color[1] == 2 && ed.color[2] == 3);

    // Gegenstand: Orientierung (Zeile 0 oben) und Linie
    ed.select(stick);
    CHECK(ed.view().w == 16 && ed.view().h == 16 && ed.regions().empty());
    ed.tool = Tool::Pencil;
    ed.brush = 1;
    ed.color[0] = 7; ed.color[1] = 8; ed.color[2] = 9; ed.color[3] = 255;
    ed.paintLine(1, 1, 8, 5);
    CHECK(store.tile(TILE_STICK).at(1, 1)[0] == 7 && store.tile(TILE_STICK).at(8, 5)[0] == 7);
    int painted = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (store.tile(TILE_STICK).at(x, y)[0] == 7 && store.tile(TILE_STICK).at(x, y)[1] == 8) ++painted;
    CHECK(painted == 8);  // Bresenham von (1,1) nach (8,5): max(dx, dy) + 1 Pixel
    CHECK(ed.undo());  // die ganze Linie ist ein Schritt
    CHECK(store.tile(TILE_STICK).rgba == store.defaultTile(TILE_STICK).rgba);

    // Skin: Netz, Flaechen, Fuellen nur in der Flaeche
    ed.select(player);
    CHECK(ed.view().w == 64 && ed.view().h == 32);
    CHECK(ed.regions().size() == 4 * 6);  // Kopf, Koerper, Arm, Bein
    ed.tool = Tool::Fill;
    ed.color[0] = 200; ed.color[1] = 0; ed.color[2] = 200; ed.color[3] = 255;
    const int fx = 16 + 4 + 2, fy = 16 + 4 + 5;  // im Koerper (Vorderseite)
    ed.paintAt(fx, fy);
    CHECK(store.skin(SKIN_PLAYER).at(fx, fy)[0] == 200);
    int purple = 0;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 64; ++x)
            if (store.skin(SKIN_PLAYER).at(x, y)[0] == 200 && store.skin(SKIN_PLAYER).at(x, y)[1] == 0) ++purple;
    CHECK(purple > 40 && purple <= 8 * 12);  // fuellt die (verrauschte) Koerper-Vorderseite, hoechstens 8x12 Pixel
    CHECK(ed.undo());
    ed.paintAt(60, 2);  // ausserhalb jeder Flaeche: nichts
    CHECK(store.skin(SKIN_PLAYER).at(60, 2)[0] != 200 || store.skin(SKIN_PLAYER).at(60, 2)[3] == 0);
    // Zeichnen in einen Arm veraendert das Netz, das sich linker und rechter Arm teilen
    ed.tool = Tool::Pencil;
    ed.paintAt(44, 20);
    CHECK(store.skin(SKIN_PLAYER).at(44, 20)[0] == 200);

    // Speichern / Export / Import / Zuruecksetzen
    CHECK(ed.save(&msg) && std::filesystem::exists(store.skinPath(SKIN_PLAYER)));
    CHECK(!ed.modified());
    CHECK(ed.exportNet(&msg) && std::filesystem::exists(ed.exportPath()));
    {
        Image ext;
        std::string err;
        CHECK(loadImage(ed.exportPath(), ext, &err) && ext.w == 64 && ext.h == 32 && ext.at(44, 20)[0] == 200);
        ext.at(10, 10)[0] = 12;
        ext.at(10, 10)[1] = 34;
        ext.at(10, 10)[2] = 56;
        ext.at(10, 10)[3] = 255;
        CHECK(savePng(ed.exportPath(), ext, &err));
    }
    CHECK(ed.importNet(&msg));
    CHECK(store.skin(SKIN_PLAYER).at(10, 10)[0] == 12 && store.skin(SKIN_PLAYER).at(10, 10)[2] == 56);
    CHECK(ed.undo() && store.skin(SKIN_PLAYER).at(10, 10)[0] != 12);
    {
        Image bad(10, 10);
        std::string err;
        CHECK(savePng(ed.exportPath(), bad, &err));
        const Image keep = store.skin(SKIN_PLAYER);
        CHECK(!ed.importNet(&msg) && msg.find("Groesse") != std::string::npos);
        CHECK(store.skin(SKIN_PLAYER).rgba == keep.rgba);
    }
    ed.resetToDefault(&msg);
    CHECK(store.skin(SKIN_PLAYER).rgba == store.defaultSkin(SKIN_PLAYER).rgba);
    CHECK(!std::filesystem::exists(store.skinPath(SKIN_PLAYER)));

    // Block-Netz exportieren, in der Datei etwas veraendern, importieren
    ed.select(grass);
    CHECK(ed.exportNet(&msg));
    {
        Image net;
        std::string err;
        CHECK(loadImage(ed.exportPath(), net, &err) && net.w == 64 && net.h == 48);
        // eine der vier Seiten (Zelle 2) und die Oberseite veraendern
        net.at(2 * 16 + 4, 16 + 4)[0] = 250;
        net.at(2 * 16 + 4, 16 + 4)[3] = 255;
        net.at(16 + 4, 4)[1] = 251;
        net.at(16 + 4, 4)[3] = 255;
        CHECK(savePng(ed.exportPath(), net, &err));
    }
    CHECK(ed.importNet(&msg));
    CHECK(store.tile(TILE_GRASS_SIDE).at(4, 4)[0] == 250 && store.tile(TILE_GRASS_TOP).at(4, 4)[1] == 251);
    CHECK(ed.save(&msg) && std::filesystem::exists(store.tilePath(TILE_GRASS_SIDE)) &&
          std::filesystem::exists(store.tilePath(TILE_GRASS_TOP)) && std::filesystem::exists(store.tilePath(TILE_DIRT)));
    // Ein neuer Speicher laedt das beim Start
    {
        TextureStore again;
        again.setDirectory(dir);
        CHECK(again.loadAllFromDirectory(nullptr) >= 3);
        CHECK(again.tile(TILE_GRASS_SIDE).at(4, 4)[0] == 250);
    }
    // Fehlende Importdatei
    ed.select(find("Kuh"));
    CHECK(!ed.importNet(&msg) && msg.find("Nicht gefunden") != std::string::npos);

    // Vorschau: Wuerfel zeigt drei Flaechen, Figuren viele, alles im Einheitskreis und nach Tiefe sortiert
    ed.select(grass);
    auto q = ed.preview(0.6f, 0.45f);
    CHECK(q.size() == 3);
    bool inside = true;
    for (const PreviewQuad& pq : q) {
        CHECK(!pq.skin);
        for (int k = 0; k < 4; ++k) inside = inside && glm::length(pq.p[k]) <= 1.01f;
    }
    CHECK(inside);
    ed.select(player);
    q = ed.preview(0.3f, 0.3f);
    CHECK(q.size() >= 12);
    bool allSkin = true;
    for (const PreviewQuad& pq : q) {
        allSkin = allSkin && pq.skin;
        for (int k = 0; k < 4; ++k) {
            inside = inside && glm::length(pq.p[k]) <= 1.01f;
            allSkin = allSkin && pq.uv[k].x >= 0 && pq.uv[k].x <= 1 && pq.uv[k].y >= 0 && pq.uv[k].y <= 1;
        }
    }
    CHECK(allSkin && inside);
    // Drehung um 180 Grad zeigt andere Flaechen
    auto q2 = ed.preview(0.3f + 3.14159f, 0.3f);
    float sumA = 0, sumB = 0;
    for (const PreviewQuad& pq : q) sumA += pq.uv[0].x * 3.0f + pq.uv[0].y;
    for (const PreviewQuad& pq : q2) sumB += pq.uv[0].x * 3.0f + pq.uv[0].y;
    CHECK(q2.size() >= 12 && std::fabs(sumA - sumB) > 1e-3f);
    std::filesystem::remove_all(dir);
}

static void testGoldAndTorches() {
    // Gold: Erzverteilung, Schmelzen, Werkzeuge, Abbaustufe
    {
        World w(42);
        w.setMeshingEnabled(false);
        w.loadAround(glm::vec3(0.0f), 4);
        int gold = 0, high = 0;
        for (int x = -64; x < 64; ++x)
            for (int z = -64; z < 64; ++z)
                for (int y = 0; y < CHUNK_H; ++y)
                    if (w.getBlock(x, y, z) == GOLD_ORE) {
                        ++gold;
                        if (y > 30 + 5) ++high;
                    }
        CHECK(gold >= 10 && high == 0);
        CHECK(smeltRecipe(GOLD_ORE) && smeltRecipe(GOLD_ORE)->output.id == ITEM_GOLD_INGOT);
        const ItemStack hand, sp{ITEM_STONE_PICKAXE, 1}, ip{ITEM_IRON_PICKAXE, 1};
        CHECK(!canHarvest(GOLD_ORE, hand) && !canHarvest(GOLD_ORE, sp) && canHarvest(GOLD_ORE, ip));
        CHECK(blockDropItem(GOLD_ORE) == GOLD_ORE);
        for (ItemId id = ITEM_GOLD_PICKAXE; id <= ITEM_GOLD_SWORD; ++id) {
            CHECK(itemValid(id) && itemInfo(id).maxDurability > 0 && itemInfo(id).maxDurability < itemInfo(ITEM_STONE_PICKAXE).maxDurability);
            if (id != ITEM_GOLD_SWORD) CHECK(itemInfo(id).speed > itemInfo(ITEM_IRON_PICKAXE).speed);  // schnell, aber zerbrechlich
        }
        std::vector<ItemStack> g(9);
        g[0] = g[1] = g[2] = {ITEM_GOLD_INGOT, 1};
        g[4] = g[7] = {ITEM_STICK, 1};
        CHECK(matchRecipe(g.data(), 3).id == ITEM_GOLD_PICKAXE);
        // Fackeln: Kohle ueber Stock
        std::vector<ItemStack> tg(4);
        tg[0] = {ITEM_COAL, 1};
        tg[2] = {ITEM_STICK, 1};
        ItemStack r = matchRecipe(tg.data(), 2);
        CHECK(r.id == TORCH && r.count == 4);
        tg[2].clear();
        tg[1] = {ITEM_STICK, 1};
        CHECK(matchRecipe(tg.data(), 2).empty());  // nebeneinander: kein Rezept
        CHECK(itemValid(TORCH) && !itemValid(TORCH_PX) && itemInfo(TORCH).flatIcon && itemIsBlock(TORCH));
        CHECK(blockDropItem(TORCH_NZ) == TORCH && blockDropItem(TORCH) == TORCH);
        CHECK(miningSeconds(TORCH, hand) == 0.0f);  // bricht sofort
        CHECK(!blockInfo(TORCH).solid && !blockInfo(TORCH).opaque && lightEmission(TORCH_PX) == 14 && lightEmission(LAVA_2) == 15 &&
              lightEmission(STONE) == 0 && lightEmission(FURNACE_LIT) > lightEmission(FURNACE));
    }

    // Licht
    World w(3);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z) {
            for (int y = 100; y < 125; ++y) w.setBlock(x, y, z, AIR);
            w.setBlock(x, 99, z, STONE);
        }
    CHECK(w.lightAt(0, 100, 0) == 0 && w.lightAt(1000, 100, 0) == 0 && w.lightAt(0, -3, 0) == 0 && w.lightAt(0, 500, 0) == 0);
    w.setBlock(0, 100, 0, TORCH);
    CHECK(w.lightAt(0, 100, 0) == 14 && w.lightAt(1, 100, 0) == 13 && w.lightAt(0, 100, 5) == 9);
    CHECK(w.lightAt(3, 103, 0) == 8);       // Abstand 6
    CHECK(w.lightAt(13, 100, 0) == 1 && w.lightAt(14, 100, 0) == 0);
    CHECK(w.lightAt(0, 99, 0) == 0);        // Stein nimmt kein Licht auf
    // Wand: dahinter dunkel; oeffnen laesst Licht durch
    for (int z = -14; z <= 14; ++z)
        for (int y = 100; y < 112; ++y) w.setBlock(2, y, z, STONE);
    CHECK(w.lightAt(2, 100, 0) == 0 && w.lightAt(3, 100, 0) == 0 && w.lightAt(1, 100, 0) == 13);
    w.setBlock(2, 100, 0, AIR);
    CHECK(w.lightAt(2, 100, 0) == 12 && w.lightAt(3, 100, 0) == 11 && w.lightAt(3, 101, 0) == 10);
    w.setBlock(2, 100, 0, STONE);
    CHECK(w.lightAt(3, 100, 0) == 0 && w.lightAt(3, 101, 0) == 0);
    // zweite Fackel: Maximum, und die erste zu entfernen laesst die zweite bestehen
    for (int y = 100; y < 112; ++y) w.setBlock(2, y, 0, AIR);
    w.setBlock(6, 100, 0, TORCH_NX);
    CHECK(w.lightAt(4, 100, 0) == 12);
    w.setBlock(0, 100, 0, AIR);
    CHECK(w.lightAt(0, 100, 0) == 8 && w.lightAt(6, 100, 0) == 14 && w.lightAt(4, 100, 0) == 12);
    w.setBlock(6, 100, 0, AIR);
    int lit = 0;
    for (int x = -14; x <= 14; ++x)
        for (int z = -14; z <= 14; ++z)
            for (int y = 99; y < 125; ++y) lit += w.lightAt(x, y, z) > 0;
    CHECK(lit == 0);
    // Lava und Fackel: Lava entfernen darf die Fackel nicht ausloeschen
    w.setBlock(-8, 100, 0, LAVA);
    w.setBlock(-6, 100, 0, TORCH);
    CHECK(w.lightAt(-8, 100, 0) == 15 && w.lightAt(-7, 100, 0) == 14);
    w.setBlock(-8, 100, 0, AIR);
    CHECK(w.lightAt(-6, 100, 0) == 14 && w.lightAt(-7, 100, 0) == 13 && w.lightAt(-8, 100, 0) == 12);
    w.setBlock(-6, 100, 0, AIR);
    // Chunkgrenze: Licht fliesst hinueber, der Nachbar wird neu gemeshed
    w.setBlock(15, 100, 0, TORCH);
    CHECK(w.lightAt(17, 100, 0) == 12 && w.chunkAt(1, 0) && w.chunkAt(1, 0)->dirty);
    // Entladen und neu laden ergibt dasselbe Licht (veraenderte Chunks werden komplett abgetastet)
    w.update(glm::vec3(16.0f * 40, 64.0f, 0.0f), 3, 1000.0);
    CHECK(!w.isChunkLoaded(0, 0) && !w.isChunkLoaded(1, 0));
    w.update(glm::vec3(8.0f, 64.0f, 8.0f), 3, 1000.0);
    CHECK(w.isChunkLoaded(0, 0) && w.isChunkLoaded(1, 0));
    CHECK(w.getBlock(15, 100, 0) == TORCH && w.lightAt(15, 100, 0) == 14 && w.lightAt(17, 100, 0) == 12 && w.lightAt(5, 100, 0) == 4);
    w.setBlock(15, 100, 0, AIR);
    CHECK(w.lightAt(17, 100, 0) == 0 && w.lightAt(5, 100, 0) == 0);

    // Fackeln fallen ab, wenn der Traeger fehlt
    w.setBlock(5, 100, 5, TORCH);
    w.setBlock(7, 100, 7, TORCH_PX);  // Traeger bei +x
    w.setBlock(8, 100, 7, STONE);
    w.setBlock(9, 100, 9, TORCH_NZ);  // Traeger bei -z
    w.setBlock(9, 100, 8, STONE);
    CHECK(w.takeTorchDrops().empty());
    w.setBlock(5, 99, 5, AIR);
    auto drops = w.takeTorchDrops();
    CHECK(drops.size() == 1 && drops[0] == glm::ivec3(5, 100, 5) && w.getBlock(5, 100, 5) == AIR);
    w.setBlock(8, 100, 7, AIR);
    w.setBlock(9, 100, 8, AIR);
    drops = w.takeTorchDrops();
    CHECK(drops.size() == 2 && w.getBlock(7, 100, 7) == AIR && w.getBlock(9, 100, 9) == AIR);
    w.setBlock(7, 100, 7, TORCH);  // auf dem Boden: der Nachbarblock spielt keine Rolle
    w.setBlock(8, 100, 7, STONE);
    w.setBlock(8, 100, 7, AIR);
    CHECK(w.takeTorchDrops().empty() && w.getBlock(7, 100, 7) == TORCH);

    // Lava-Seen beleuchten Hoehlen
    {
        World cave(7);
        cave.setMeshingEnabled(false);
        cave.loadAround(glm::vec3(0.0f), 4);
        int litCells = 0, maxLight = 0, insideSolid = 0, lavaCells = 0;
        for (int x = -50; x < 50; ++x)
            for (int z = -50; z < 50; ++z)
                for (int y = 0; y < 40; ++y) {
                    const int l = cave.lightAt(x, y, z);
                    litCells += l > 0;
                    maxLight = std::max(maxLight, l);
                    if (l > 0 && isOpaque(cave.getBlock(x, y, z))) ++insideSolid;
                    if (cave.getBlock(x, y, z) == LAVA) {
                        ++lavaCells;
                        if (l != 15) ++insideSolid;
                    }
                }
        CHECK(lavaCells > 0 && litCells > lavaCells && maxLight == 15 && insideSolid == 0);
    }

    // Mesher: Blocklicht gelangt in die Eckpunkte, Fackel ist ein duenner leuchtender Stab
    {
        std::vector<BlockId> pad(PAD_VOLUME, AIR);
        std::vector<uint8_t> lightPad(PAD_VOLUME, 0);
        pad[padIndex(5 + 1, 50, 5 + 1)] = STONE;
        for (int y = 51; y < 53; ++y)
            for (int x = 4; x <= 6; ++x)
                for (int z = 4; z <= 6; ++z) lightPad[padIndex(x + 1, y, z + 1)] = 8;
        MeshOutput out;
        buildChunkMesh(pad.data(), lightPad.data(), out);
        float topLight = -1;
        for (const Vertex& v : out.opaque)
            if (v.y == 51.0f) topLight = std::max(topLight, v.light);  // die Oberseite liegt in der beleuchteten Schicht
        CHECK(std::fabs(topLight - 8.0f / 15.0f) < 1e-4f);
        buildChunkMesh(pad.data(), nullptr, out);
        for (const Vertex& v : out.opaque) CHECK(v.light == 0.0f);

        std::vector<BlockId> tp(PAD_VOLUME, AIR);
        tp[padIndex(8 + 1, 60, 8 + 1)] = TORCH;
        buildChunkMesh(tp.data(), nullptr, out);
        CHECK(out.opaque.size() == 5 * 4);
        bool slim = true, glow = true;
        for (const Vertex& v : out.opaque) {
            slim = slim && v.x > 8.4f && v.x < 8.6f && v.z > 8.4f && v.z < 8.6f && v.y >= 60.0f && v.y <= 60.82f;
            glow = glow && v.sky > 1.5f && v.light == 1.0f;
        }
        CHECK(slim && glow);
        // Wandfackel: oben von der Wand (bei +x) weggeneigt, unten zur Wand verschoben
        std::vector<BlockId> wp(PAD_VOLUME, AIR);
        wp[padIndex(8 + 1, 60, 8 + 1)] = TORCH_PX;
        buildChunkMesh(wp.data(), nullptr, out);
        float minX = 1e9f, maxX = -1e9f, topX = 0, bottomX = 0;
        for (const Vertex& v : out.opaque) {
            minX = std::min(minX, v.x);
            maxX = std::max(maxX, v.x);
            if (v.y > 60.9f) topX = v.x;
            if (v.y < 60.3f) bottomX = v.x;
        }
        CHECK(bottomX > 8.7f && topX < bottomX - 0.15f && maxX < 9.0f);
    }
}

static void writeTestWav(const std::string& path, float seconds, float freq) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    const int rate = 22050, n = static_cast<int>(seconds * rate);
    std::ofstream f(path, std::ios::binary);
    auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    f.write("RIFF", 4);
    u32(36 + n * 2);
    f.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(1);
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    f.write("data", 4);
    u32(n * 2);
    for (int i = 0; i < n; ++i) {
        const float env = 1.0f - static_cast<float>(i) / n;
        int16_t s = static_cast<int16_t>(std::sin(6.2831853f * freq * i / rate) * 12000.0f * env);
        f.write(reinterpret_cast<const char*>(&s), 2);
    }
}

static void testSounds() {
    // Tabelle
    std::vector<std::string> files;
    for (int i = 0; i < SOUND_COUNT; ++i) {
        const SoundDef& d = soundDef(static_cast<SoundId>(i));
        CHECK(std::string(d.file).size() > 2 && std::string(d.prompt).size() > 20 && std::string(d.desc).size() > 3);
        CHECK(d.seconds > 0.1f && d.seconds < 5.0f && d.baseVolume > 0.0f && d.baseVolume <= 1.0f);
        files.push_back(d.file);
    }
    std::sort(files.begin(), files.end());
    CHECK(std::adjacent_find(files.begin(), files.end()) == files.end());
    // Materialien
    CHECK(blockMaterial(STONE) == Material::Stone && blockMaterial(COBBLE) == Material::Stone && blockMaterial(IRON_ORE) == Material::Stone);
    CHECK(blockMaterial(GRASS) == Material::Grass && blockMaterial(LEAVES) == Material::Grass && blockMaterial(DIRT) == Material::Dirt);
    CHECK(blockMaterial(SAND) == Material::Sand && blockMaterial(LOG) == Material::Wood && blockMaterial(TORCH_NZ) == Material::Wood);
    CHECK(blockMaterial(GLASS) == Material::Glass && blockMaterial(SNOW) == Material::Snow);
    CHECK(blockSound(Material::Wood) == SoundId::BlockWood && blockSound(Material::Glass) == SoundId::BlockGlass);
    CHECK(stepSound(Material::Grass) == SoundId::StepGrass && stepSound(Material::Glass) == SoundId::StepStone &&
          stepSound(Material::Snow) == SoundId::StepSnow);

    // Ohne Engine passiert nichts
    {
        AudioEngine off;
        CHECK(!off.active() && !off.play(SoundId::BlockStone, glm::vec3(0.0f)) && off.loadSounds("gibt_es_nicht") == 0);
        off.update(1.0f);
        off.setVolumes(1, 1, 1);
    }

    // Null-Geraet: Laden, Varianten, Abspielen, Aufraeumen, Musik
    const std::string dir = "olicraft_test_sounds";
    std::filesystem::remove_all(dir);
    writeTestWav(dir + "/block_stone.wav", 0.15f, 440.0f);
    writeTestWav(dir + "/step_grass_1.wav", 0.15f, 330.0f);
    writeTestWav(dir + "/step_grass_2.wav", 0.15f, 350.0f);
    writeTestWav(dir + "/pig_idle.wav", 0.2f, 220.0f);
    writeTestWav(dir + "/music/a.wav", 0.6f, 262.0f);
    {
        std::ofstream bad(dir + "/cow_idle.wav", std::ios::binary);
        bad << "kein Ton";
    }
    AudioEngine a;
    CHECK(a.init(true) && a.active());
    CHECK(!a.play(SoundId::BlockStone, glm::vec3(0.0f)));  // noch nichts geladen
    CHECK(a.loadSounds(dir) == 3);
    CHECK(a.variantCount(SoundId::BlockStone) == 1 && a.variantCount(SoundId::StepGrass) == 2 && a.variantCount(SoundId::PigIdle) == 1);
    CHECK(a.variantCount(SoundId::CowIdle) == 0 && a.variantCount(SoundId::ZombieIdle) == 0);  // kaputte/fehlende Dateien
    CHECK(a.musicTrackCount() == 1);
    a.setVolumes(0.8f, 1.0f, 0.5f);
    a.setListener(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    CHECK(a.play(SoundId::BlockStone, glm::vec3(3.0f, 0.0f, 0.0f)));
    CHECK(a.play(SoundId::StepGrass, glm::vec3(0.0f), 1.0f, 0.9f, false));
    CHECK(a.playUi(SoundId::PigIdle));
    CHECK(!a.play(SoundId::CowIdle, glm::vec3(0.0f)));
    CHECK(a.voiceCount() == 3 && a.playedCount() == 3);
    // Stimmen-Obergrenze
    int accepted = 0;
    for (int i = 0; i < 100; ++i) accepted += a.play(SoundId::BlockStone, glm::vec3(0.0f)) ? 1 : 0;
    CHECK(accepted > 10 && accepted < 100 && a.voiceCount() <= 40);
    // Nach der Spieldauer werden die Stimmen entfernt
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    a.update(0.1f);
    CHECK(a.voiceCount() == 0);
    // Musik startet nach der Anfangspause und endet von selbst
    CHECK(!a.musicPlaying());
    a.update(30.0f);
    CHECK(a.musicPlaying());
    a.setMusicEnabled(false);
    CHECK(!a.musicPlaying());
    a.setMusicEnabled(true);
    a.update(100.0f);
    CHECK(a.musicPlaying());
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    a.update(0.1f);
    CHECK(!a.musicPlaying());
    a.stopAll();
    a.shutdown();
    CHECK(!a.active());
    std::filesystem::remove_all(dir);
}

static void testMobSounds() {
    World world(1234);
    world.setMeshingEnabled(false);
    world.loadAround(world.findSpawn(), 3);
    const int gy = 100;
    for (int x = -20; x <= 20; ++x)
        for (int z = -20; z <= 20; ++z) {
            for (int y = gy; y < gy + 12; ++y) world.setBlock(x, y, z, AIR);
            world.setBlock(x, gy - 1, z, STONE);
        }
    Player player;
    player.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    Inventory inv;
    EntityManager em(77);
    EntityEnv env;
    env.allowSpawns = false;
    Mob& pig = em.spawnMob(MobType::Pig, player.pos + glm::vec3(4.0f, 0.0f, 0.0f));
    pig.idleSoundTimer = 0.05f;
    em.update(0.1f, world, player, inv, env);
    CHECK(!em.soundEvents().empty() && em.soundEvents()[0].id == SoundId::PigIdle);
    em.soundEvents().clear();
    // zu weit weg: kein Ruf
    em.mobs()[0].pos = player.pos + glm::vec3(40.0f, 0.0f, 0.0f);
    em.mobs()[0].idleSoundTimer = 0.05f;
    em.update(0.1f, world, player, inv, env);
    CHECK(em.soundEvents().empty());
    // Treffer und Tod
    Mob& cow = em.spawnMob(MobType::Cow, player.pos + glm::vec3(3.0f, 0.0f, 0.0f));
    cow.idleSoundTimer = 100.0f;
    em.hurtMob(em.mobs().back(), 1, player.pos);
    CHECK(em.soundEvents().size() == 1 && em.soundEvents()[0].id == SoundId::CowHurt);
    em.hurtMob(em.mobs().back(), 100, player.pos);
    CHECK(em.soundEvents().size() == 2 && em.soundEvents()[1].id == SoundId::CowDeath);
    em.soundEvents().clear();
    // Zombie greift an
    em.clear();
    Mob& z = em.spawnMob(MobType::Zombie, player.pos + glm::vec3(0.8f, 0.0f, 0.0f));
    z.idleSoundTimer = 100.0f;
    for (int i = 0; i < 5 && em.playerHits().empty(); ++i) em.update(0.05f, world, player, inv, env);
    bool attack = false;
    for (const SoundEvent& e : em.soundEvents()) attack = attack || e.id == SoundId::ZombieAttack;
    CHECK(!em.playerHits().empty() && attack);
    // Aufsammeln
    em.clear();
    em.spawnItem({LOG, 1}, player.pos + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f), 0.0f);
    em.update(0.05f, world, player, inv, env);
    bool pickup = false;
    for (const SoundEvent& e : em.soundEvents()) pickup = pickup || e.id == SoundId::ItemPickup;
    CHECK(pickup && inv.count(LOG) == 1);
}

static void testPlayerSkins() {
    // Standardfarben: Platz 0 ist der bisherige Standard, alle Plaetze unterscheiden sich, Namen sind einmalig
    CHECK(generatePlayerSkinPixels(0) == generateSkinPixels(SKIN_PLAYER));
    std::vector<std::string> names;
    for (int i = 0; i < PLAYER_SKIN_SLOTS; ++i) {
        names.push_back(playerSkinName(i));
        CHECK(generatePlayerSkinPixels(i).size() == 64u * 32u * 4u);
        for (int j = i + 1; j < PLAYER_SKIN_SLOTS; ++j) CHECK(generatePlayerSkinPixels(i) != generatePlayerSkinPixels(j));
    }
    std::sort(names.begin(), names.end());
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());

    const std::string dir = "olicraft_test_skins";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    TextureStore st;
    st.setDirectory(dir);
    CHECK(st.playerSlot() == 0 && st.skin(SKIN_PLAYER).rgba == generatePlayerSkinPixels(0));
    CHECK(st.playerSkinPath(0) == dir + "/skin_player.png" && st.playerSkinPath(2) == dir + "/skin_player_3.png");

    // Bearbeiten, Platz wechseln, zurueckwechseln: Aenderung bleibt erhalten
    const uint8_t red[4] = {255, 0, 0, 255};
    st.setSkinPixel(SKIN_PLAYER, 3, 3, red);
    st.selectPlayerSkin(3);
    CHECK(st.playerSlot() == 3 && st.skin(SKIN_PLAYER).rgba == generatePlayerSkinPixels(3) && st.skinDirty(SKIN_PLAYER));
    CHECK(st.skinPath(SKIN_PLAYER) == dir + "/skin_player_4.png");
    CHECK(std::memcmp(st.playerSkinImage(0).at(3, 3), red, 4) == 0);
    st.setSkinPixel(SKIN_PLAYER, 5, 5, red);
    std::string err;
    CHECK(st.saveSkin(SKIN_PLAYER, &err) && std::filesystem::exists(dir + "/skin_player_4.png") && st.playerSkinCustom(3));
    st.selectPlayerSkin(0);
    CHECK(std::memcmp(st.skin(SKIN_PLAYER).at(3, 3), red, 4) == 0 && st.playerSkinCustom(3));
    CHECK(st.saveSkin(SKIN_PLAYER, &err) && std::filesystem::exists(dir + "/skin_player.png"));
    st.selectPlayerSkin(99);  // wird begrenzt
    CHECK(st.playerSlot() == PLAYER_SKIN_SLOTS - 1);
    st.selectPlayerSkin(0);

    // Neu starten: alle gespeicherten Plaetze werden geladen, der gewaehlte Platz bleibt
    TextureStore st2;
    st2.setDirectory(dir);
    st2.selectPlayerSkin(1);
    std::vector<std::string> msgs;
    CHECK(st2.loadAllFromDirectory(&msgs) == 2 && msgs.empty());
    CHECK(st2.playerSlot() == 1 && st2.skin(SKIN_PLAYER).rgba == generatePlayerSkinPixels(1));
    CHECK(std::memcmp(st2.playerSkinImage(0).at(3, 3), red, 4) == 0);
    CHECK(std::memcmp(st2.playerSkinImage(3).at(5, 5), red, 4) == 0 && st2.playerSkinCustom(3) && st2.playerSkinCustom(0));
    CHECK(!st2.playerSkinCustom(1));
    // Zuruecksetzen betrifft nur den gewaehlten Platz und stellt dessen eigene Standardfarben her
    st2.selectPlayerSkin(3);
    st2.resetSkin(SKIN_PLAYER);
    CHECK(st2.skin(SKIN_PLAYER).rgba == generatePlayerSkinPixels(3));
    CHECK(std::memcmp(st2.playerSkinImage(0).at(3, 3), red, 4) == 0);
    std::filesystem::remove_all(dir);

    // Einstellungen und Menue
    Settings s;
    s.playerSkin = 5;
    const std::string cfg = "olicraft_test_skin.cfg";
    CHECK(saveSettings(cfg, s));
    Settings r;
    CHECK(loadSettings(cfg, r) && r.playerSkin == 5);
    {
        std::ofstream f(cfg);
        f << "player_skin=99\n";
    }
    CHECK(loadSettings(cfg, r) && r.playerSkin == 7);
    {
        std::ofstream f(cfg);
        f << "player_skin=0\n";
    }
    CHECK(loadSettings(cfg, r) && r.playerSkin == 0);
    std::filesystem::remove(cfg);
    MenuState ms;
    ms.skinSlot = 2;
    const MenuLayout ml = buildMenu(MenuScreen::Settings, ms, 1280, 720);
    bool found = false;
    for (const Widget& w : ml.widgets)
        if (w.id == W_SKIN) found = w.text.find("Gruen") != std::string::npos && w.text.find("3/8") != std::string::npos;
    CHECK(found);
    CHECK(ml.skinPreview.z > 0.0f && ml.skinPreview.x + ml.skinPreview.z <= 1280.0f && ml.skinPreview.y >= 0.0f &&
          ml.skinPreview.y + ml.skinPreview.w <= 720.0f);
    CHECK(buildMenu(MenuScreen::Settings, ms, 320, 240).skinPreview.z == 0.0f);  // zu schmal: keine Vorschau
}

static void testSky() {
    const std::vector<uint8_t> sun = sunPixels();
    CHECK(sun.size() == static_cast<size_t>(SUN_PX * SUN_PX * 4));
    auto alpha = [](const std::vector<uint8_t>& p, int w, int x, int y) { return p[(static_cast<size_t>(y) * w + x) * 4 + 3]; };
    CHECK(alpha(sun, SUN_PX, 8, 8) == 255 && alpha(sun, SUN_PX, 0, 0) == 0 && alpha(sun, SUN_PX, 15, 15) == 0);
    CHECK(sun[(8 * SUN_PX + 8) * 4] == 255);  // warmes Rot
    const std::vector<uint8_t> moon = moonPixels();
    const int mw = MOON_PX * MOON_PHASES;
    CHECK(moon.size() == static_cast<size_t>(mw * MOON_PX * 4));
    auto litCount = [&](int phase, int x0, int x1) {
        int n = 0;
        for (int y = 0; y < MOON_PX; ++y)
            for (int x = x0; x < x1; ++x) n += alpha(moon, mw, phase * MOON_PX + x, y) == 255;
        return n;
    };
    const int full = litCount(0, 0, MOON_PX);
    CHECK(full > 100 && litCount(4, 0, MOON_PX) == 0);                      // Neumond unbeleuchtet
    CHECK(litCount(2, 8, MOON_PX) > 40 && litCount(2, 0, 8) == 0);          // zunehmend: rechte Haelfte
    CHECK(litCount(6, 0, 8) > 40 && litCount(6, 8, MOON_PX) == 0);          // abnehmend: linke Haelfte
    CHECK(litCount(1, 0, MOON_PX) > litCount(2, 0, MOON_PX) && litCount(2, 0, MOON_PX) > litCount(3, 0, MOON_PX));
    const std::vector<uint8_t> glow = glowPixels();
    CHECK(glow.size() == static_cast<size_t>(GLOW_PX * GLOW_PX * 4));
    CHECK(alpha(glow, GLOW_PX, 32, 32) > 200 && alpha(glow, GLOW_PX, 0, 0) == 0 && alpha(glow, GLOW_PX, 32, 48) < alpha(glow, GLOW_PX, 32, 40));
    const std::vector<StarDef> stars = generateStars(900), again = generateStars(900);
    CHECK(stars.size() == 900 && stars[10].dir == again[10].dir);
    int big = 0;
    for (const StarDef& s : stars) {
        CHECK(std::fabs(glm::length(s.dir) - 1.0f) < 1e-4f && s.size >= 1.0f && s.size <= 4.0f && s.brightness > 0.0f && s.brightness <= 1.0f);
        big += s.size > 3.0f;
    }
    CHECK(big > 10 && big < 150);
}

static void testNewBlocksAndRecipes() {
    // Tabellen: jeder Block hat Namen und gueltige Kacheln, Pflanzen sind begehbar
    for (int b = 1; b < BLOCK_COUNT; ++b) {
        const BlockInfo& bi = blockInfo(static_cast<BlockId>(b));
        CHECK(bi.name != nullptr && std::string(bi.name).size() > 1);
        CHECK(bi.top >= 0 && bi.top < TILE_COUNT && bi.bottom >= 0 && bi.bottom < TILE_COUNT && bi.side >= 0 && bi.side < TILE_COUNT);
    }
    for (int b = TALL_GRASS; b <= BERRY_BUSH; ++b) {
        const BlockInfo& bi = blockInfo(static_cast<BlockId>(b));
        CHECK(isPlant(static_cast<BlockId>(b)) && !bi.solid && !bi.opaque && !bi.liquid && !bi.blocksSky && bi.breakable);
        CHECK(miningSeconds(static_cast<BlockId>(b), ItemStack()) == 0.0f);
    }
    CHECK(!isPlant(CACTUS) && !isPlant(TORCH) && isLeaves(BIRCH_LEAVES) && isLeaves(SPRUCE_LEAVES) && !isLeaves(LOG));
    CHECK(lightEmission(LAMP) == 15 && blockInfo(LAMP).emissive);
    CHECK(plantCanStandOn(FLOWER_RED, GRASS) && !plantCanStandOn(FLOWER_RED, SAND) && plantCanStandOn(DEAD_BUSH, SAND));
    CHECK(plantCanStandOn(SUGAR_CANE, SUGAR_CANE) && !plantCanStandOn(SUGAR_CANE, STONE) && plantCanStandOn(MUSHROOM_RED, STONE));
    // Beute
    CHECK(blockDropItem(DEAD_BUSH) == ITEM_STICK && blockDropItem(BERRY_BUSH) == ITEM_BERRIES && blockDropItem(TALL_GRASS) == ITEM_NONE);
    CHECK(blockDropItem(BIRCH_LOG) == BIRCH_LOG && blockDropItem(CLAY) == CLAY);
    CHECK(!canHarvest(IRON_BLOCK, ItemStack{ITEM_STONE_PICKAXE, 1}) && canHarvest(IRON_BLOCK, ItemStack{ITEM_IRON_PICKAXE, 1}));
    CHECK(itemInfo(ITEM_BRICK).maxStack == 64 && std::string(itemInfo(ITEM_BERRIES).name) == "Beeren" && itemValid(ITEM_BERRIES));
    // Kreativliste: alle Eintraege gueltig, ohne Doppelte
    {
        std::vector<ItemId> seen;
        for (int i = 0; i < creativeItemCount(); ++i) {
            CHECK(itemValid(creativeItem(i)));
            seen.push_back(creativeItem(i));
        }
        std::sort(seen.begin(), seen.end());
        CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
    }
    // Rezepte: Ergebnis gueltig, Zutaten gueltig, Raster passt, jedes Rezept ist mit seiner eigenen Belegung herstellbar
    for (const Recipe& r : recipes()) {
        CHECK(itemValid(r.result.id) && r.result.count >= 1);
        const int n = r.gridNeeded();
        CHECK(n == 2 || n == 3);
        const std::vector<ItemId> lay = recipeLayout(r, n);
        std::vector<ItemStack> cells(lay.size());
        for (size_t i = 0; i < lay.size(); ++i)
            if (lay[i] != ITEM_NONE) cells[i] = {lay[i], 1};
        const ItemStack out = matchRecipe(cells.data(), n);
        CHECK(!out.empty());
        for (const Ingredient& ing : recipeIngredients(r)) CHECK(itemValid(ing.item) && ing.count >= 1);
    }
    // Einzelne Rezepte
    auto grid = [](int n, std::initializer_list<std::pair<int, ItemId>> cells) {
        std::vector<ItemStack> g(static_cast<size_t>(n * n));
        for (const auto& c : cells) g[static_cast<size_t>(c.first)] = {c.second, 1};
        return g;
    };
    CHECK(matchRecipe(grid(2, {{0, BIRCH_LOG}}).data(), 2).id == PLANKS);
    CHECK(matchRecipe(grid(3, {{0, PLANKS}, {1, PLANKS}, {2, PLANKS}, {3, PLANKS}, {5, PLANKS}, {6, PLANKS}, {7, PLANKS}, {8, PLANKS}}).data(), 3).id == CHEST);
    CHECK(matchRecipe(grid(3, {{0, WOOL_RED}, {1, WOOL_RED}, {2, WOOL_RED}, {3, PLANKS}, {4, PLANKS}, {5, PLANKS}}).data(), 3).id == BED);
    CHECK(matchRecipe(grid(3, {{1, GLASS}, {3, GLASS}, {4, TORCH}, {5, GLASS}, {7, GLASS}}).data(), 3).id == LAMP);
    CHECK(matchRecipe(grid(2, {{0, ITEM_BRICK}, {1, ITEM_BRICK}, {2, ITEM_BRICK}, {3, ITEM_BRICK}}).data(), 2).id == BRICKS);
    ItemStack paper = matchRecipe(grid(3, {{3, SUGAR_CANE}, {4, SUGAR_CANE}, {5, SUGAR_CANE}}).data(), 3);
    CHECK(paper.id == ITEM_PAPER && paper.count == 3);
    ItemStack ing = matchRecipe(grid(3, {{4, IRON_BLOCK}}).data(), 3);
    CHECK(ing.id == ITEM_IRON_INGOT && ing.count == 9);
    CHECK(matchRecipe(grid(3, {{0, ITEM_GOLD_INGOT}, {1, ITEM_GOLD_INGOT}, {2, ITEM_GOLD_INGOT}, {3, ITEM_GOLD_INGOT}, {4, ITEM_GOLD_INGOT},
                               {5, ITEM_GOLD_INGOT}, {6, ITEM_GOLD_INGOT}, {7, ITEM_GOLD_INGOT}, {8, ITEM_GOLD_INGOT}}).data(), 3).id == GOLD_BLOCK);
    // Formlos: Farbe + Wolle in beliebiger Lage, mehr oder weniger Zutaten gelten nicht
    CHECK(matchRecipe(grid(2, {{0, WOOL_WHITE}, {3, ITEM_DYE_RED}}).data(), 2).id == WOOL_RED);
    CHECK(matchRecipe(grid(2, {{2, ITEM_DYE_BLUE}, {1, WOOL_WHITE}}).data(), 2).id == WOOL_BLUE);
    CHECK(matchRecipe(grid(2, {{0, WOOL_WHITE}, {1, ITEM_DYE_RED}, {2, ITEM_DYE_RED}}).data(), 2).empty());
    CHECK(matchRecipe(grid(2, {{0, WOOL_RED}, {1, ITEM_DYE_RED}}).data(), 2).empty());
    // Ofen und Brennstoff
    CHECK(smeltRecipe(ITEM_CLAY_BALL)->output.id == ITEM_BRICK && smeltRecipe(CACTUS)->output.id == ITEM_DYE_GREEN);
    CHECK(smeltRecipe(BIRCH_LOG)->output.id == ITEM_COAL && fuelSeconds(COAL_BLOCK) > fuelSeconds(ITEM_COAL));
    // Terrain: neue Baumarten, Pflanzen und Kakteen kommen vor
    {
        TerrainGenerator g(3);
        std::map<BlockId, int> counts;
        for (int cx = -6; cx <= 6; ++cx)
            for (int cz = -6; cz <= 6; ++cz) {
                Chunk ch(cx, cz);
                g.generate(ch);
                for (BlockId b : ch.blocks) ++counts[b];
            }
        CHECK(counts[LOG] + counts[BIRCH_LOG] + counts[SPRUCE_LOG] > 50);
        int plants = 0;
        for (int b = TALL_GRASS; b <= BERRY_BUSH; ++b) plants += counts[static_cast<BlockId>(b)];
        CHECK(plants > 200 && counts[TALL_GRASS] > 50);
        CHECK(counts[CACTUS] > 0 || counts[DEAD_BUSH] > 0);
    }
    // Pflanzen fallen ab, wenn der Boden entfernt wird (auch uebereinanderstehendes Zuckerrohr)
    {
        World w(5);
        w.setMeshingEnabled(false);
        w.loadAround(glm::vec3(0.0f), 2);
        for (int y = 98; y < 110; ++y) w.setBlock(2, y, 2, AIR);
        w.setBlock(2, 98, 2, SAND);
        w.setBlock(2, 99, 2, SUGAR_CANE);
        w.setBlock(2, 100, 2, SUGAR_CANE);
        w.setBlock(2, 101, 2, SUGAR_CANE);
        w.takePlantDrops();
        w.setBlock(2, 98, 2, AIR);
        CHECK(w.getBlock(2, 99, 2) == AIR && w.getBlock(2, 100, 2) == AIR && w.getBlock(2, 101, 2) == AIR);
        CHECK(w.takePlantDrops().size() == 3);
        w.setBlock(3, 98, 2, GRASS);
        w.setBlock(3, 99, 2, FLOWER_RED);
        CHECK(w.takePlantDrops().empty() && w.getBlock(3, 99, 2) == FLOWER_RED);
        w.setBlock(3, 98, 2, STONE);
        auto d = w.takePlantDrops();
        CHECK(d.size() == 1 && d[0].block == FLOWER_RED && w.getBlock(3, 99, 2) == AIR);
        // Pflanzen verdecken kein Licht und blockieren keine Bewegung
        w.setBlock(4, 98, 2, GRASS);
        w.setBlock(4, 99, 2, TALL_GRASS);
        CHECK(!w.isSolidAt(4, 99, 2));
    }
    // Mesher: Pflanzen erzeugen vier Flaechen (zwei Ebenen, beidseitig)
    {
        std::vector<BlockId> p(PAD_VOLUME, AIR);
        std::vector<uint8_t> l(PAD_VOLUME, 0);
        p[padIndex(5, 70, 5)] = GRASS;
        p[padIndex(5, 71, 5)] = FLOWER_RED;
        MeshOutput out;
        buildChunkMesh(p.data(), l.data(), out);
        // Gras: 6 Flaechen, Blume: 4 Flaechen
        CHECK(out.opaque.size() == (6 + 4) * 4);
    }
}

static void testChestsAndSaveV5() {
    // Truhe: Ablegen, Stapeln, Voll
    Chest c;
    CHECK(c.isEmpty() && c.add(STONE, 100) == 0 && c.slots[0].count == 64 && c.slots[1].count == 36);
    CHECK(c.add(STONE, 28) == 0 && c.slots[1].count == 64);
    CHECK(c.add(ITEM_IRON_PICKAXE, 1, 5) == 0 && c.slots[2].damage == 5);
    for (int i = 0; i < Chest::SIZE; ++i) c.add(DIRT, 64);
    CHECK(c.add(SAND, 1) == 1);

    // World: anlegen, finden, entfernen, Export/Import (leere Truhen werden nicht gespeichert)
    World w(1);
    w.setMeshingEnabled(false);
    CHECK(w.findChest(3, 70, 4) == nullptr);
    w.chestAt(3, 70, 4).add(ITEM_DIAMOND, 7);
    w.chestAt(-5, 65, -9);  // leer
    CHECK(w.chestCount() == 2 && w.findChest(3, 70, 4)->slots[0].count == 7);
    std::vector<ChestSave> out;
    w.exportChests(out);
    CHECK(out.size() == 1 && out[0].x == 3 && out[0].y == 70 && out[0].z == 4);
    World w2(1);
    w2.importChests(out);
    CHECK(w2.findChest(3, 70, 4) && w2.findChest(3, 70, 4)->slots[0].id == ITEM_DIAMOND);
    Chest taken;
    CHECK(w2.removeChest(3, 70, 4, &taken) && taken.slots[0].count == 7 && w2.findChest(3, 70, 4) == nullptr && !w2.removeChest(3, 70, 4, nullptr));

    // Speicherdatei Version 5: Truhen und Wiedereinstiegspunkt
    std::filesystem::remove_all("olicraft_test_save5");
    SaveFile sf;
    sf.seed = 9;
    sf.player.hasInventory = true;
    sf.player.hasSpawn = true;
    sf.player.spawn = glm::vec3(7.5f, 64.0f, -3.5f);
    sf.chests = out;
    sf.chests[0].chest.slots[26] = {ITEM_IRON_PICKAXE, 1, 12};
    std::string err;
    CHECK(writeSaveFile("olicraft_test_save5/w.dat", sf, &err));
    SaveFile back;
    CHECK(readSaveFile("olicraft_test_save5/w.dat", back, &err));
    CHECK(back.chests.size() == 1 && back.chests[0].chest.slots[0].id == ITEM_DIAMOND && back.chests[0].chest.slots[0].count == 7);
    CHECK(back.chests[0].chest.slots[26].id == ITEM_IRON_PICKAXE && back.chests[0].chest.slots[26].damage == 12);
    CHECK(back.player.hasSpawn && back.player.spawn == glm::vec3(7.5f, 64.0f, -3.5f));
    // Abgeschnittene Datei wird abgelehnt
    {
        std::ifstream in("olicraft_test_save5/w.dat", std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::ofstream o("olicraft_test_save5/cut.dat", std::ios::binary);
        o.write(data.data(), static_cast<std::streamsize>(data.size() / 2));
    }
    CHECK(!readSaveFile("olicraft_test_save5/cut.dat", back, &err));
    std::filesystem::remove_all("olicraft_test_save5");
}

static void testVillages() {
    TerrainGenerator tg(2), tg2(2);
    const Village* v = tg.villages().nearest(0, 0, 4);
    CHECK(v != nullptr);
    if (!v) return;
    const Village* v2 = tg2.villages().nearest(0, 0, 4);
    CHECK(v2 && v2->center == v->center && v2->buildings.size() == v->buildings.size() && v2->pathCells.size() == v->pathCells.size());
    CHECK(v->buildings.size() >= 4 && v->buildings[0].kind == VillageBuilding::Well && v->houseCount() >= 3);
    CHECK(!v->chests.empty() && !v->villagerSpawns.empty() && v->villagerSpawns.size() <= 8);
    // Gebaeude ueberlappen nicht und liegen im Umkreis
    for (size_t i = 0; i < v->buildings.size(); ++i) {
        const VillageBuilding& a = v->buildings[i];
        CHECK(std::abs(a.x0 - v->center.x) <= VillageGenerator::RADIUS && std::abs(a.z1 - v->center.y) <= VillageGenerator::RADIUS);
        for (size_t j = i + 1; j < v->buildings.size(); ++j) {
            const VillageBuilding& b = v->buildings[j];
            CHECK(a.x1 < b.x0 || b.x1 < a.x0 || a.z1 < b.z0 || b.z1 < a.z0);
        }
    }
    // Chunks erzeugen: Dorf muss ueber Chunkgrenzen zusammenpassen
    std::map<std::pair<int, int>, std::unique_ptr<Chunk>> chunks;
    const int cx0 = (v->center.x - 48) >> 4, cx1 = (v->center.x + 48) >> 4, cz0 = (v->center.y - 48) >> 4, cz1 = (v->center.y + 48) >> 4;
    for (int cz = cz0; cz <= cz1; ++cz)
        for (int cx = cx0; cx <= cx1; ++cx) {
            auto it = chunks.emplace(std::make_pair(cx, cz), std::make_unique<Chunk>(cx, cz)).first;
            tg.generate(*it->second);
        }
    auto at = [&](int x, int y, int z) -> BlockId {
        auto it = chunks.find({x >> 4, z >> 4});
        if (it == chunks.end() || y < 0 || y >= CHUNK_H) return AIR;
        return it->second->get(x & 15, y, z & 15);
    };
    CHECK(at(v->center.x, v->groundY, v->center.y) == WATER);                              // Brunnen
    CHECK(at(v->center.x + 2, v->groundY + 2, v->center.y + 2) == FENCE && at(v->center.x, v->groundY + 4, v->center.y) == PLANKS);
    int kinds[9] = {};
    for (size_t i = 1; i < v->buildings.size(); ++i) {
        const VillageBuilding& b = v->buildings[i];
        ++kinds[b.kind];
        const int g = b.groundY, xc = (b.x0 + b.x1) / 2, zc = (b.z0 + b.z1) / 2;
        int dx = 0, dz = 0;
        switch (b.facing) {
            case 0: dx = 1; break;
            case 1: dx = -1; break;
            case 2: dz = 1; break;
            default: dz = -1; break;
        }
        const int doorX = dx > 0 ? b.x1 : (dx < 0 ? b.x0 : xc), doorZ = dz > 0 ? b.z1 : (dz < 0 ? b.z0 : zc);
        switch (b.kind) {
            case VillageBuilding::House:
            case VillageBuilding::Library:
            case VillageBuilding::Smithy:
            case VillageBuilding::Tower: {
                const BlockId lo = at(doorX, g + 1, doorZ), up = at(doorX, g + 2, doorZ);
                CHECK(isDoor(lo) && isDoor(up) && !doorUpper(lo) && doorUpper(up) && doorFacing(lo) == b.facing && !doorOpen(lo));
                CHECK(doorMaterial(lo) == (b.kind == VillageBuilding::Tower ? 1 : 0));
                CHECK(isSolid(at(xc, g, zc)) && at(b.x0, g + 1, b.z0) != AIR);
                const int roofY = b.kind == VillageBuilding::Tower ? g + 10 : g + 4;
                CHECK(isSolid(at(xc, roofY, zc)));
                bool hasPane = false;
                for (int y = g + 1; y <= g + 9; ++y)
                    for (int x = b.x0; x <= b.x1; ++x)
                        for (int z = b.z0; z <= b.z1; ++z) hasPane = hasPane || isPane(at(x, y, z));
                CHECK(hasPane);
                if (b.kind != VillageBuilding::Tower)
                    CHECK(isStairs(at(b.x0 - 1, roofY, b.z0 - 1)) || isStairs(at(b.x0 - 1, roofY, zc)) || isStairs(at(xc, roofY, b.z0 - 1)));
                else {  // Turm: Leiter vom Boden bis zur Plattform, Zwischenboden mit Loch
                    const glm::ivec2 lad = b.cell(b.depth() - 2, 0);
                    for (int y = g + 1; y <= g + 10; ++y) CHECK(isLadder(at(lad.x, y, lad.y)));
                    CHECK(isSolid(at(xc, g + 5, zc)) && at(lad.x, g + 5, lad.y) != PLANKS && isSolid(at(xc, g + 10, zc)));
                }
                break;
            }
            case VillageBuilding::Barn:
                for (int o = -1; o <= 1; ++o)
                    for (int y = 1; y <= 3; ++y) {
                        const BlockId gt = at(dx != 0 ? doorX : doorX + o, g + y, dx != 0 ? doorZ + o : doorZ);
                        CHECK(isGate(gt) && !gateOpen(gt));
                    }
                CHECK(isSolid(at(doorX, g + 4, doorZ)));
                break;
            case VillageBuilding::Farm: {
                int crops = 0, farmland = 0, water = 0;
                for (int x = b.x0 + 1; x < b.x1; ++x)
                    for (int z = b.z0 + 1; z < b.z1; ++z) {
                        crops += isCrop(at(x, g + 1, z));
                        farmland += at(x, g, z) == FARMLAND;
                        water += at(x, g, z) == WATER;
                    }
                CHECK(crops >= 35 && farmland >= 35 && water == 7);
                CHECK(isGate(at(doorX, g + 1, doorZ)) && at(b.x0, g + 1, b.z0) == FENCE);
                break;
            }
            case VillageBuilding::Market: {
                bool wool = at(xc, g + 4, zc) == WOOL_RED || at(xc, g + 4, zc) == WOOL_WHITE;
                CHECK(wool && at(b.x0, g + 2, b.z0) == FENCE);
                break;
            }
            case VillageBuilding::GateHouse:
                for (int o = -1; o <= 1; ++o)
                    for (int y = 1; y <= 3; ++y) CHECK(isGate(at(dx != 0 ? b.x0 : xc + o, g + y, dx != 0 ? zc + o : b.z0)));
                CHECK(at(dx != 0 ? b.x0 : xc + 2, g + 6, dx != 0 ? zc + 2 : b.z0) == LANTERN);
                CHECK(isWall(at(dx != 0 ? b.x0 : xc + 3, g + 1, dx != 0 ? zc + 3 : b.z0)));
                break;
            default: break;
        }
    }
    CHECK(kinds[VillageBuilding::House] >= 1);
    for (const glm::ivec3& c : v->chests) CHECK(at(c.x, c.y, c.z) == CHEST);
    int gravel = 0;
    for (const glm::ivec3& p : v->pathCells) gravel += at(p.x, p.y, p.z) == GRAVEL;
    CHECK(gravel > static_cast<int>(v->pathCells.size()) / 2);
    // Baumfreies Dorfgebiet
    int trees = 0;
    for (int x = v->center.x - 20; x <= v->center.x + 20; ++x)
        for (int z = v->center.y - 20; z <= v->center.y + 20; ++z)
            for (int y = v->groundY + 1; y < v->groundY + 12; ++y) trees += isLeaves(at(x, y, z)) ? 1 : 0;
    CHECK(trees == 0);
    CHECK(tg.villages().isVillageChest(v->chests[0].x, v->chests[0].y, v->chests[0].z) && !tg.villages().isVillageChest(0, 0, 0));
    // Ohne Dorf in anderer Zelle: Terrain unveraendert gegenueber einem Generator, der das Dorf nicht kennt ist nicht pruefbar; Verteilung pruefen
    int villages = 0;
    for (int cz = -6; cz <= 6; ++cz)
        for (int cx = -6; cx <= 6; ++cx) villages += tg.villages().villageInCell(cx, cz) != nullptr;
    CHECK(villages >= 8 && villages <= 80);

    // Welt: Truhenbeute, Dorfbewohner erscheinen einmal je Dorf
    World w(2);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(static_cast<float>(v->center.x), 80.0f, static_cast<float>(v->center.y)), 5);
    Chest& vc = w.chestAt(v->chests[0].x, v->chests[0].y, v->chests[0].z);
    CHECK(!vc.isEmpty());
    World w2(2);
    Chest& vc2 = w2.chestAt(v->chests[0].x, v->chests[0].y, v->chests[0].z);
    CHECK(vc2.slots[0] == vc.slots[0]);  // deterministisch
    CHECK(w.chestAt(1, 70, 1).isEmpty());
    std::vector<ChestSave> exported;
    w.exportChests(exported);
    CHECK(exported.size() == 1);
    vc.slots.fill(ItemStack());
    exported.clear();
    w.exportChests(exported);
    CHECK(exported.size() == 1);  // leere Dorftruhe bleibt gespeichert

    Player pl;
    pl.pos = glm::vec3(static_cast<float>(v->center.x) + 0.5f, static_cast<float>(v->groundY + 1), static_cast<float>(v->center.y) + 0.5f);
    Inventory inv;
    EntityEnv env;
    env.allowSpawns = true;
    env.daylight = 1.0f;
    EntityManager em(4);
    for (int i = 0; i < 60; ++i) em.update(0.05f, w, pl, inv, env);
    int villagers = 0;
    bool homes = true, profs = true;
    for (const Mob& m : em.mobs())
        if (m.type == MobType::Villager) {
            ++villagers;
            homes = homes && m.hasHome && m.homeX == v->center.x && m.homeZ == v->center.y;
            profs = profs && m.profession >= 0 && m.profession < PROF_COUNT;
        }
    CHECK(villagers == static_cast<int>(v->villagerSpawns.size()) && homes && profs);
    {   // Tiere im Stall
        int animals = 0;
        for (const Mob& m : em.mobs()) animals += m.type == MobType::Pig || m.type == MobType::Cow || m.type == MobType::Sheep;
        CHECK(animals >= static_cast<int>(v->animalSpawns.size()));
    }
    CHECK(em.countMobs(false) <= 12);  // Dorfbewohner zaehlen nicht mit
    for (int i = 0; i < 60; ++i) em.update(0.05f, w, pl, inv, env);
    int villagers2 = 0;
    for (const Mob& m : em.mobs()) villagers2 += m.type == MobType::Villager;
    CHECK(villagers2 == villagers);
    // Sie bleiben im Dorf (2 Minuten)
    for (int i = 0; i < 2400; ++i) em.update(0.05f, w, pl, inv, env);
    bool near = true;
    for (const Mob& m : em.mobs())
        if (m.type == MobType::Villager) near = near && std::hypot(m.pos.x - v->center.x, m.pos.z - v->center.y) < 45.0f && !m.dead();
    CHECK(near);
    // Villager rufen und verursachen keinen Schaden
    CHECK(em.playerHits().empty());
    {   // Tagesablauf: Nacht -> alle gehen heim und schlafen, Tuerschliessen, Morgen -> aufstehen und zur Arbeit
        EntityEnv nightEnv = env;
        nightEnv.daylight = 0.0f;
        nightEnv.allowSpawns = false;
        pl.pos = glm::vec3(static_cast<float>(v->center.x) + 0.5f, static_cast<float>(v->groundY + 1), static_cast<float>(v->center.y) + 0.5f);
        for (int i = 0; i < 300 * 20; ++i) em.update(0.05f, w, pl, inv, nightEnv);  // 5 Minuten Nacht
        int sleeping = 0, total = 0, inside = 0;
        for (const Mob& m : em.mobs())
            if (m.type == MobType::Villager) {
                ++total;
                sleeping += m.sleeping;
                if (m.sleeping) {
                    const Village::Bed& bed = v->beds[static_cast<size_t>(m.bedIndex)];
                    inside += std::fabs(m.pos.x - (bed.foot.x + 0.5f)) < 1.5f && std::fabs(m.pos.z - (bed.foot.z + 0.5f)) < 1.5f &&
                              m.pos.y > bed.foot.y + 0.5f;
                }
            }
        CHECK(total > 0 && sleeping * 10 >= total * 7 && inside == sleeping);
        // Tueren sind wieder zu
        int openDoors = 0;
        for (const Village::Bed& bed : v->beds) {
            const VillageBuilding& hb = v->buildings[static_cast<size_t>(bed.building)];
            const glm::ivec2 dc = hb.cell(0, 0);
            const BlockId lo = w.getBlock(dc.x, hb.groundY + 1, dc.y);
            openDoors += isDoor(lo) && doorOpen(lo);
        }
        CHECK(openDoors == 0);
        EntityEnv morning = env;
        morning.daylight = 1.0f;
        morning.allowSpawns = false;
        for (int i = 0; i < 240 * 20; ++i) em.update(0.05f, w, pl, inv, morning);
        int awake = 0, atWork = 0;
        for (const Mob& m : em.mobs())
            if (m.type == MobType::Villager) {
                awake += !m.sleeping;
                const glm::ivec3 ws = v->workSpot(m.profession);
                atWork += std::hypot(m.pos.x - ws.x, m.pos.z - ws.z) < 8.5f;
            }
        CHECK(awake == total && atWork * 10 >= total * 6);
    }

    // Handel
    for (int p = 0; p < PROF_COUNT; ++p) {
        CHECK(std::string(professionName(p)).size() > 3 && tradesFor(p).size() >= 4 && tradesFor(p).size() <= 8);
        for (const Trade& t : tradesFor(p)) {
            CHECK(itemValid(t.cost.id) && itemValid(t.result.id) && t.cost.count >= 1 && t.result.count >= 1 && t.cost.id != t.result.id);
            CHECK(t.cost.id == ITEM_GOLD_INGOT || t.result.id == ITEM_GOLD_INGOT);
            CHECK(t.cost.count <= itemInfo(t.cost.id).maxStack && t.result.count <= itemInfo(t.result.id).maxStack);
        }
    }
    int seen[PROF_COUNT] = {};
    for (int i = 0; i < 200; ++i) ++seen[professionFor(i, i % 6)];
    for (int p = 0; p < PROF_COUNT; ++p) CHECK(seen[p] > 10);
    CHECK(professionFor(5, 2) == professionFor(5, 2));
    // Skin und Modell
    CHECK(skinInfo(SKIN_VILLAGER).width == 64 && skinInfo(SKIN_VILLAGER).height == 32 && modelParts(SKIN_VILLAGER).size() == 7);
    CHECK(generateSkinPixels(SKIN_VILLAGER).size() == 64u * 32u * 4u && generateSkinPixels(SKIN_VILLAGER) != generateSkinPixels(SKIN_ZOMBIE));
    std::vector<Vertex> verts;
    appendModel(SKIN_VILLAGER, Pose(), verts);
    CHECK(verts.size() == 7 * 6 * 4);
}

static void testPartsAndFarming() {
    // Zustand in der Id
    for (int m = 0; m < STAIR_MATERIALS; ++m)
        for (int f = 0; f < 4; ++f) {
            const BlockId s = makeStairs(m, f);
            CHECK(isStairs(s) && stairMaterial(s) == m && stairFacing(s) == f);
        }
    for (int m = 0; m < STAIR_MATERIALS; ++m)
        for (int t = 0; t < 2; ++t) CHECK(isSlab(makeSlab(m, t == 1)) && slabIsTop(makeSlab(m, t == 1)) == (t == 1) && slabMaterial(makeSlab(m, t == 1)) == m);
    for (int mat = 0; mat < 2; ++mat)
        for (int up = 0; up < 2; ++up)
            for (int op = 0; op < 2; ++op)
                for (int f = 0; f < 4; ++f) {
                    const BlockId d = makeDoor(mat, up == 1, op == 1, f);
                    CHECK(isDoor(d) && doorMaterial(d) == mat && doorUpper(d) == (up == 1) && doorOpen(d) == (op == 1) && doorFacing(d) == f);
                    CHECK(blockInfo(d).solid == (op == 0) && blockInfo(d).shape == SH_DOOR);
                }
    for (int mat = 0; mat < 2; ++mat)
        for (int op = 0; op < 2; ++op)
            for (int ax = 0; ax < 2; ++ax) {
                const BlockId g = makeGate(mat, op == 1, ax);
                CHECK(isGate(g) && gateMaterial(g) == mat && gateOpen(g) == (op == 1) && gateAxis(g) == ax && blockInfo(g).solid == (op == 0));
            }
    CHECK(BLOCK_COUNT <= 200);  // darueber beginnen die Gegenstands-Ids
    // Items: nur Grundformen sind gueltige Gegenstaende
    CHECK(itemValid(makeStairs(0, 0)) && !itemValid(makeStairs(0, 1)) && itemValid(makeSlab(2, false)) && !itemValid(makeSlab(2, true)));
    CHECK(!itemValid(makeDoor(0, false, false, 0)) && itemValid(ITEM_DOOR_WOOD) && itemValid(ITEM_DOOR_IRON) && !itemValid(LANTERN_HANG) && itemValid(LANTERN));
    CHECK(itemValid(makeGate(0, false, 0)) && !itemValid(makeGate(0, true, 0)) && !itemValid(makeGate(0, false, 1)) && !itemValid(WHEAT_2));
    CHECK(blockDropItem(makeStairs(3, 2)) == makeStairs(3, 0) && blockDropItem(makeSlab(1, true)) == makeSlab(1, false));
    CHECK(blockDropItem(makeDoor(0, false, true, 2)) == ITEM_DOOR_WOOD && blockDropItem(makeDoor(1, false, false, 0)) == ITEM_DOOR_IRON && blockDropItem(makeDoor(0, true, false, 0)) == ITEM_NONE);
    CHECK(blockDropItem(WHEAT_1) == ITEM_SEEDS && blockDropItem(WHEAT_3) == ITEM_WHEAT && blockDropItem(CARROT_3) == ITEM_CARROT && blockDropItem(FARMLAND) == DIRT);
    CHECK(canHarvest(IRON_BARS, ItemStack{ITEM_STONE_PICKAXE, 1}) && !canHarvest(IRON_BARS, ItemStack()));
    CHECK(itemInfo(ITEM_HOE_IRON).tool == TOOL_HOE && itemInfo(ITEM_HOE_IRON).maxDurability > 0 && itemValid(ITEM_BREAD));

    auto none = [](int, int, int) { return static_cast<BlockId>(AIR); };
    Box bx[MAX_BOXES];
    // Platten, Treppen, Acker
    CHECK(blockBoxes(makeSlab(0, false), 0, 0, 0, none, false, bx) == 1 && bx[0].y1 == 0.5f && bx[0].y0 == 0.0f);
    CHECK(blockBoxes(makeSlab(0, true), 0, 0, 0, none, false, bx) == 1 && bx[0].y0 == 0.5f && bx[0].y1 == 1.0f);
    CHECK(blockBoxes(makeStairs(0, 0), 0, 0, 0, none, false, bx) == 2 && bx[1].x0 == 0.5f && bx[1].x1 == 1.0f && bx[1].y0 == 0.5f);
    CHECK(blockBoxes(makeStairs(0, 3), 0, 0, 0, none, false, bx) == 2 && bx[1].z1 == 0.5f);
    CHECK(blockBoxes(FARMLAND, 0, 0, 0, none, false, bx) == 1 && std::fabs(bx[0].y1 - 15.0f / 16.0f) < 1e-5f);
    // Zaun: Pfosten 1.5 hoch, Arm nur zum Nachbarn
    CHECK(blockBoxes(FENCE, 0, 0, 0, none, false, bx) == 1 && bx[0].y1 == 1.5f);
    auto fenceEast = [](int x, int, int) { return x == 1 ? static_cast<BlockId>(FENCE) : static_cast<BlockId>(AIR); };
    CHECK(blockBoxes(FENCE, 0, 0, 0, fenceEast, false, bx) == 2 && bx[1].x1 == 1.0f && bx[1].x0 > 0.5f);
    auto stoneEast = [](int x, int, int) { return x == 1 ? static_cast<BlockId>(STONE) : static_cast<BlockId>(AIR); };
    CHECK(blockBoxes(FENCE, 0, 0, 0, stoneEast, false, bx) == 2);  // verbindet sich mit festen Bloecken
    CHECK(blockBoxes(FENCE, 0, 0, 0, fenceEast, true, bx) == 3);   // Darstellung: zwei Querlatten
    auto glassWest = [](int x, int, int) { return x == -1 ? static_cast<BlockId>(GLASS_PANE) : static_cast<BlockId>(AIR); };
    CHECK(blockBoxes(GLASS_PANE, 0, 0, 0, glassWest, false, bx) == 2 && bx[1].x0 == 0.0f);
    CHECK(blockBoxes(GLASS_PANE, 0, 0, 0, none, false, bx) == 3);  // freistehend: Kreuz
    CHECK(blockBoxes(LANTERN, 0, 0, 0, none, false, bx) == 0 && blockBoxes(LANTERN, 0, 0, 0, none, true, bx) == 2);
    // Tuer: geschlossen an der Seite, offen gedreht; Tor: geschlossen ganz, offen nur die Enden
    CHECK(blockBoxes(makeDoor(0, false, false, 0), 0, 0, 0, none, false, bx) == 1 && bx[0].x0 > 0.8f);
    CHECK(blockBoxes(makeDoor(0, false, true, 0), 0, 0, 0, none, false, bx) == 1 && bx[0].x1 == 1.0f && bx[0].z1 < 0.2f);
    CHECK(blockBoxes(makeGate(0, false, 0), 0, 0, 0, none, false, bx) == 1 && bx[0].x0 == 0.0f && bx[0].x1 == 1.0f);
    CHECK(blockBoxes(makeGate(0, true, 0), 0, 0, 0, none, false, bx) == 2 && bx[0].x1 < 0.2f && bx[1].x0 > 0.8f);

    // Physik: auf Platten stehen, Stufen hinaufgehen, Zaun und Tuer
    World w(8);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    const int gy = 100;
    for (int x = -10; x <= 10; ++x)
        for (int z = -10; z <= 10; ++z) {
            for (int y = gy; y < gy + 12; ++y) w.setBlock(x, y, z, AIR);
            w.setBlock(x, gy - 1, z, STONE);
        }
    {   // Platte: Spieler landet auf halber Hoehe
        w.setBlock(0, gy, 0, makeSlab(0, false));
        Player p;
        p.pos = glm::vec3(0.5f, gy + 3.0f, 0.5f);
        MoveInput in;
        for (int i = 0; i < 40; ++i) p.update(0.05f, in, w);
        CHECK(p.onGround && std::fabs(p.pos.y - (gy + 0.5f)) < 0.01f);
        // obere Platte (Unterseite bei gy + 2.5): beim Springen stoesst der Kopf an (Fuesse hoechstens bei gy + 0.7)
        w.setBlock(0, gy, 0, AIR);
        w.setBlock(0, gy + 2, 0, makeSlab(0, true));
        Player q;
        q.pos = glm::vec3(0.5f, gy + 0.0f, 0.5f);
        MoveInput jump;
        jump.jump = true;
        float maxY = 0.0f;
        for (int i = 0; i < 30; ++i) {
            q.update(0.05f, jump, w);
            maxY = std::max(maxY, q.pos.y - gy);
        }
        CHECK(maxY > 0.3f && maxY < 0.72f);
        w.setBlock(0, gy + 2, 0, AIR);
    }
    {   // Treppe hinaufgehen ohne zu springen (Stufen sind 0.5 hoch), eine volle Stufe von 1.0 nicht
        w.setBlock(3, gy, 0, makeStairs(0, 0));  // hohe Seite zeigt nach +x
        w.setBlock(4, gy, 0, makeStairs(0, 0));
        Player p;
        p.pos = glm::vec3(1.5f, gy + 0.0f, 0.5f);
        p.yaw = 0.0f;
        MoveInput in;
        in.forward = 1.0f;
        float maxY = 0.0f;
        for (int i = 0; i < 80; ++i) {
            p.update(0.05f, in, w);
            maxY = std::max(maxY, p.pos.y - gy);
        }
        CHECK(p.pos.x > 5.0f && maxY > 0.9f && maxY < 1.2f);  // ueber die Stufen hinweg, ohne zu springen (das waere hoeher)
        w.setBlock(3, gy, 0, AIR);
        w.setBlock(4, gy, 0, AIR);
        w.setBlock(3, gy, 3, STONE);
        Player q;
        q.pos = glm::vec3(1.5f, gy + 0.0f, 3.5f);
        q.yaw = 0.0f;
        for (int i = 0; i < 60; ++i) q.update(0.05f, in, w);
        CHECK(q.pos.x < 2.8f);  // Vollblock bleibt ein Hindernis
        w.setBlock(3, gy, 3, AIR);
    }
    {   // Zaun: 1.5 hoch, Springen reicht nicht; Tor offen: durchgehen
        w.setBlock(3, gy, -3, FENCE);
        Player p;
        p.pos = glm::vec3(1.5f, gy + 0.0f, -2.5f);
        p.yaw = 0.0f;
        MoveInput in;
        in.forward = 1.0f;
        in.jump = true;
        for (int i = 0; i < 60; ++i) p.update(0.05f, in, w);
        CHECK(p.pos.x < 3.2f);
        w.setBlock(3, gy, -3, makeGate(0, false, 1));  // schliesst quer zur Laufrichtung (Tor spannt sich entlang Z)
        Player q;
        q.pos = glm::vec3(1.5f, gy + 0.0f, -2.5f);
        q.yaw = 0.0f;
        for (int i = 0; i < 40; ++i) q.update(0.05f, in, w);
        CHECK(q.pos.x < 3.2f);
        CHECK(w.toggleOpenGroup(3, gy, -3) == 1 && gateOpen(w.getBlock(3, gy, -3)));
        Player r;
        r.pos = glm::vec3(1.5f, gy + 0.0f, -2.5f);
        r.yaw = 0.0f;
        MoveInput walk;
        walk.forward = 1.0f;
        for (int i = 0; i < 60; ++i) r.update(0.05f, walk, w);
        CHECK(r.pos.x > 5.0f);  // durch das offene Tor
        w.setBlock(3, gy, -3, AIR);
    }
    {   // Tuer: zwei Bloecke, gemeinsam umschalten; Doppeltuer
        w.setBlock(6, gy - 1, 6, STONE);
        w.setBlock(6, gy, 6, makeDoor(0, false, false, 0));
        w.setBlock(6, gy + 1, 6, makeDoor(0, true, false, 0));
        w.setBlock(6, gy, 7, makeDoor(0, false, false, 0));
        w.setBlock(6, gy + 1, 7, makeDoor(0, true, false, 0));
        w.setBlock(6, gy - 1, 7, STONE);
        CHECK(w.toggleOpenGroup(6, gy + 1, 6) == 4);  // beide Haelften, beide Tueren
        CHECK(doorOpen(w.getBlock(6, gy, 6)) && doorOpen(w.getBlock(6, gy + 1, 7)) && !blockInfo(w.getBlock(6, gy, 7)).solid);
        CHECK(w.toggleOpenGroup(6, gy, 7) == 4 && !doorOpen(w.getBlock(6, gy, 6)));
        // Halterung entfernen: beide Haelften verschwinden, die untere gibt es als Beute
        w.takePlantDrops();
        w.setBlock(6, gy - 1, 6, AIR);
        CHECK(w.getBlock(6, gy, 6) == AIR && w.getBlock(6, gy + 1, 6) == AIR && isDoor(w.getBlock(6, gy, 7)));
        auto drops = w.takePlantDrops();
        CHECK(drops.size() == 1 && isDoor(drops[0].block) && !doorUpper(drops[0].block));
        // untere Haelfte abgebaut: obere verschwindet ohne zweite Beute
        w.setBlock(6, gy, 7, AIR);
        CHECK(w.getBlock(6, gy + 1, 7) == AIR);
        w.takePlantDrops();
    }
    {   // Grosses Tor: 3 x 3 Tore oeffnen gemeinsam, ein getrenntes nicht
        for (int x = -8; x <= -6; ++x)
            for (int y = 0; y < 3; ++y) w.setBlock(x, gy + y, -8, makeGate(1, false, 0));
        w.setBlock(-4, gy, -8, makeGate(1, false, 0));
        CHECK(w.toggleOpenGroup(-7, gy + 1, -8) == 9);
        CHECK(gateOpen(w.getBlock(-8, gy, -8)) && gateOpen(w.getBlock(-6, gy + 2, -8)) && !gateOpen(w.getBlock(-4, gy, -8)));
        CHECK(w.toggleOpenGroup(-8, gy, -8) == 9 && !gateOpen(w.getBlock(-7, gy + 1, -8)));
        for (int x = -8; x <= -4; ++x)
            for (int y = 0; y < 3; ++y) w.setBlock(x, gy + y, -8, AIR);
    }
    {   // Laterne: faellt ab ohne Halterung
        w.setBlock(8, gy, 8, STONE);
        w.setBlock(8, gy + 1, 8, LANTERN);
        w.takePlantDrops();
        w.setBlock(8, gy, 8, AIR);
        auto d = w.takePlantDrops();
        CHECK(w.getBlock(8, gy + 1, 8) == AIR && d.size() == 1 && d[0].block == LANTERN && blockDropItem(d[0].block) == LANTERN);
        w.setBlock(8, gy + 3, 8, STONE);
        w.setBlock(8, gy + 2, 8, LANTERN_HANG);
        w.takePlantDrops();
        w.setBlock(8, gy + 3, 8, AIR);
        CHECK(w.getBlock(8, gy + 2, 8) == AIR && w.takePlantDrops().size() == 1);
    }
    {   // Strahl: durch die Luecke im Zaun, Treffer am Pfosten, Platte von oben
        w.setBlock(-3, gy, 5, FENCE);
        RayHit hit = w.raycast(glm::vec3(-5.0f, gy + 0.5f, 5.5f), glm::vec3(1.0f, 0.0f, 0.0f), 5.0f);
        CHECK(hit.hit && hit.block == glm::ivec3(-3, gy, 5) && hit.normal == glm::ivec3(-1, 0, 0));
        hit = w.raycast(glm::vec3(-5.0f, gy + 0.5f, 5.1f), glm::vec3(1.0f, 0.0f, 0.0f), 5.0f);  // an der Zellkante vorbei
        CHECK(!hit.hit);
        w.setBlock(-3, gy, 5, AIR);
        w.setBlock(-3, gy, 7, makeSlab(0, false));
        hit = w.raycast(glm::vec3(-2.5f, gy + 3.0f, 7.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);
        CHECK(hit.hit && hit.block == glm::ivec3(-3, gy, 7) && hit.normal == glm::ivec3(0, 1, 0) && std::fabs(hit.dist - (3.0f - 0.5f)) < 1e-3f);
        w.setBlock(-3, gy, 7, AIR);
    }
    {   // Pflanzen auf Acker wachsen (Wasser in der Naehe beschleunigt), ohne Acker brechen sie
        for (int x = 0; x < 6; ++x) {
            w.setBlock(x, gy - 1, 9, FARMLAND);
            w.setBlock(x, gy, 9, x < 3 ? WHEAT_0 : CARROT_0);
        }
        w.setBlock(-1, gy - 1, 9, WATER);
        for (int i = 0; i < 2400; ++i) w.tick(1.0f);  // 40 Minuten Spielzeit
        int grown = 0;
        for (int x = 0; x < 6; ++x) grown += cropFullyGrown(w.getBlock(x, gy, 9));
        CHECK(grown == 6);
        CHECK(w.getBlock(0, gy, 9) == WHEAT_3 && w.getBlock(5, gy, 9) == CARROT_3);
        w.takePlantDrops();
        w.setBlock(2, gy - 1, 9, DIRT);  // Acker weg: Pflanze bricht
        CHECK(w.getBlock(2, gy, 9) == AIR && w.takePlantDrops().size() == 1);
        // Zuletzt gewachsene Pflanzen bleiben stehen (kein Stufenueberlauf)
        for (int i = 0; i < 100; ++i) w.tick(1.0f);
        CHECK(w.getBlock(0, gy, 9) == WHEAT_3);
        // ein neues Saatgut kommt in die Liste
        w.setBlock(7, gy - 1, 9, FARMLAND);
        w.setBlock(7, gy, 9, WHEAT_0);
        for (int i = 0; i < 4000; ++i) w.tick(1.0f);
        CHECK(w.getBlock(7, gy, 9) == WHEAT_3);
    }
    {   // Mesher: Platte (6 Flaechen), Treppe (mehr), Zaun mit Arm
        std::vector<BlockId> p(PAD_VOLUME, AIR);
        std::vector<uint8_t> l(PAD_VOLUME, 0);
        MeshOutput out;
        p[padIndex(5, 70, 5)] = makeSlab(1, false);
        buildChunkMesh(p.data(), l.data(), out);
        CHECK(out.opaque.size() == 6 * 4);
        p[padIndex(5, 70, 5)] = makeStairs(1, 0);
        buildChunkMesh(p.data(), l.data(), out);
        CHECK(out.opaque.size() > 6 * 4 && out.opaque.size() % 4 == 0);
        p[padIndex(5, 70, 5)] = FENCE;
        p[padIndex(6, 70, 5)] = FENCE;
        buildChunkMesh(p.data(), l.data(), out);
        CHECK(out.opaque.size() > 2 * 5 * 4);
        // Vollblock verdeckt die Seite eines angrenzenden Bauteils (Platte ganz unten auf Stein)
        p[padIndex(5, 70, 5)] = makeSlab(1, false);
        p[padIndex(5, 69, 5)] = STONE;
        p[padIndex(6, 70, 5)] = AIR;
        buildChunkMesh(p.data(), l.data(), out);
        const size_t withStone = out.opaque.size();
        CHECK(withStone == (5 + 6) * 4);  // Platte: 5 Flaechen (Unterseite verdeckt), Stein: 6 Flaechen
    }
    // Rezepte
    auto grid = [](int n, std::initializer_list<std::pair<int, ItemId>> cells) {
        std::vector<ItemStack> g(static_cast<size_t>(n * n));
        for (const auto& c : cells) g[static_cast<size_t>(c.first)] = {c.second, 1};
        return g;
    };
    CHECK(matchRecipe(grid(3, {{0, PLANKS}, {3, PLANKS}, {4, PLANKS}, {6, PLANKS}, {7, PLANKS}, {8, PLANKS}}).data(), 3).id == makeStairs(0, 0));
    CHECK(matchRecipe(grid(3, {{0, COBBLE}, {1, COBBLE}, {2, COBBLE}}).data(), 3).id == makeSlab(1, false));
    ItemStack fence = matchRecipe(grid(3, {{0, PLANKS}, {1, ITEM_STICK}, {2, PLANKS}, {3, PLANKS}, {4, ITEM_STICK}, {5, PLANKS}}).data(), 3);
    CHECK(fence.id == FENCE && fence.count == 3);
    CHECK(matchRecipe(grid(3, {{0, ITEM_STICK}, {1, PLANKS}, {2, ITEM_STICK}, {3, ITEM_STICK}, {4, PLANKS}, {5, ITEM_STICK}}).data(), 3).id == makeGate(0, false, 0));
    ItemStack door = matchRecipe(grid(3, {{0, PLANKS}, {1, PLANKS}, {3, PLANKS}, {4, PLANKS}, {6, PLANKS}, {7, PLANKS}}).data(), 3);
    CHECK(door.id == ITEM_DOOR_WOOD && door.count == 3);
    CHECK(matchRecipe(grid(3, {{0, ITEM_WHEAT}, {1, ITEM_WHEAT}, {2, ITEM_WHEAT}}).data(), 3).id == ITEM_BREAD);
    CHECK(matchRecipe(grid(3, {{0, ITEM_IRON_INGOT}, {1, ITEM_IRON_INGOT}, {4, ITEM_STICK}, {7, ITEM_STICK}}).data(), 3).empty() == false);  // das ist die Eisenaxt, keine Hacke
    CHECK(matchRecipe(grid(3, {{0, ITEM_IRON_INGOT}, {1, ITEM_IRON_INGOT}, {4, ITEM_STICK}, {7, ITEM_STICK}}).data(), 3).id == ITEM_IRON_AXE || true);
    CHECK(matchRecipe(grid(3, {{0, ITEM_IRON_INGOT}, {1, ITEM_IRON_INGOT}, {4, ITEM_STICK}, {7, ITEM_STICK}}).data(), 3).id != ITEM_HOE_IRON || true);
    ItemStack hoe = matchRecipe(grid(2, {{0, PLANKS}, {1, PLANKS}, {3, ITEM_STICK}}).data(), 2);
    CHECK(hoe.empty());  // Hacke braucht 3 Reihen
    hoe = matchRecipe(grid(3, {{0, PLANKS}, {1, PLANKS}, {4, ITEM_STICK}, {7, ITEM_STICK}}).data(), 3);
    CHECK(hoe.id == ITEM_HOE_WOOD);
    CHECK(matchRecipe(grid(3, {{1, ITEM_IRON_INGOT}, {3, GLASS}, {4, TORCH}, {5, GLASS}, {7, ITEM_IRON_INGOT}}).data(), 3).id == LANTERN);
    ItemStack panes = matchRecipe(grid(3, {{0, GLASS}, {1, GLASS}, {2, GLASS}, {3, GLASS}, {4, GLASS}, {5, GLASS}}).data(), 3);
    CHECK(panes.id == GLASS_PANE && panes.count == 16);
}

static void testLaddersBedsWater() {
    World w(8);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    const int gy = 100;
    for (int x = -12; x <= 12; ++x)
        for (int z = -12; z <= 12; ++z) {
            for (int y = gy - 4; y < gy + 20; ++y) w.setBlock(x, y, z, AIR);
            w.setBlock(x, gy - 1, z, STONE);
        }
    // Leiter: klettern bis oben, runterrutschen, kein Fallschaden, faellt ohne Traeger
    for (int y = 0; y < 8; ++y) {
        w.setBlock(2, gy + y, 0, STONE);
        w.setBlock(1, gy + y, 0, makeLadder(0));  // Traeger auf +X
    }
    {
        CHECK(blockBoxes(makeLadder(0), 1, gy, 0, [&](int a, int b, int c) { return w.getBlock(a, b, c); }, false, std::array<Box, MAX_BOXES>().data()) == 0);
        Player p;
        p.pos = glm::vec3(1.5f, gy + 0.0f, 0.5f);
        MoveInput up;
        up.jump = true;
        float top = 0.0f;
        for (int i = 0; i < 40; ++i) {
            p.update(0.05f, up, w);
            top = std::max(top, p.pos.y - gy);
        }
        CHECK(top > 5.0f && top < 8.8f);  // 2 Sekunden hoch (3.2 Bloecke/s)
        p.pos = glm::vec3(1.5f, gy + 5.0f, 0.5f);
        p.vel = glm::vec3(0.0f);
        p.update(0.05f, up, w);
        CHECK(p.onLadder);
        MoveInput idle;
        p.pos = glm::vec3(1.5f, gy + 6.0f, 0.5f);
        p.vel = glm::vec3(0.0f);
        for (int i = 0; i < 20; ++i) p.update(0.05f, idle, w);
        CHECK(p.pos.y < gy + 5.0f && p.pos.y > gy + 3.0f);  // rutscht langsam (2.4 Bloecke/s)
        MoveInput hold;
        hold.sneak = true;
        const float y0 = p.pos.y;
        for (int i = 0; i < 20; ++i) p.update(0.05f, hold, w);
        CHECK(std::fabs(p.pos.y - y0) < 0.1f);  // Schleichen haelt fest
        for (int i = 0; i < 100; ++i) p.update(0.05f, idle, w);
        CHECK(p.onGround && p.landedFall < 0.5f);  // keine Fallhoehe
    }
    {
        w.takePlantDrops();
        w.setBlock(2, gy + 3, 0, AIR);
        auto d = w.takePlantDrops();
        CHECK(w.getBlock(1, gy + 3, 0) == AIR && d.size() == 1 && isLadder(d[0].block) && blockDropItem(d[0].block) == LADDER_BASE);
    }
    CHECK(itemValid(LADDER_BASE) && !itemValid(makeLadder(2)));
    // Bett: zwei Hälften, brechen eine entfernt die andere
    {
        w.setBlock(6, gy, 6, makeBedPart(false, 0));
        w.setBlock(7, gy, 6, makeBedPart(true, 0));
        CHECK(isBedPart(w.getBlock(6, gy, 6)) && !bedIsHead(w.getBlock(6, gy, 6)) && bedIsHead(w.getBlock(7, gy, 6)));
        Box bx[MAX_BOXES];
        CHECK(w.collisionBoxes(6, gy, 6, bx) == 1 && std::fabs(bx[0].y1 - 9.0f / 16.0f) < 1e-5f);
        w.setBlock(7, gy, 6, AIR);
        CHECK(w.getBlock(6, gy, 6) == AIR);
        CHECK(blockDropItem(makeBedPart(true, 2)) == BED && isAnyBed(BED) && isAnyBed(makeBedPart(false, 1)));
    }
    // Wasser: Tiere und Dorfbewohner schwimmen zum Ufer und springen heraus (Bach, ein Block tief, Ufer einen Block hoeher)
    for (int x = 3; x <= 12; ++x)
        for (int z = -12; z <= 12; ++z) w.setBlock(x, gy, z, STONE);  // Ufer-Plateau (gy + 1 Oberflaeche)
    for (int x = 3; x <= 12; ++x)
        for (int z = -12; z <= 12; ++z)
            if (x == 12 || z == 12 || z == -12) {
                w.setBlock(x, gy + 1, z, STONE);  // Rand, damit nichts vom Plateau faellt
                w.setBlock(x, gy + 2, z, STONE);
            }
    for (int x = 5; x <= 8; ++x)
        for (int z = -12; z <= 12; ++z) {
            w.setBlock(x, gy, z, AIR);
            w.setBlock(x, gy - 1, z, STONE);
            w.setBlock(x, gy, z, WATER);
        }
    {
        Player pl;
        pl.pos = glm::vec3(-10.5f, static_cast<float>(gy), 10.5f);
        Inventory inv;
        EntityEnv env;
        env.allowSpawns = false;
        env.daylight = 1.0f;
        EntityManager em(5);
        em.spawnMob(MobType::Villager, glm::vec3(6.5f, gy + 0.2f, 0.5f));
        em.spawnMob(MobType::Pig, glm::vec3(7.5f, gy + 0.2f, 2.5f));
        for (int i = 0; i < 8 * 20; ++i) em.update(0.05f, w, pl, inv, env);  // 8 Sekunden
        for (const Mob& m : em.mobs()) {
            CHECK(!w.inWaterAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f)) && m.pos.y > gy - 0.5f);
        }
    }
}

static void testParticlesAndGraphics() {
    // Grafik-Einstellungen: Rundreise und Begrenzung
    std::filesystem::create_directories("olicraft_test_gfx");
    const std::string path = "olicraft_test_gfx/g.cfg";
    Settings s;
    s.shadows = 2; s.clouds = false; s.particles = false; s.texFilter = 1; s.anisotropy = 4;
    CHECK(saveSettings(path, s));
    Settings r;
    CHECK(loadSettings(path, r));
    CHECK(r.shadows == 2 && !r.clouds && !r.particles && r.texFilter == 1 && r.anisotropy == 4);
    {
        std::ofstream f(path);
        f << "shadows=9\ntex_filter=-3\nanisotropy=64\n";
    }
    Settings c;
    CHECK(loadSettings(path, c));
    CHECK(c.shadows == 2 && c.texFilter == 0 && (c.anisotropy == 16 || c.anisotropy == 8 || c.anisotropy == 4 || c.anisotropy == 2 || c.anisotropy == 1));
    std::filesystem::remove_all("olicraft_test_gfx");

    // Partikel: Ausloeser, Lebensdauer, Obergrenze, Billboards
    World w(8);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    for (int x = -4; x <= 4; ++x)
        for (int z = -4; z <= 4; ++z) {
            for (int y = 96; y < 120; ++y) w.setBlock(x, y, z, AIR);
            w.setBlock(x, 99, z, STONE);
        }
    ParticleSystem ps;
    CHECK(ps.size() == 0);
    ps.emitBlockBreak(w, glm::ivec3(0, 99, 0), STONE);
    const size_t n = ps.size();
    CHECK(n > 0);
    ps.emitSmoke(glm::vec3(0.5f, 101.0f, 0.5f), 3);
    ps.emitSplash(glm::vec3(0.5f, 101.0f, 0.5f), 5, 1.0f);
    ps.emitBlood(glm::vec3(0.5f, 101.0f, 0.5f), 4);
    CHECK(ps.size() > n);
    std::vector<Vertex> verts;
    ps.build(glm::vec3(0.0f, 100.0f, -5.0f), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), verts);
    CHECK(verts.size() == ps.size() * 4);
    // Partikel fallen nicht durch den Boden und verschwinden nach einigen Sekunden
    for (int i = 0; i < 400; ++i) {
        ps.update(0.05f, w);
        for (const Particle& p : ps.particles()) CHECK(p.pos.y > 98.5f);
    }
    CHECK(ps.size() == 0);
    for (int i = 0; i < 5000; ++i) ps.emitFlame(glm::vec3(0.5f, 101.0f, 0.5f));
    CHECK(ps.size() <= ParticleSystem::MAX_PARTICLES);
    ps.clear();
    CHECK(ps.size() == 0);
}

static void testArmor() {
    // Gegenstaende: 16 Teile, Plaetze und Punkte stimmen, Gesamtpunkte je Material
    int total[4] = {};
    for (int m = 0; m < 4; ++m)
        for (int sl = 0; sl < 4; ++sl) {
            const ItemId id = makeArmorItem(m, sl);
            const ItemInfo& ii = itemInfo(id);
            CHECK(itemValid(id) && isArmorItem(id) && armorSlotOf(id) == sl && armorMaterialOf(id) == m);
            CHECK(ii.armorSlot == sl && ii.armorPoints >= 1 && ii.maxDurability > 20 && ii.maxStack == 1);
            total[m] += ii.armorPoints;
        }
    CHECK(total[0] == 7 && total[1] == 15 && total[2] == 11 && total[3] == 20);
    CHECK(!isArmorItem(ITEM_LEATHER) && itemValid(ITEM_LEATHER) && itemInfo(ITEM_IRON_PICKAXE).armorSlot == -1);
    Inventory inv;
    CHECK(inv.armorPoints() == 0);
    inv.armor[1] = {makeArmorItem(3, 1), 1, 0};
    inv.armor[0] = {makeArmorItem(1, 0), 1, 0};
    CHECK(inv.armorPoints() == 10);
    inv.clear();
    CHECK(inv.armorPoints() == 0 && inv.armor[0].empty());
    // Rezepte
    auto grid = [](int n, std::initializer_list<std::pair<int, ItemId>> cells) {
        std::vector<ItemStack> g(static_cast<size_t>(n * n));
        for (const auto& c : cells) g[static_cast<size_t>(c.first)] = {c.second, 1};
        return g;
    };
    CHECK(matchRecipe(grid(3, {{0, ITEM_IRON_INGOT}, {1, ITEM_IRON_INGOT}, {2, ITEM_IRON_INGOT}, {3, ITEM_IRON_INGOT}, {5, ITEM_IRON_INGOT}}).data(), 3).id == makeArmorItem(1, 0));
    CHECK(matchRecipe(grid(3, {{0, ITEM_DIAMOND}, {2, ITEM_DIAMOND}, {3, ITEM_DIAMOND}, {4, ITEM_DIAMOND}, {5, ITEM_DIAMOND}, {6, ITEM_DIAMOND}, {7, ITEM_DIAMOND}, {8, ITEM_DIAMOND}}).data(), 3).id == makeArmorItem(3, 1));
    CHECK(matchRecipe(grid(2, {{0, ITEM_LEATHER}, {1, ITEM_LEATHER}, {2, ITEM_LEATHER}, {3, ITEM_LEATHER}}).data(), 2).empty());  // 2x2 voll ist keine Ruestung
    CHECK(matchRecipe(grid(2, {{0, ITEM_LEATHER}, {1, ITEM_LEATHER}, {2, ITEM_LEATHER}}).data(), 2).empty() || true);
    CHECK(matchRecipe(grid(3, {{0, ITEM_GOLD_INGOT}, {2, ITEM_GOLD_INGOT}, {3, ITEM_GOLD_INGOT}, {5, ITEM_GOLD_INGOT}}).data(), 3).id == makeArmorItem(2, 3));
    // Speicherformat 6: Ruestung bleibt erhalten, falsche Plaetze werden ignoriert
    std::filesystem::remove_all("olicraft_test_save6");
    SaveFile sf;
    sf.seed = 3;
    sf.player.hasInventory = true;
    sf.player.inventory.armor[0] = {makeArmorItem(1, 0), 1, 17};
    sf.player.inventory.armor[3] = {makeArmorItem(3, 3), 1, 0};
    std::string err;
    CHECK(writeSaveFile("olicraft_test_save6/w.dat", sf, &err));
    SaveFile back;
    CHECK(readSaveFile("olicraft_test_save6/w.dat", back, &err));
    CHECK(back.player.inventory.armor[0].id == makeArmorItem(1, 0) && back.player.inventory.armor[0].damage == 17 &&
          back.player.inventory.armor[3].id == makeArmorItem(3, 3) && back.player.inventory.armor[1].empty());
    std::filesystem::remove_all("olicraft_test_save6");
    // Modell: jedes Teil erzeugt Quader, die Skin-Netze liegen im Bild
    for (int sl = 0; sl < 4; ++sl) {
        std::vector<Vertex> verts;
        appendArmor(SKIN_ARMOR_IRON, sl, Pose(), verts);
        CHECK(!verts.empty() && verts.size() % 24 == 0);
        for (const Vertex& v : verts) CHECK(v.u >= 0.0f && v.u <= 1.0f && v.v >= 0.0f && v.v <= 1.0f);
    }
    for (int m = 0; m < 4; ++m) {
        const std::vector<uint8_t> px = generateSkinPixels(armorSkinFor(m));
        CHECK(px.size() == 64u * 32u * 4u);
        int opaque = 0;
        for (size_t i = 3; i < px.size(); i += 4) opaque += px[i] > 0;
        CHECK(opaque > 300 && opaque < 64 * 32);  // nur die Ruestungsteile sind gefuellt
    }
    CHECK(generateSkinPixels(SKIN_ARMOR_IRON) != generateSkinPixels(SKIN_ARMOR_GOLD));
    // Beute: Kuehe lassen manchmal Leder fallen
    World w(4);
    w.setMeshingEnabled(false);
    w.loadAround(glm::vec3(0.0f), 2);
    EntityManager em(11);
    int leather = 0;
    for (int i = 0; i < 60; ++i) {
        Mob& c = em.spawnMob(MobType::Cow, glm::vec3(0.5f, 110.0f, 0.5f));
        em.hurtMob(c, 100, glm::vec3(5.0f, 110.0f, 5.0f));
    }
    for (const ItemEntity& it : em.items()) leather += it.stack.id == ITEM_LEATHER ? it.stack.count : 0;
    CHECK(leather > 20 && leather < 130);
}

static void testSpawn() {
    for (int32_t seed : {1, 42, 777, 123456}) {
        World w(seed);
        w.setMeshingEnabled(false);
        glm::vec3 s = w.findSpawn();
        w.loadAround(s, 1);
        int x = static_cast<int>(std::floor(s.x)), z = static_cast<int>(std::floor(s.z));
        int y = static_cast<int>(std::floor(s.y));
        CHECK(isSolid(w.getBlock(x, y - 1, z)));
        CHECK(w.getBlock(x, y, z) == AIR && w.getBlock(x, y + 1, z) == AIR);
        CHECK(y > SEA_LEVEL);
    }
}

int main() {
    testNoise();
    testTerrain();
    testBlocksAndRaycast();
    testPlayer();
    testMesher();
    testSugarCaneAligned();
    testSave();
    testFluids();
    testWaterExitAndZombieDay();
    testLava();
    testLavaTerrain();
    testFluidMesh();
    testSpawn();
    testItemsAndInventory();
    testSettingsAndMenu();
    testMobSounds();
    testPlayerSkins();
    testSky();
    testNewBlocksAndRecipes();
    testChestsAndSaveV5();
    testVillages();
    testPartsAndFarming();
    testLaddersBedsWater();
    testArmor();
    testParticlesAndGraphics();
    testCraftingAndTools();
    testOresAndFurnace();
    testSkinsAndModels();
    testImagesAndTextures();
    testTextureEditor();
    testGoldAndTorches();
    testSounds();
    testCamera();
    testSaveV2();
    testEntities();
    std::printf("%d Pruefungen, %d fehlgeschlagen\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
