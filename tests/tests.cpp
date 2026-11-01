// Tests ohne OpenGL-Kontext: Welt, Terrain, Physik, Mesher, Speichern.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "Block.h"
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
    testFluidMesh();
    testSpawn();
    std::printf("%d Pruefungen, %d fehlgeschlagen\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
