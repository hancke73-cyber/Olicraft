#include "Game.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>

#include "Crafting.h"
#include "Models.h"
#include "Trades.h"
#include "Noise.h"

namespace oc {

// ---------------------------------------------------------------------------------------------------------------
// Szenen fuer Screenshots (Entwicklung)
// ---------------------------------------------------------------------------------------------------------------

void Game::applyDemo() {
    const glm::vec3 f(std::cos(glm::radians(player_.yaw)), 0.0f, std::sin(glm::radians(player_.yaw)));
    auto groundAt = [&](float x, float z) {
        int y = CHUNK_H - 2;
        while (y > 1 && !isSolid(world_->getBlock(static_cast<int>(std::floor(x)), y, static_cast<int>(std::floor(z))))) --y;
        return static_cast<float>(y + 1);
    };
    if (opt_.demo == "mobs") {
        const MobType kinds[4] = {MobType::Pig, MobType::Cow, MobType::Sheep, MobType::Zombie};
        for (int i = 0; i < 4; ++i) {
            glm::vec3 p = player_.pos + f * (4.0f + 0.4f * i) + glm::vec3(-f.z, 0.0f, f.x) * (-4.5f + 3.0f * i);
            p.y = groundAt(p.x, p.z);
            Mob& m = entities_.spawnMob(kinds[i], p);
            m.yaw = std::atan2(-f.z, -f.x) + 0.5f;
            m.stateTimer = 1000.0f;  // stehen lassen
        }
        entities_.mobs()[1].hurtTimer = 100.0f;  // eine Kuh rot aufleuchten lassen
        glm::vec3 ip = player_.pos + f * 2.5f;
        ip.y = groundAt(ip.x, ip.z);
        entities_.spawnItem({GRASS, 1}, ip + glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f), 99.0f);
        entities_.spawnItem({ITEM_MEAT, 1}, ip - glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f), 99.0f);
    } else if (opt_.demo == "torch") {
        const glm::vec3 side(-f.z, 0.0f, f.x);
        for (int i = -2; i <= 2; ++i) {
            glm::vec3 p = player_.pos + f * (5.0f + (i % 2 ? 1.5f : 0.0f)) + side * (3.0f * i);
            p.y = groundAt(p.x, p.z);
            world_->setBlock(static_cast<int>(std::floor(p.x)), static_cast<int>(p.y), static_cast<int>(std::floor(p.z)), TORCH);
        }
        // eine Wand mit Wandfackel
        glm::vec3 w = player_.pos + f * 9.0f;
        const int wx = static_cast<int>(std::floor(w.x)), wz = static_cast<int>(std::floor(w.z));
        const int wy = static_cast<int>(groundAt(w.x, w.z));
        for (int y = wy; y < wy + 4; ++y)
            for (int d = -3; d <= 3; ++d) world_->setBlock(wx + (std::fabs(f.x) > std::fabs(f.z) ? 0 : d), y, wz + (std::fabs(f.x) > std::fabs(f.z) ? d : 0), PLANKS);
        const bool alongX = std::fabs(f.x) > std::fabs(f.z);
        const int sx = alongX ? (f.x > 0 ? -1 : 1) : 0, sz = alongX ? 0 : (f.z > 0 ? -1 : 1);
        world_->setBlock(wx + sx, wy + 1, wz + sz, sx < 0 ? TORCH_PX : (sx > 0 ? TORCH_NX : (sz < 0 ? TORCH_PZ : TORCH_NZ)));
    } else if (opt_.demo == "parts") {
        // Flache Buehne mit allen neuen Bauteilen (Blick entlang +x)
        const int ox = static_cast<int>(std::floor(player_.pos.x)) + 4, oz = static_cast<int>(std::floor(player_.pos.z)) - 12;
        const int gy = static_cast<int>(groundAt(player_.pos.x, player_.pos.z)) - 1;
        for (int z = 0; z <= 26; ++z)
            for (int x = -2; x <= 16; ++x) {
                for (int y = gy + 1; y <= gy + 8; ++y) world_->setBlock(ox + x, y, oz + z, AIR);
                world_->setBlock(ox + x, gy, oz + z, GRASS);
            }
        auto put = [&](int x, int y, int z, BlockId b) { world_->setBlock(ox + x, gy + 1 + y, oz + z, b); };
        // Reihe 1: Treppen und Platten (z = 0..)
        for (int m = 0; m < STAIR_MATERIALS; ++m) {
            put(0, 0, m * 2, makeStairs(m, 0));
            put(1, 0, m * 2, makeStairs(m, 2));
            put(3, 0, m * 2, makeSlab(m, false));
            put(4, 0, m * 2, makeSlab(m, true));
            put(4, -1 + 0, m * 2, stairMaterialBlock(m));
        }
        // Zaun mit Tor, Mauer mit grossem Tor
        for (int x = 0; x <= 8; ++x) put(x, 0, 12, FENCE);
        put(4, 0, 12, makeGate(0, false, 0));
        put(5, 0, 12, makeGate(0, true, 0));
        for (int x = 0; x <= 12; ++x) {
            put(x, 0, 15, WALL_COBBLE);
            put(x, 1, 15, x % 4 == 0 ? WALL_BRICKS : AIR);
        }
        for (int x = 4; x <= 7; ++x)
            for (int y = 0; y < 3; ++y) put(x, y, 15, makeGate(0, false, 0));
        // Haus mit Tuer, Fenster, Laterne
        for (int z = 18; z <= 22; ++z)
            for (int x = 0; x <= 4; ++x)
                for (int y = 0; y < 3; ++y) {
                    const bool edge = x == 0 || x == 4 || z == 18 || z == 22;
                    if (edge) put(x, y, z, PLANKS);
                }
        put(2, 0, 18, makeDoor(0, false, false, 3));
        put(2, 1, 18, makeDoor(0, true, false, 3));
        put(4, 1, 20, GLASS_PANE);
        put(0, 1, 20, GLASS_PANE);
        put(0, 1, 19, GLASS_PANE);
        put(0, 1, 21, GLASS_PANE);
        put(4, 0, 21, IRON_BARS);
        for (int y = 0; y < 4; ++y) {
            put(5, y, 23, STONE);
            put(4, y, 23, makeLadder(0));
        }
        put(10, 0, 22, makeBedPart(false, 2));
        put(10, 0, 23, makeBedPart(true, 2));
        put(4, 1, 21, IRON_BARS);
        put(2, 0, 16, LANTERN);
        put(6, 3, 20, STONE);
        put(6, 2, 20, LANTERN_HANG);
        // zwei offene und geschlossene Tueren nebeneinander
        put(8, 0, 18, makeDoor(1, false, true, 0));
        put(8, 1, 18, makeDoor(1, true, true, 0));
        put(10, 0, 18, makeDoor(0, false, false, 0));
        put(10, 1, 18, makeDoor(0, true, false, 0));
        // Acker mit Wasser und Feldfruechten
        for (int z = 24; z <= 26; ++z)
            for (int x = 0; x <= 8; ++x) {
                world_->setBlock(ox + x, gy, oz + z, z == 25 && x % 4 == 0 ? WATER : FARMLAND);
                if (z != 25) put(x, 0, z, x < 4 ? static_cast<BlockId>(WHEAT_0 + (x % 4)) : static_cast<BlockId>(CARROT_0 + (x % 4)));
            }
        // Walmdach aus Treppen: die Ecken ergeben sich aus den Nachbarn
        for (int k = 0; k < 3; ++k) {
            const int lo = 14 + k, hi = 20 - k, zlo = k, zhi = 6 - k;
            for (int x = lo; x <= hi; ++x)
                for (int z = zlo; z <= zhi; ++z) {
                    if (x != lo && x != hi && z != zlo && z != zhi) continue;
                    put(x, k, z, makeStairs(3, x == lo ? 0 : (x == hi ? 1 : (z == zlo ? 2 : 3))));
                }
        }
        put(17, 3, 3, BRICKS);
        // glatter Giebel aus Dachkeilen (zwei Reihen mit First in der Mitte) und eine Rampe
        for (int x = 30; x <= 36; ++x) {
            put(x, 0, 0, makeWedge(3, 3));  // hohe Seiten zeigen nach aussen, in der Mitte laeuft der First
            put(x, 0, 1, makeWedge(3, 2));
        }
        for (int k = 0; k < 3; ++k) put(30 + k, 0, 4, makeWedge(1, 0));
        for (int z = -2; z <= 8; ++z)  // zusaetzliche Flaeche fuer die hinteren Teile der Buehne
            for (int x = 17; x <= 60; ++x) {
                for (int y = gy + 1; y <= gy + 8; ++y) world_->setBlock(ox + x, y, oz + z, AIR);
                world_->setBlock(ox + x, gy, oz + z, GRASS);
            }
        // Schilder: Stehschild mit Text und ein Wandschild
        put(40, 0, 0, makeSign(false, 3));
        put(43, 0, 0, STONE);
        put(43, 1, 0, STONE);
        put(42, 1, 0, makeSign(true, 1));
        {
            SignText t;
            t.lines = {"Willkommen", "im Dorf", "Haus 3 links", ""};
            world_->setSign(ox + 40, gy + 1, oz + 0, t);
            SignText t2;
            t2.lines = {"Schmiede", "", "", ""};
            world_->setSign(ox + 42, gy + 2, oz + 0, t2);
        }
        // Steampunk: Kessel, Rohre, Motor, Welle, Bohrer, Band, Lift, Destille
        put(50, 0, 0, BOILER);
        put(51, 0, 0, BOILER_LIT);
        put(50, 1, 0, PIPE);
        put(50, 2, 0, PIPE);
        put(51, 1, 0, PIPE);
        put(52, 1, 0, PIPE);
        put(53, 1, 0, MOTOR);
        put(54, 1, 0, SHAFT);
        put(55, 1, 0, SHAFT);
        put(56, 1, 0, makeDrill(0));
        put(52, 0, 0, STILL);
        put(52, 2, 0, PIPE);
        for (int k = 0; k < 4; ++k) {
            put(50 + k, 0, 3, makeConveyor(k));
            put(50 + k, 0, 5, makeDrill(k));
        }
        put(55, 0, 3, makeDrill(4));
        put(56, 0, 3, makeDrill(5));
        put(55, 0, 5, LIFT);
        put(55, 1, 5, LIFT);
        put(55, 2, 5, LIFT);
        // Wasserleitung: Pumpe, Wasserrohre mit Knick, Auslass, Kessel am Ende
        put(58, 0, 0, PUMP);
        for (int k = 1; k <= 3; ++k) put(58 + k, 0, 0, WATER_PIPE);
        put(61, 1, 0, WATER_PIPE);
        put(61, 2, 0, OUTLET);
        put(62, 0, 0, BOILER);
        // Bogen mit umgedrehten Treppen unter der Decke
        for (int z = 0; z <= 2; ++z) {
            for (int y = 0; y <= 2; ++y) {
                put(24, y, z, STONE_BRICKS);
                put(28, y, z, STONE_BRICKS);
            }
            put(25, 2, z, makeStairs(2, 0, true));
            put(26, 2, z, makeSlab(2, true));
            put(27, 2, z, makeStairs(2, 1, true));
        }
        player_.pos = glm::vec3(static_cast<float>(ox + 5), static_cast<float>(gy + 11), static_cast<float>(oz - 3));
        player_.yaw = 90.0f;
        player_.pitch = -42.0f;
        player_.flying = true;
        creative_ = true;
        if (const char* cam = std::getenv("OLICRAFT_DEMO_CAM")) {  // Test: Kamera relativ zur Buehne "dx dy dz yaw pitch"
            float dx, dy, dz, yaw, pitch;
            if (std::sscanf(cam, "%f %f %f %f %f", &dx, &dy, &dz, &yaw, &pitch) == 5) {
                player_.pos = glm::vec3(ox + dx, gy + dy, oz + dz);
                player_.yaw = yaw;
                player_.pitch = pitch;
            }
        }
        if (const char* mach = std::getenv("OLICRAFT_MACHINE")) {  // Test: Maschinenfenster (b Kessel, d Destille)
            const bool still = mach[0] == 'd';
            const glm::ivec3 mp = still ? glm::ivec3(ox + 52, gy + 1, oz) : glm::ivec3(ox + 51, gy + 1, oz);
            if (Machine* m = world_->findMachine(mp.x, mp.y, mp.z)) {
                m->water = 1300.0f;
                m->slots[0] = {ITEM_COAL, 5, 0};
                m->slots[1] = {still ? ITEM_TAR : ITEM_BUCKET_WATER, still ? 3 : 1, 0};
                m->steam = 0.6f;
                m->load = 0.4f;
            }
            openMachine(mp);
        }
        if (std::getenv("OLICRAFT_SIGN")) openSign(glm::ivec3(ox + 40, gy + 1, oz));  // Test: Schildfenster
    } else if (opt_.demo == "water") {
        // Quelle auf einer Anhoehe, das Wasser laeuft ueber das Gelaende
        glm::vec3 p = player_.pos + f * 6.0f;
        int x = static_cast<int>(std::floor(p.x)), z = static_cast<int>(std::floor(p.z));
        int y = static_cast<int>(groundAt(p.x, p.z));
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz) world_->setBlock(x + dx, y, z + dz, STONE);
        world_->setBlock(x, y + 1, z, WATER);
        for (int i = 0; i < 40; ++i) world_->tick(0.25f);
    } else if (opt_.demo == "inv") {
        inventory_.clear();
        const ItemId items[] = {GRASS, DIRT, STONE, COBBLE, SAND, PLANKS, LOG, LEAVES, GLASS, SNOW, ITEM_MEAT, WATER};
        int i = 0;
        for (ItemId id : items) inventory_.add(id, 3 + i++ * 7);
        inventory_.slots[2].count = 64;
        if (creative_) {
            for (int k = 0; k < Inventory::HOTBAR; ++k) inventory_.slots[k] = {DEFAULT_HOTBAR[k], 64};
        }
        cursor_ = {STONE, 12};
    }
}

}  // namespace oc
