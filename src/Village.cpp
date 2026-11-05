#include "Village.h"

#include <algorithm>
#include <cmath>

#include "Chunk.h"
#include "Noise.h"
#include "TerrainGen.h"

namespace oc {

namespace {

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uniform() { return (next() & 0xFFFFFFu) / 16777216.0f; }
    int range(int a, int b) { return a + static_cast<int>(next() % static_cast<uint32_t>(b - a + 1)); }
};

int floorDivI(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

bool overlaps(const VillageBuilding& a, int x0, int z0, int x1, int z1, int margin) {
    return !(a.x1 + margin < x0 || x1 + margin < a.x0 || a.z1 + margin < z0 || z1 + margin < a.z0);
}

}  // namespace

glm::ivec3 Village::workSpot(int profession) const {
    // Bauer: Feld, Bibliothekar: Bibliothek, Schmied: Schmiede, Steinmetz: Marktstand, Schaefer: Scheune
    static const VillageBuilding::Kind want[5] = {VillageBuilding::Farm, VillageBuilding::Library, VillageBuilding::Smithy,
                                                  VillageBuilding::Market, VillageBuilding::Barn};
    const VillageBuilding::Kind k = want[profession < 0 || profession >= 5 ? 0 : profession];
    for (const VillageBuilding& b : buildings)
        if (b.kind == k) return glm::ivec3(b.door.x, b.groundY + 1, b.door.y);
    return glm::ivec3(center.x, groundY + 1, center.y + 3);
}

int Village::houseCount() const {
    int n = 0;
    for (const VillageBuilding& b : buildings)
        if (b.kind != VillageBuilding::Well) ++n;
    return n;
}

Village VillageGenerator::build(int cellX, int cellZ, bool& exists) const {
    exists = false;
    Village v;
    const uint32_t h0 = hash3(cellX, 0x7011, cellZ, static_cast<uint32_t>(seed_) ^ 0x51AB3u);
    if (h0 % 100u >= 60u) return v;
    Rng rng(h0 ^ 0x9E3779B1u);
    const int cx = cellX * CELL + 41 + rng.range(0, 78);
    const int cz = cellZ * CELL + 41 + rng.range(0, 78);

    auto height = [&](int x, int z) { return terrain_.column(x, z).height; };
    const TerrainGenerator::Column cc = terrain_.column(cx, cz);
    if (cc.height < SEA_LEVEL + 2 || cc.height > 86 || cc.snow) return v;
    // Umgebung des Brunnens muss ziemlich eben und trocken sein
    for (int dz = -6; dz <= 6; dz += 6)
        for (int dx = -6; dx <= 6; dx += 6) {
            const int h = height(cx + dx, cz + dz);
            if (h < SEA_LEVEL + 1 || std::abs(h - cc.height) > 3) return v;
        }

    v.id = (cellX * 73856093) ^ (cellZ * 19349663) ^ 0x5A5A;
    v.center = {cx, cz};
    v.groundY = cc.height;
    VillageBuilding well;
    well.kind = VillageBuilding::Well;
    well.x0 = cx - 2;
    well.x1 = cx + 2;
    well.z0 = cz - 2;
    well.z1 = cz + 2;
    well.groundY = cc.height;
    v.buildings.push_back(well);

    using K = VillageBuilding;
    static const K::Kind order[10] = {K::House, K::Library, K::Farm,  K::House, K::Smithy,
                                      K::Barn,  K::Market,  K::House, K::Tower, K::Farm};
    // Ausdehnung (Tiefe entlang der Tuerachse, Breite quer dazu)
    auto dims = [&](K::Kind k, int& depth, int& width) {
        switch (k) {
            case K::Library: depth = 7; width = 9; break;
            case K::Farm: depth = 9; width = 9; break;
            case K::Barn: depth = 11; width = 9; break;
            case K::Market: depth = 5; width = 9; break;
            case K::Tower: depth = 5; width = 5; break;
            case K::GateHouse: depth = 1; width = 9; break;
            case K::Church: depth = 11; width = 7; break;
            case K::House: depth = width = rng.range(0, 3) == 0 ? 5 : 7; break;
            default: depth = width = 7; break;
        }
    };
    auto tryPlace = [&](K::Kind kind, float ang, float dist) -> bool {
        const int px = cx + static_cast<int>(std::lround(std::cos(ang) * dist));
        const int pz = cz + static_cast<int>(std::lround(std::sin(ang) * dist));
        const int dx = px - cx, dz = pz - cz;
        const bool alongX = std::abs(dx) >= std::abs(dz);  // Tuer zeigt entlang dieser Achse zum Zentrum
        VillageBuilding b;
        b.kind = kind;
        int depth, width;
        dims(kind, depth, width);
        const int ex = alongX ? depth : width, ez = alongX ? width : depth;
        b.x0 = px - ex / 2;
        b.x1 = b.x0 + ex - 1;
        b.z0 = pz - ez / 2;
        b.z1 = b.z0 + ez - 1;
        b.facing = alongX ? (dx > 0 ? 1 : 0) : (dz > 0 ? 3 : 2);
        for (const VillageBuilding& o : v.buildings)
            if (overlaps(o, b.x0, b.z0, b.x1, b.z1, o.kind == VillageBuilding::Well ? 4 : 3)) return false;
        const int hc = height(px, pz);
        if (hc < SEA_LEVEL + 1 || std::abs(hc - v.groundY) > 5) return false;
        const int tol = kind == K::GateHouse ? 2 : 3;
        for (int z : {b.z0 - 1, b.z1 + 1})
            for (int x : {b.x0 - 1, b.x1 + 1}) {
                const int h = height(x, z);
                if (h < SEA_LEVEL + 1 || std::abs(h - hc) > tol) return false;
            }
        if (kind == K::GateHouse)  // die ganze Reihe muss einigermassen eben sein
            for (int z = b.z0; z <= b.z1; ++z)
                for (int x = b.x0; x <= b.x1; ++x)
                    if (std::abs(height(x, z) - hc) > 2 || height(x, z) < SEA_LEVEL + 1) return false;
        b.groundY = hc;
        const int xc = (b.x0 + b.x1) / 2, zc = (b.z0 + b.z1) / 2;
        switch (b.facing) {
            case 0: b.door = {b.x1 + 1, zc}; break;
            case 1: b.door = {b.x0 - 1, zc}; break;
            case 2: b.door = {xc, b.z1 + 1}; break;
            default: b.door = {xc, b.z0 - 1}; break;
        }
        v.buildings.push_back(b);
        return true;
    };

    int accepted = 0;
    for (int attempt = 0; attempt < 120 && accepted < 10; ++attempt) {
        const float ang = rng.uniform() * 6.2831853f;
        const float dist = 12.0f + rng.uniform() * 19.0f;
        if (tryPlace(order[accepted], ang, dist)) ++accepted;
    }
    if (accepted < 3) return v;
    // Dorftor am Rand
    for (int attempt = 0; attempt < 16; ++attempt)
        if (tryPlace(K::GateHouse, rng.uniform() * 6.2831853f, 34.0f)) break;
    // Kirche: nach allem anderen versucht, damit die uebrigen Gebaeude unveraendert bleiben (nicht jedes Dorf hat Platz)
    for (int attempt = 0; attempt < 80; ++attempt)
        if (tryPlace(K::Church, rng.uniform() * 6.2831853f, 14.0f + rng.uniform() * 17.0f)) break;

    // Wege: von jeder Tuer zum Zentrum, erst entlang der Tuerachse, dann quer
    std::vector<std::pair<int, int>> seen;
    auto inBuilding = [&](int x, int z) {
        for (const VillageBuilding& o : v.buildings) {
            const int m = o.kind == VillageBuilding::Well ? 1 : 0;
            if (x >= o.x0 - m && x <= o.x1 + m && z >= o.z0 - m && z <= o.z1 + m) return true;
        }
        return false;
    };
    auto addPath = [&](int x, int z) {
        if (inBuilding(x, z)) return;
        for (const auto& p : seen)
            if (p.first == x && p.second == z) return;
        seen.emplace_back(x, z);
        v.pathCells.emplace_back(x, height(x, z), z);
    };
    for (size_t i = 1; i < v.buildings.size(); ++i) {
        const VillageBuilding& b = v.buildings[i];
        int x = b.door.x, z = b.door.y;
        const bool alongX = b.facing <= 1;
        addPath(x, z);
        if (alongX) {
            while (x != cx) {
                x += x < cx ? 1 : -1;
                addPath(x, z);
            }
            while (z != cz) {
                z += z < cz ? 1 : -1;
                addPath(x, z);
            }
        } else {
            while (z != cz) {
                z += z < cz ? 1 : -1;
                addPath(x, z);
            }
            while (x != cx) {
                x += x < cx ? 1 : -1;
                addPath(x, z);
            }
        }
        if (v.villagerSpawns.size() < 8 && b.kind != K::GateHouse && b.kind != K::Tower && b.kind != K::Barn && b.kind != K::Church)
            v.villagerSpawns.emplace_back(b.door.x, height(b.door.x, b.door.y) + 1, b.door.y);
    }
    // Truhen, Betten, Tiere
    int animal = 0;
    for (size_t i = 1; i < v.buildings.size(); ++i) {
        const VillageBuilding& b = v.buildings[i];
        if (b.kind == K::Farm || b.kind == K::GateHouse) continue;
        const bool small = b.kind == K::House && b.width() == 5;
        const int back = b.depth() - 2;  // hinterste Zeile im Inneren
        glm::ivec2 c = b.cell(back, small ? 1 : 0);
        if (b.kind == K::Tower) c = b.cell(back, 1);  // die Leiter steht in der Mitte der Rueckwand
        v.chests.emplace_back(c.x, b.groundY + 1, c.y);
        if (b.kind == K::House) {
            // zwei Betten (kleine Haeuser eines) an der Rueckwand, Kopfteil zur Truhe hin
            const int w1 = small ? -1 : -2;
            const glm::ivec2 f1 = b.cell(back, w1), h1 = b.cell(back, w1 + 1);
            v.beds.push_back({glm::ivec3(f1.x, b.groundY + 1, f1.y), glm::ivec3(h1.x, b.groundY + 1, h1.y), static_cast<int>(i)});
            if (!small) {
                const glm::ivec2 f2 = b.cell(back, 2), h2 = b.cell(back, 1);
                v.beds.push_back({glm::ivec3(f2.x, b.groundY + 1, f2.y), glm::ivec3(h2.x, b.groundY + 1, h2.y), static_cast<int>(i)});
            }
        }
        if (b.kind == K::Barn) {
            const int xc = (b.x0 + b.x1) / 2, zc = (b.z0 + b.z1) / 2;
            // drei Tiere auf der Mittellinie des Stalls
            for (int k = -1; k <= 1; ++k) {
                const int ax = b.facing <= 1 ? xc + k * 2 : xc, az = b.facing <= 1 ? zc : zc + k * 2;
                v.animalSpawns.push_back({glm::ivec3(ax, b.groundY + 1, az), (animal++) % 3});
            }
        }
    }
    v.minX = cx - RADIUS;
    v.maxX = cx + RADIUS;
    v.minZ = cz - RADIUS;
    v.maxZ = cz + RADIUS;
    exists = true;
    return v;
}

const Village* VillageGenerator::villageInCell(int cellX, int cellZ) const {
    const auto key = std::make_pair(cellX, cellZ);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        bool exists = false;
        Village v = build(cellX, cellZ, exists);
        it = cache_.emplace(key, std::make_pair(exists, std::move(v))).first;
    }
    return it->second.first ? &it->second.second : nullptr;
}

