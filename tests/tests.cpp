// Tests ohne OpenGL-Kontext: Welt, Terrain, Physik, Mesher, Speichern.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Block.h"
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

static void testMesher() {
    std::vector<BlockId> pad(PAD_VOLUME, AIR);
    auto set = [&](int x, int y, int z, BlockId b) { pad[padIndex(x + 1, y, z + 1)] = b; };
    MeshOutput out;

    set(5, 50, 5, STONE);
    buildChunkMesh(pad.data(), out);
    CHECK(out.opaque.size() == 6 * 4);
    CHECK(out.water.empty());

    set(6, 50, 5, STONE);
    buildChunkMesh(pad.data(), out);
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
        buildChunkMesh(p2.data(), o2);
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
    buildChunkMesh(p3.data(), o3);
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
    buildChunkMesh(p4.data(), o4);
    CHECK(o4.opaque.size() == 10 * 4);

    // Himmelslicht: Block mit Dach darueber bekommt weniger Himmelslicht an der Oberseite als ein freier
    std::vector<BlockId> p5(PAD_VOLUME, AIR);
    p5[padIndex(2 + 1, 50, 2 + 1)] = STONE;       // frei
    p5[padIndex(10 + 1, 50, 10 + 1)] = STONE;     // mit Dach
    for (int x = 8; x <= 12; ++x)
        for (int z = 8; z <= 12; ++z) p5[padIndex(x + 1, 55, z + 1)] = STONE;
    MeshOutput o5;
    buildChunkMesh(p5.data(), o5);
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
    buildChunkMesh(pad.data(), out);
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
    buildChunkMesh(p2.data(), o2);
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
    std::vector<Vertex> a, b2;
    em.buildMesh(w, glm::vec3(0.0f, gy + 2.0f, -3.0f), 1.0f, a, b2);
    CHECK(!a.empty() && !b2.empty() && a.size() % 4 == 0 && b2.size() % 4 == 0);
    bool finite = true;
    for (const Vertex& v : a) finite = finite && std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    CHECK(finite);
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
    buildChunkMesh(pad.data(), out);
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
    CHECK(saveSettings(path, s));
    Settings r;
    CHECK(loadSettings(path, r));
    CHECK(r.renderDistance == 17 && std::fabs(r.mouseSensitivity - 0.15f) < 1e-4f && r.fov == 95.0f && !r.vsync);
    // Werte ausserhalb und Muell werden abgefangen
    {
        std::ofstream f(path);
        f << "render_distance=999\nfov=abc\nmouse_sensitivity=-4\nunbekannt=1\nkeingleichzeichen\n";
    }
    Settings c;
    CHECK(loadSettings(path, c));
    CHECK(c.renderDistance == Settings::MAX_DISTANCE && c.fov == 70.0f && c.mouseSensitivity == Settings::MIN_SENS);
    CHECK(!loadSettings("olicraft_test_settings/gibt_es_nicht.cfg", c));
    std::filesystem::remove_all("olicraft_test_settings");

    // Menue-Layouts: Widgets liegen im Fenster, ueberlappen nicht und sind per Mausposition treffbar
    for (MenuScreen scr : {MenuScreen::Pause, MenuScreen::Settings}) {
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
        CHECK(matchRecipe(g.data(), 2).empty());
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
    testSave();
    testFluids();
    testWaterExitAndZombieDay();
    testLava();
    testLavaTerrain();
    testFluidMesh();
    testSpawn();
    testItemsAndInventory();
    testSettingsAndMenu();
    testCraftingAndTools();
    testOresAndFurnace();
    testSaveV2();
    testEntities();
    std::printf("%d Pruefungen, %d fehlgeschlagen\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