std::vector<const Village*> VillageGenerator::villagesIn(int minX, int minZ, int maxX, int maxZ) const {
    std::vector<const Village*> out;
    for (int cz = floorDivI(minZ - RADIUS - 120, CELL); cz <= floorDivI(maxZ + RADIUS, CELL); ++cz)
        for (int cx = floorDivI(minX - RADIUS - 120, CELL); cx <= floorDivI(maxX + RADIUS, CELL); ++cx) {
            const Village* v = villageInCell(cx, cz);
            if (v && !(v->maxX < minX || v->minX > maxX || v->maxZ < minZ || v->minZ > maxZ)) out.push_back(v);
        }
    return out;
}

const Village* VillageGenerator::nearest(int x, int z, int cellRadius) const {
    const int cx0 = floorDivI(x, CELL), cz0 = floorDivI(z, CELL);
    const Village* best = nullptr;
    double bestD = 1e18;
    for (int dz = -cellRadius; dz <= cellRadius; ++dz)
        for (int dx = -cellRadius; dx <= cellRadius; ++dx) {
            const Village* v = villageInCell(cx0 + dx, cz0 + dz);
            if (!v) continue;
            const double d = std::hypot(static_cast<double>(v->center.x - x), static_cast<double>(v->center.y - z));
            if (d < bestD) {
                bestD = d;
                best = v;
            }
        }
    return best;
}

bool VillageGenerator::inArea(int x, int z) const {
    for (int cz = floorDivI(z - 120, CELL); cz <= floorDivI(z, CELL); ++cz)
        for (int cx = floorDivI(x - 120, CELL); cx <= floorDivI(x, CELL); ++cx) {
            const Village* v = villageInCell(cx, cz);
            if (v && std::abs(v->center.x - x) <= RADIUS - 4 && std::abs(v->center.y - z) <= RADIUS - 4) return true;
        }
    return false;
}

bool VillageGenerator::isVillageChest(int x, int y, int z) const {
    for (const Village* v : villagesIn(x, z, x, z))
        for (const glm::ivec3& c : v->chests)
            if (c.x == x && c.y == y && c.z == z) return true;
    return false;
}

std::vector<VillageGenerator::Loot> VillageGenerator::lootFor(const Village& v, const glm::ivec3& chest) const {
    (void)v;
    Rng rng(hash3(chest.x, chest.y, chest.z, 0x100Fu) ^ static_cast<uint32_t>(seed_));
    struct Entry {
        ItemId item;
        int lo, hi, weight;
    };
    static const Entry table[] = {
        {ITEM_COAL, 2, 5, 10},        {ITEM_IRON_INGOT, 1, 3, 8},  {ITEM_GOLD_INGOT, 1, 2, 5},  {ITEM_COOKED_MEAT, 1, 3, 8},
        {ITEM_BERRIES, 2, 6, 8},      {ITEM_STICK, 2, 6, 8},       {TORCH, 3, 8, 8},            {ITEM_PAPER, 2, 5, 5},
        {ITEM_BOOK, 1, 2, 4},         {ITEM_DIAMOND, 1, 1, 1},     {ITEM_IRON_PICKAXE, 1, 1, 1}, {WOOL_WHITE, 2, 4, 4},
        {BRICKS, 3, 8, 4},            {ITEM_CLAY_BALL, 3, 6, 4},
    };
    int total = 0;
    for (const Entry& e : table) total += e.weight;
    std::vector<Loot> out;
    const int rolls = rng.range(3, 6);
    for (int i = 0; i < rolls; ++i) {
        int r = rng.range(0, total - 1);
        for (const Entry& e : table) {
            if ((r -= e.weight) < 0) {
                out.push_back({e.item, rng.range(e.lo, e.hi)});
                break;
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// Bauen
// ---------------------------------------------------------------------------------------------------------------

namespace {

struct Painter {
    Chunk& chunk;
    int ox, oz;
    bool inside(int wx, int wz) const { return wx >= ox && wx < ox + CHUNK_W && wz >= oz && wz < oz + CHUNK_W; }
    BlockId get(int wx, int y, int wz) const {
        if (!inside(wx, wz) || y < 0 || y >= CHUNK_H) return AIR;
        return chunk.get(wx - ox, y, wz - oz);
    }
    void set(int wx, int y, int wz, BlockId b) const {
        if (!inside(wx, wz) || y < 1 || y >= CHUNK_H) return;
        chunk.set(wx - ox, y, wz - oz, b);
    }
};

// Dach als Stufenpyramide: der Rand jeder Lage besteht aus Treppen mit der hohen Seite nach innen, das Innere ist voll
void roofPitched(const Painter& P, int rx0, int rz0, int rx1, int rz1, int y, int stairMat, BlockId solid) {
    for (int k = 0;; ++k) {
        const int x0 = rx0 + k, x1 = rx1 - k, z0 = rz0 + k, z1 = rz1 - k;
        if (x0 > x1 || z0 > z1) break;
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) {
                const bool edgeX = x == x0 || x == x1, edgeZ = z == z0 || z == z1;
                if (x0 == x1 || z0 == z1 || !(edgeX || edgeZ)) {
                    P.set(x, y + k, z, solid);
                    continue;
                }
                int facing;
                if (x == x1 && edgeX) facing = 1;       // Ostkante: hohe Seite nach -X
                else if (x == x0 && edgeX) facing = 0;  // Westkante: nach +X
                else if (z == z1) facing = 3;
                else facing = 2;
                P.set(x, y + k, z, makeStairs(stairMat, facing));
            }
    }
}

int gateAxisFor(int facing) { return facing <= 1 ? 1 : 0; }  // Wand in Z-Richtung (Tuer an X-Seite): Tor spannt sich entlang Z

}  // namespace

void VillageGenerator::placeInChunk(Chunk& chunk) const {
    const int ox = chunk.cx * CHUNK_W, oz = chunk.cz * CHUNK_W;
    const std::vector<const Village*> vs = villagesIn(ox, oz, ox + CHUNK_W - 1, oz + CHUNK_W - 1);
    if (vs.empty()) return;
    const Painter P{chunk, ox, oz};
    auto height = [&](int x, int z) { return terrain_.column(x, z).height; };
    using K = VillageBuilding;

    for (const Village* vp : vs) {
        const Village& v = *vp;

        // Wege (zuerst; Gebaeude und Brunnen ueberschreiben sie nicht, weil Wegzellen dort ausgelassen sind)
        for (size_t i = 0; i < v.pathCells.size(); ++i) {
            const glm::ivec3& c = v.pathCells[i];
            if (!P.inside(c.x, c.z)) continue;
            const BlockId top = P.get(c.x, c.y, c.z);
            if (top == GRASS || top == DIRT || top == SAND || top == SANDSTONE) P.set(c.x, c.y, c.z, GRAVEL);
            for (int y = c.y + 1; y <= c.y + 3; ++y) {
                const BlockId b = P.get(c.x, y, c.z);
                if (isPlant(b) || isLeaves(b) || isLogBlock(b)) P.set(c.x, y, c.z, AIR);
            }
            // Laternenpfahl: zwei Zaunstuecke und eine Laterne
            if (i % 8 == 4 && P.get(c.x, c.y, c.z) == GRAVEL) {
                P.set(c.x, c.y + 1, c.z, FENCE);
                P.set(c.x, c.y + 2, c.z, FENCE);
                P.set(c.x, c.y + 3, c.z, LANTERN);
            }
        }

        for (const VillageBuilding& b : v.buildings) {
            const int g = b.groundY;
            const bool farm = b.kind == K::Farm;
            // Gelaende: Fundament auffuellen, Ueberstehendes abtragen, Baeume und Pflanzen im Umfeld entfernen
            const int roofRoom = b.kind == K::Church ? 20 : (b.kind == K::Tower ? 18 : 14);
            for (int z = b.z0 - 1; z <= b.z1 + 1; ++z)
                for (int x = b.x0 - 1; x <= b.x1 + 1; ++x) {
                    if (!P.inside(x, z)) continue;
                    const int h = height(x, z);
                    const bool core = x >= b.x0 && x <= b.x1 && z >= b.z0 && z <= b.z1;
                    if (core) {
                        for (int y = h + 1; y < g; ++y) P.set(x, y, z, farm ? DIRT : COBBLE);
                        for (int y = g + 1; y <= std::max(h, g) + roofRoom && y < CHUNK_H; ++y) P.set(x, y, z, AIR);
                        if (h > g)
                            for (int y = g; y <= h; ++y) P.set(x, y, z, AIR);
                    } else {
                        for (int y = h + 1; y <= g + roofRoom && y < CHUNK_H; ++y) P.set(x, y, z, AIR);
                    }
                }

            const int xc = (b.x0 + b.x1) / 2, zc = (b.z0 + b.z1) / 2;
            const bool doorAlongX = b.facing <= 1;
            const int fx = b.facing == 0 ? 1 : (b.facing == 1 ? -1 : 0), fz = b.facing == 2 ? 1 : (b.facing == 3 ? -1 : 0);  // Richtung nach aussen
            auto onDoorWall = [&](int x, int z) {
                return (b.facing == 0 && x == b.x1) || (b.facing == 1 && x == b.x0) || (b.facing == 2 && z == b.z1) || (b.facing == 3 && z == b.z0);
            };
            // Versatz entlang der Wand relativ zur Mitte
            auto wallOff = [&](int x, int z) { return doorAlongX ? z - zc : x - xc; };
            // Zelle im Gebaeude: u = Tiefe vom Eingang aus (0 = Tuerwand), w = Abstand von der Mittellinie
            auto cellAt = [&](int u, int w, int& x, int& z) {
                if (b.facing == 0) { x = b.x1 - u; z = zc + w; }
                else if (b.facing == 1) { x = b.x0 + u; z = zc + w; }
                else if (b.facing == 2) { z = b.z1 - u; x = xc + w; }
                else { z = b.z0 + u; x = xc + w; }
            };
            (void)fx;
            (void)fz;

            switch (b.kind) {
                case K::Well: {
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            const bool rim = x == b.x0 || x == b.x1 || z == b.z0 || z == b.z1;
                            const bool post = (x == b.x0 || x == b.x1) && (z == b.z0 || z == b.z1);
                            P.set(x, g, z, rim ? COBBLE : WATER);
                            if (rim) P.set(x, g + 1, z, post ? static_cast<BlockId>(WALL_COBBLE) : makeSlab(1, false));
                            if (post) {
                                P.set(x, g + 2, z, FENCE);
                                P.set(x, g + 3, z, FENCE);
                            }
                            P.set(x, g + 4, z, PLANKS);
                        }
                    P.set(xc, g + 3, zc, LANTERN_HANG);
                    break;
                }
                case K::House:
                case K::Library:
                case K::Smithy: {
                    const bool smithy = b.kind == K::Smithy, library = b.kind == K::Library;
                    const BlockId wall = smithy ? COBBLE : PLANKS;
                    const BlockId corner = smithy ? SPRUCE_LOG : (library ? BIRCH_LOG : LOG);
                    const int roofMat = smithy ? 2 : (library ? 0 : 3);  // Steinziegel, Bretter, Ziegel
                    const BlockId roofSolid = stairMaterialBlock(roofMat);
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            const bool edgeX = x == b.x0 || x == b.x1, edgeZ = z == b.z0 || z == b.z1;
                            const bool perim = edgeX || edgeZ, isCorner = edgeX && edgeZ;
                            P.set(x, g, z, perim ? COBBLE : PLANKS);
                            for (int y = g + 1; y <= g + 3; ++y) {
                                if (!perim) {
                                    P.set(x, y, z, AIR);
                                    continue;
                                }
                                BlockId bl = isCorner ? corner : wall;
                                if (!isCorner) {
                                    const bool dw = onDoorWall(x, z);
                                    const int off = wallOff(x, z);
                                    if (dw && off == 0 && y <= g + 2) bl = AIR;  // Tuer (wird unten eingesetzt)
                                    else if (y == g + 2 && ((dw && std::abs(off) == 2) || (!dw && (off == 0 || std::abs(off) == 2)))) bl = GLASS_PANE;
                                }
                                P.set(x, y, z, bl);
                            }
                        }
                    // Tuer
                    {
                        int dx0, dz0;
                        cellAt(0, 0, dx0, dz0);
                        P.set(dx0, g + 1, dz0, makeDoor(0, false, false, b.facing));
                        P.set(dx0, g + 2, dz0, makeDoor(0, true, false, b.facing));
                    }
                    // Dach mit Ueberstand
                    for (int z = b.z0 - 1; z <= b.z1 + 1; ++z)
                        for (int x = b.x0 - 1; x <= b.x1 + 1; ++x) P.set(x, g + 4, z, roofSolid);  // Decke, Ueberstand wird gleich ueberbaut
                    roofPitched(P, b.x0 - 1, b.z0 - 1, b.x1 + 1, b.z1 + 1, g + 4, roofMat, roofSolid);
                    // Einrichtung
                    const int back = b.depth() - 2;
                    const bool smallHouse = b.kind == K::House && b.width() == 5;
                    const int tw = smallHouse ? 1 : 2;
                    const glm::ivec2 table = b.cell(1, tw);  // vordere Ecke neben der Tuer
                    if (b.kind == K::House) {
                        for (const Village::Bed& bed : v.beds) {
                            if (&v.buildings[static_cast<size_t>(bed.building)] != &b) continue;
                            const int fdx = bed.head.x - bed.foot.x, fdz = bed.head.z - bed.foot.z;
                            const int bf = fdx > 0 ? 0 : (fdx < 0 ? 1 : (fdz > 0 ? 2 : 3));
                            P.set(bed.foot.x, g + 1, bed.foot.z, makeBedPart(false, bf));
                            P.set(bed.head.x, g + 1, bed.head.z, makeBedPart(true, bf));
                        }
                        P.set(table.x, g + 1, table.y, CRAFTING_TABLE);
                    } else if (library) {
                        P.set(table.x, g + 1, table.y, CRAFTING_TABLE);
                        for (int w = -(b.width() / 2 - 1); w <= b.width() / 2 - 1; ++w) {
                            const glm::ivec2 s = b.cell(back, w);
                            if (w == 0) continue;  // hier steht die Truhe
                            P.set(s.x, g + 1, s.y, BOOKSHELF);
                            P.set(s.x, g + 2, s.y, BOOKSHELF);
                        }
                    } else {
                        const glm::ivec2 fur = b.cell(back, -2), tab = b.cell(back, 2);
                        P.set(fur.x, g + 1, fur.y, FURNACE);
                        P.set(tab.x, g + 1, tab.y, CRAFTING_TABLE);
                    }
                    for (const glm::ivec3& c : v.chests)
                        if (c.x >= b.x0 && c.x <= b.x1 && c.z >= b.z0 && c.z <= b.z1) P.set(c.x, c.y, c.z, CHEST);
                    P.set(xc, g + 3, zc, LANTERN_HANG);
                    break;
                }
                case K::Farm: {
                    // Zaun, Tor, Acker mit Wasserrinne in der Mitte
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            const bool perim = x == b.x0 || x == b.x1 || z == b.z0 || z == b.z1;
                            const bool isCorner = (x == b.x0 || x == b.x1) && (z == b.z0 || z == b.z1);
                            if (perim) {
                                P.set(x, g, z, DIRT);
                                P.set(x, g + 1, z, FENCE);
                                if (isCorner) P.set(x, g + 2, z, LANTERN);
                                continue;
                            }
                            // u: Tiefe von der Tuerwand, w: seitlich
                            // Die Wasserrinne verlaeuft in der Mitte quer zur Tuerachse (der Eingang liegt am Rinnenrand)
                            const int lane = doorAlongX ? x - xc : z - zc;  // Abstand von der Mittellinie quer zur Tuerachse
                            if (lane == 0) {
                                P.set(x, g, z, WATER);
                                continue;
                            }
                            P.set(x, g, z, FARMLAND);
                            const uint32_t hv = hash3(x, g, z, 0xFA2Au);
                            const int stage = hv % 10u < 7u ? 3 : 1 + static_cast<int>(hv % 3u);
                            P.set(x, g + 1, z, static_cast<BlockId>((lane < 0 ? WHEAT_0 : CARROT_0) + stage));
                        }
                    // Tor in der Zaunmitte
                    {
                        int gx, gz;
                        cellAt(0, 0, gx, gz);
                        P.set(gx, g + 1, gz, makeGate(0, false, gateAxisFor(b.facing)));
                    }
                    break;
                }
                case K::Barn: {
                    const int sMat = 0;
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            const bool edgeX = x == b.x0 || x == b.x1, edgeZ = z == b.z0 || z == b.z1;
                            const bool perim = edgeX || edgeZ, isCorner = edgeX && edgeZ;
                            P.set(x, g, z, perim ? COBBLE : GRAVEL);
                            for (int y = g + 1; y <= g + 4; ++y) {
                                if (!perim) {
                                    P.set(x, y, z, AIR);
                                    continue;
                                }
                                BlockId bl = isCorner ? SPRUCE_LOG : PLANKS;
                                if (!isCorner) {
                                    const bool dw = onDoorWall(x, z);
                                    const int off = wallOff(x, z);
                                    if (dw && std::abs(off) <= 1 && y <= g + 3) bl = makeGate(0, false, gateAxisFor(b.facing));  // grosses Tor 3 x 3
                                    else if (!dw && y == g + 3 && std::abs(off) % 3 == 2) bl = GLASS_PANE;
                                }
                                P.set(x, y, z, bl);
                            }
                        }
                    for (int z = b.z0 - 1; z <= b.z1 + 1; ++z)
                        for (int x = b.x0 - 1; x <= b.x1 + 1; ++x) P.set(x, g + 5, z, PLANKS);
                    roofPitched(P, b.x0 - 1, b.z0 - 1, b.x1 + 1, b.z1 + 1, g + 5, sMat, PLANKS);
                    // Heu, Truhe, Laternen, Boxentrenner
                    for (int w = -3; w <= 3; w += 6) {
                        int hx, hz;
                        cellAt(b.facing <= 1 ? (b.x1 - b.x0 - 1) : (b.z1 - b.z0 - 1), w / 3 * 3, hx, hz);
                        P.set(hx, g + 1, hz, WOOL_YELLOW);
                        P.set(hx, g + 2, hz, WOOL_YELLOW);
                    }
                    for (int u = 2; u <= 8; u += 3) {
                        int lx, lz;
                        cellAt(u, 0, lx, lz);
                        P.set(lx, g + 4, lz, LANTERN_HANG);
                    }
                    for (const glm::ivec3& c : v.chests)
                        if (c.x >= b.x0 && c.x <= b.x1 && c.z >= b.z0 && c.z <= b.z1) P.set(c.x, c.y, c.z, CHEST);
                    break;
                }
                case K::Market: {
                    // vier Pfosten, Markise aus rot-weisser Wolle, Theke mit Luecke, Truhe, Lampen
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            P.set(x, g, z, PLANKS);
                            for (int y = g + 1; y <= g + 3; ++y) P.set(x, y, z, AIR);
                            const bool isCorner = (x == b.x0 || x == b.x1) && (z == b.z0 || z == b.z1);
                            if (isCorner)
                                for (int y = g + 1; y <= g + 3; ++y) P.set(x, y, z, FENCE);
                            const int off = wallOff(x, z);
                            P.set(x, g + 4, z, std::abs(off) % 2 == 0 ? WOOL_RED : WOOL_WHITE);
                            if (onDoorWall(x, z) && !isCorner && off != 0) {
                                P.set(x, g + 1, z, PLANKS);
                                P.set(x, g + 2, z, (std::abs(off) % 2 == 1) ? LANTERN : AIR);
                            }
                        }
                    // Waren: Kuerbisse und Heu hinten
                    {
                        int px, pz;
                        cellAt(b.facing <= 1 ? (b.x1 - b.x0 - 1) : (b.z1 - b.z0 - 1), -2, px, pz);
                        P.set(px, g + 1, pz, PUMPKIN);
                        cellAt(b.facing <= 1 ? (b.x1 - b.x0 - 1) : (b.z1 - b.z0 - 1), 2, px, pz);
                        P.set(px, g + 1, pz, WOOL_YELLOW);
                        cellAt(2, 0, px, pz);
                        P.set(px, g + 3, pz, LANTERN_HANG);
                    }
                    for (const glm::ivec3& c : v.chests)
                        if (c.x >= b.x0 && c.x <= b.x1 && c.z >= b.z0 && c.z <= b.z1) P.set(c.x, c.y, c.z, CHEST);
                    break;
                }
                case K::Tower: {
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            const bool edgeX = x == b.x0 || x == b.x1, edgeZ = z == b.z0 || z == b.z1;
                            const bool perim = edgeX || edgeZ, isCorner = edgeX && edgeZ;
                            P.set(x, g, z, perim ? COBBLE : PLANKS);
                            for (int y = g + 1; y <= g + 9; ++y) {
                                if (!perim) {
                                    P.set(x, y, z, AIR);
                                    continue;
                                }
                                BlockId bl = isCorner ? STONE_BRICKS : (y <= g + 2 ? COBBLE : STONE_BRICKS);
                                if (!isCorner) {
                                    const bool dw = onDoorWall(x, z);
                                    const int off = wallOff(x, z);
                                    if (dw && off == 0 && y <= g + 2) bl = AIR;
                                    else if (off == 0 && (y == g + 4 || y == g + 7)) bl = IRON_BARS;
                                    else if (off == 0 && y == g + 8 && !dw) bl = GLASS_PANE;
                                }
                                P.set(x, y, z, bl);
                            }
                        }
                    {
                        int dx0, dz0;
                        cellAt(0, 0, dx0, dz0);
                        P.set(dx0, g + 1, dz0, makeDoor(1, false, false, b.facing));
                        P.set(dx0, g + 2, dz0, makeDoor(1, true, false, b.facing));
                    }
                    // Leiter an der Rueckwand, Zwischenboden und Plattform mit Zinnen (beide mit Loch an der Leiter)
                    {
                        const glm::ivec2 lad = b.cell(b.depth() - 2, 0);
                        const int lf = b.facing ^ 1;  // der Traeger liegt auf der Seite der Rueckwand
                        for (int z = b.z0; z <= b.z1; ++z)
                            for (int x = b.x0; x <= b.x1; ++x) {
                                const bool hole = x == lad.x && z == lad.y;
                                const bool perim = x == b.x0 || x == b.x1 || z == b.z0 || z == b.z1;
                                if (!hole) {
                                    P.set(x, g + 10, z, STONE_BRICKS);
                                    if (!perim) P.set(x, g + 5, z, PLANKS);
                                } else {
                                    P.set(x, g + 10, z, AIR);
                                    P.set(x, g + 5, z, AIR);
                                }
                                // Zinnen: jede zweite Randzelle, an den Ecken hoeher mit Laterne
                                if (perim) {
                                    const bool corner = (x == b.x0 || x == b.x1) && (z == b.z0 || z == b.z1);
                                    if (corner) {
                                        P.set(x, g + 11, z, STONE_BRICKS);
                                        P.set(x, g + 12, z, STONE_BRICKS);
                                        P.set(x, g + 13, z, LANTERN);
                                    } else if ((x + z) % 2 == 0) {
                                        P.set(x, g + 11, z, STONE_BRICKS);
                                    }
                                }
                            }
                        for (int y = g + 1; y <= g + 10; ++y) P.set(lad.x, y, lad.y, makeLadder(lf));
                    }
                    P.set((b.x0 + b.x1) / 2, g + 4, (b.z0 + b.z1) / 2, LANTERN_HANG);
                    P.set((b.x0 + b.x1) / 2, g + 9, (b.z0 + b.z1) / 2, LANTERN_HANG);
                    P.set(b.x0 + 1, g + 1, b.z0 + 1, LANTERN);
                    for (const glm::ivec3& c : v.chests)
                        if (c.x >= b.x0 && c.x <= b.x1 && c.z >= b.z0 && c.z <= b.z1) P.set(c.x, c.y, c.z, CHEST);
                    break;
                }
                case K::Church: {
                    // Kirchenschiff 11 x 7: Steinziegelwaende, Fenster, roter Mittelgang, Baenke, Altar mit Truhe,
                    // Walmdach aus Bruchsteintreppen und vorn ein Glockenturm mit offener Glockenstube und Spitze
                    const int D = b.depth(), half = b.width() / 2;
                    const BlockId roofSolid = stairMaterialBlock(1);
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            const int u = doorAlongX ? (b.facing == 0 ? b.x1 - x : x - b.x0) : (b.facing == 2 ? b.z1 - z : z - b.z0);
                            const int w = wallOff(x, z), aw = std::abs(w);
                            const bool endWall = u == 0 || u == D - 1, sideWall = aw == half;
                            const bool perim = endWall || sideWall, isCorner = endWall && sideWall;
                            P.set(x, g, z, perim ? COBBLE : (w == 0 && u >= 1 && u <= D - 3 ? WOOL_RED : PLANKS));
                            for (int y = g + 1; y <= g + 5; ++y) {
                                if (!perim) {
                                    P.set(x, y, z, AIR);
                                    continue;
                                }
                                BlockId bl = isCorner ? SPRUCE_LOG : (y == g + 1 ? COBBLE : STONE_BRICKS);
                                if (!isCorner) {
                                    if (sideWall && u % 2 == 0 && u >= 2 && u <= D - 3 && y >= g + 2 && y <= g + 4) bl = GLASS_PANE;  // hohe Fenster
                                    else if (u == 0 && w == 0 && y <= g + 2) bl = AIR;                                              // Tuer
                                    else if (u == 0 && w == 0 && y >= g + 3 && y <= g + 4) bl = GLASS_PANE;                         // Oberlicht
                                    else if (u == D - 1 && ((w == 0 && y >= g + 2 && y <= g + 4) || (aw == 1 && y == g + 3))) bl = GLASS_PANE;  // Kreuzfenster
                                }
                                P.set(x, y, z, bl);
                            }
                        }
                    {
                        int dx0, dz0;
                        cellAt(0, 0, dx0, dz0);
                        P.set(dx0, g + 1, dz0, makeDoor(0, false, false, b.facing));
                        P.set(dx0, g + 2, dz0, makeDoor(0, true, false, b.facing));
                    }
                    // Decke und Dach mit Ueberstand
                    for (int z = b.z0 - 1; z <= b.z1 + 1; ++z)
                        for (int x = b.x0 - 1; x <= b.x1 + 1; ++x) P.set(x, g + 6, z, roofSolid);
                    roofPitched(P, b.x0 - 1, b.z0 - 1, b.x1 + 1, b.z1 + 1, g + 6, 1, roofSolid);
                    // Baenke (Treppen, Lehne zur Tuer), Altar mit Buecherregalen und Lichtern
                    for (int u = 3; u <= 7; u += 2)
                        for (int w = -2; w <= 2; ++w) {
                            if (w == 0) continue;
                            int px, pz;
                            cellAt(u, w, px, pz);
                            P.set(px, g + 1, pz, makeStairs(0, b.facing));
                        }
                    for (int w = -1; w <= 1; w += 2) {
                        int px, pz;
                        cellAt(D - 2, w, px, pz);
                        P.set(px, g + 1, pz, BOOKSHELF);
                        P.set(px, g + 2, pz, LANTERN);
                    }
                    for (int u = 3; u <= 6; u += 3) {
                        int px, pz;
                        cellAt(u, 0, px, pz);
                        P.set(px, g + 5, pz, LANTERN_HANG);
                    }
                    for (const glm::ivec3& c : v.chests)
                        if (c.x >= b.x0 && c.x <= b.x1 && c.z >= b.z0 && c.z <= b.z1) P.set(c.x, c.y, c.z, CHEST);
                    // Glockenturm ueber dem Eingang (3 x 3): Boden, vier Pfeiler, offene Glockenstube, Spitze
                    {
                        int ax, az, bx, bz;
                        cellAt(0, -1, ax, az);
                        cellAt(2, 1, bx, bz);
                        const int tx0 = std::min(ax, bx), tx1 = std::max(ax, bx), tz0 = std::min(az, bz), tz1 = std::max(az, bz);
                        const int tcx = (tx0 + tx1) / 2, tcz = (tz0 + tz1) / 2;
                        for (int z = tz0; z <= tz1; ++z)
                            for (int x = tx0; x <= tx1; ++x) {
                                const bool edge = x == tx0 || x == tx1 || z == tz0 || z == tz1;
                                const bool isCorner = (x == tx0 || x == tx1) && (z == tz0 || z == tz1);
                                P.set(x, g + 6, z, STONE_BRICKS);
                                for (int y = g + 7; y <= g + 10; ++y) {
                                    if (!edge) {
                                        P.set(x, y, z, y == g + 10 ? STONE_BRICKS : AIR);  // Kappe, an der die "Glocke" haengt
                                        continue;
                                    }
                                    BlockId bl = STONE_BRICKS;
                                    if (!isCorner && (y == g + 8 || y == g + 9)) bl = IRON_BARS;  // Schallloecher der Glockenstube
                                    P.set(x, y, z, bl);
                                }
                                if (!edge) P.set(x, g + 11, z, STONE_BRICKS);
                            }
                        P.set(tcx, g + 9, tcz, LANTERN_HANG);  // "Glocke"
                        roofPitched(P, tx0 - 1, tz0 - 1, tx1 + 1, tz1 + 1, g + 11, 1, roofSolid);
                        P.set(tcx, g + 14, tcz, FENCE);
                        P.set(tcx, g + 15, tcz, LANTERN);
                    }
                    break;
                }
                case K::GateHouse: {
                    // eine Reihe quer zur Strasse: Pfeiler, Laternen, Mauerstuecke und ein grosses Tor (3 x 3)
                    for (int z = b.z0; z <= b.z1; ++z)
                        for (int x = b.x0; x <= b.x1; ++x) {
                            const int off = wallOff(x, z);
                            const int a = std::abs(off);
                            P.set(x, g, z, a <= 1 ? GRAVEL : COBBLE);
                            for (int y = g + 1; y <= g + 6; ++y) P.set(x, y, z, AIR);
                            if (a <= 1) {
                                for (int y = g + 1; y <= g + 3; ++y) P.set(x, y, z, makeGate(0, false, gateAxisFor(b.facing)));
                                P.set(x, g + 4, z, STONE_BRICKS);
                            } else if (a == 2) {
                                for (int y = g + 1; y <= g + 5; ++y) P.set(x, y, z, STONE_BRICKS);
                                P.set(x, g + 6, z, LANTERN);
                            } else {
                                P.set(x, g + 1, z, WALL_COBBLE);
                                P.set(x, g + 2, z, WALL_COBBLE);
                            }
                        }
                    break;
                }
            }
        }
    }
}

}  // namespace oc
