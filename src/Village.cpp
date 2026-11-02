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

    static const VillageBuilding::Kind order[8] = {VillageBuilding::House,   VillageBuilding::Library, VillageBuilding::House,
                                                   VillageBuilding::Smithy,  VillageBuilding::House,   VillageBuilding::House,
                                                   VillageBuilding::Library, VillageBuilding::House};
    int accepted = 0;
    for (int attempt = 0; attempt < 60 && accepted < 8; ++attempt) {
        const float ang = rng.uniform() * 6.2831853f;
        const float dist = 12.0f + rng.uniform() * 18.0f;
        const int px = cx + static_cast<int>(std::lround(std::cos(ang) * dist));
        const int pz = cz + static_cast<int>(std::lround(std::sin(ang) * dist));
        const int dx = px - cx, dz = pz - cz;
        const bool alongX = std::abs(dx) >= std::abs(dz);  // Tuer zeigt entlang dieser Achse zum Zentrum
        VillageBuilding b;
        b.kind = order[accepted];
        int depth = 7, width = 7;  // depth: Ausdehnung entlang der Tuerachse
        if (b.kind == VillageBuilding::Library) width = 9;
        else if (b.kind == VillageBuilding::House && rng.range(0, 3) == 0) depth = width = 5;
        const int ex = alongX ? depth : width, ez = alongX ? width : depth;
        b.x0 = px - ex / 2;
        b.x1 = b.x0 + ex - 1;
        b.z0 = pz - ez / 2;
        b.z1 = b.z0 + ez - 1;
        b.facing = alongX ? (dx > 0 ? 1 : 0) : (dz > 0 ? 3 : 2);
        bool ok = true;
        for (const VillageBuilding& o : v.buildings) ok = ok && !overlaps(o, b.x0, b.z0, b.x1, b.z1, o.kind == VillageBuilding::Well ? 4 : 3);
        if (!ok) continue;
        const int hc = height(px, pz);
        if (hc < SEA_LEVEL + 1 || std::abs(hc - v.groundY) > 5) continue;
        for (int z : {b.z0 - 1, b.z1 + 1})
            for (int x : {b.x0 - 1, b.x1 + 1}) {
                const int h = height(x, z);
                ok = ok && h >= SEA_LEVEL + 1 && std::abs(h - hc) <= 3;
            }
        if (!ok) continue;
        b.groundY = hc;
        const int xc = (b.x0 + b.x1) / 2, zc = (b.z0 + b.z1) / 2;
        switch (b.facing) {
            case 0: b.door = {b.x1 + 1, zc}; break;
            case 1: b.door = {b.x0 - 1, zc}; break;
            case 2: b.door = {xc, b.z1 + 1}; break;
            default: b.door = {xc, b.z0 - 1}; break;
        }
        v.buildings.push_back(b);
        ++accepted;
    }
    if (accepted < 3) return v;

    // Wege: von jeder Tuer zum Zentrum, erst entlang der Tuerachse, dann quer
    std::vector<std::pair<int, int>> seen;
    auto inBuilding = [&](int x, int z) {
        for (const VillageBuilding& o : v.buildings)
            if (x >= o.x0 - (o.kind == VillageBuilding::Well ? 1 : 0) && x <= o.x1 + (o.kind == VillageBuilding::Well ? 1 : 0) &&
                z >= o.z0 - (o.kind == VillageBuilding::Well ? 1 : 0) && z <= o.z1 + (o.kind == VillageBuilding::Well ? 1 : 0))
                return true;
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
        if (v.villagerSpawns.size() < 6) v.villagerSpawns.emplace_back(b.door.x, height(b.door.x, b.door.y) + 1, b.door.y);
    }
    // Truhen
    for (size_t i = 1; i < v.buildings.size(); ++i) {
        const VillageBuilding& b = v.buildings[i];
        const int xc = (b.x0 + b.x1) / 2, zc = (b.z0 + b.z1) / 2;
        int x = xc, z = zc;
        switch (b.facing) {
            case 0: x = b.x0 + 1; break;
            case 1: x = b.x1 - 1; break;
            case 2: z = b.z0 + 1; break;
            default: z = b.z1 - 1; break;
        }
        v.chests.emplace_back(x, b.groundY + 1, z);
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

void VillageGenerator::placeInChunk(Chunk& chunk) const {
    const int ox = chunk.cx * CHUNK_W, oz = chunk.cz * CHUNK_W;
    const std::vector<const Village*> vs = villagesIn(ox, oz, ox + CHUNK_W - 1, oz + CHUNK_W - 1);
    if (vs.empty()) return;

    auto inside = [&](int wx, int wz) { return wx >= ox && wx < ox + CHUNK_W && wz >= oz && wz < oz + CHUNK_W; };
    auto get = [&](int wx, int y, int wz) -> BlockId {
        if (!inside(wx, wz) || y < 0 || y >= CHUNK_H) return AIR;
        return chunk.get(wx - ox, y, wz - oz);
    };
    auto set = [&](int wx, int y, int wz, BlockId b) {
        if (!inside(wx, wz) || y < 1 || y >= CHUNK_H) return;
        chunk.set(wx - ox, y, wz - oz, b);
    };
    auto height = [&](int x, int z) { return terrain_.column(x, z).height; };

    for (const Village* vp : vs) {
        const Village& v = *vp;

        // Wege (zuerst, Gebaeude und Brunnen ueberschreiben sie nicht, weil Wegzellen dort ausgelassen sind)
        for (size_t i = 0; i < v.pathCells.size(); ++i) {
            const glm::ivec3& c = v.pathCells[i];
            if (!inside(c.x, c.z)) continue;
            const BlockId top = get(c.x, c.y, c.z);
            if (top == GRASS || top == DIRT || top == SAND || top == SANDSTONE) set(c.x, c.y, c.z, GRAVEL);
            for (int y = c.y + 1; y <= c.y + 3; ++y) {
                const BlockId b = get(c.x, y, c.z);
                if (isPlant(b) || isLeaves(b) || isLogBlock(b)) set(c.x, y, c.z, AIR);
            }
            if (i % 7 == 3 && get(c.x, c.y, c.z) == GRAVEL) set(c.x, c.y + 1, c.z, TORCH);
        }

        for (const VillageBuilding& b : v.buildings) {
            const int g = b.groundY;
            const bool well = b.kind == VillageBuilding::Well;
            // Gelaende: Fundament auffuellen, Ueberstehendes abtragen, Baeume und Pflanzen im Umfeld entfernen
            for (int z = b.z0 - 1; z <= b.z1 + 1; ++z)
                for (int x = b.x0 - 1; x <= b.x1 + 1; ++x) {
                    if (!inside(x, z)) continue;
                    const int h = height(x, z);
                    const bool core = x >= b.x0 && x <= b.x1 && z >= b.z0 && z <= b.z1;
                    if (core) {
                        for (int y = h + 1; y < g; ++y) set(x, y, z, COBBLE);
                        for (int y = g + 1; y <= std::max(h, g) + 12 && y < CHUNK_H; ++y) set(x, y, z, AIR);
                        if (h > g)
                            for (int y = g; y <= h; ++y) set(x, y, z, AIR);
                    } else {
                        for (int y = h + 1; y <= g + 12 && y < CHUNK_H; ++y) set(x, y, z, AIR);
                    }
                }

            if (well) {
                for (int z = b.z0; z <= b.z1; ++z)
                    for (int x = b.x0; x <= b.x1; ++x) {
                        const bool rim = x == b.x0 || x == b.x1 || z == b.z0 || z == b.z1;
                        const bool post = rim && (x == b.x0 || x == b.x1) && (z == b.z0 || z == b.z1);
                        set(x, g, z, rim ? COBBLE : WATER);
                        if (rim) set(x, g + 1, z, COBBLE);
                        if (post) {
                            set(x, g + 2, z, LOG);
                            set(x, g + 3, z, LOG);
                        }
                        set(x, g + 4, z, PLANKS);
                    }
                continue;
            }

            const bool smithy = b.kind == VillageBuilding::Smithy, library = b.kind == VillageBuilding::Library;
            const BlockId wall = smithy ? COBBLE : PLANKS;
            const BlockId corner = smithy ? SPRUCE_LOG : (library ? BIRCH_LOG : LOG);
            const BlockId roof = smithy ? STONE_BRICKS : (library ? PLANKS : BRICKS);
            const int xc = (b.x0 + b.x1) / 2, zc = (b.z0 + b.z1) / 2;
            const bool doorAlongX = b.facing <= 1;
            for (int z = b.z0; z <= b.z1; ++z)
                for (int x = b.x0; x <= b.x1; ++x) {
                    if (!inside(x, z)) continue;
                    const bool edgeX = x == b.x0 || x == b.x1, edgeZ = z == b.z0 || z == b.z1;
                    const bool perim = edgeX || edgeZ, isCorner = edgeX && edgeZ;
                    set(x, g, z, perim ? COBBLE : PLANKS);
                    for (int y = g + 1; y <= g + 3; ++y) {
                        if (!perim) {
                            set(x, y, z, AIR);
                            continue;
                        }
                        BlockId bl = isCorner ? corner : wall;
                        if (!isCorner) {
                            // Lage entlang der Wand relativ zur Mitte, und ob es die Tuerwand ist
                            const bool onDoorWall = (b.facing == 0 && x == b.x1) || (b.facing == 1 && x == b.x0) ||
                                                    (b.facing == 2 && z == b.z1) || (b.facing == 3 && z == b.z0);
                            const int off = edgeX ? z - zc : x - xc;
                            if (onDoorWall && off == 0 && y <= g + 2) bl = AIR;  // Tuer
                            else if (y == g + 2 && ((onDoorWall && std::abs(off) == 2) || (!onDoorWall && (off == 0 || std::abs(off) == 2))))
                                bl = GLASS;
                        }
                        set(x, y, z, bl);
                    }
                }
            // Dach: Stufenpyramide mit Ueberstand
            for (int k = 0;; ++k) {
                const int rx0 = b.x0 - 1 + k, rx1 = b.x1 + 1 - k, rz0 = b.z0 - 1 + k, rz1 = b.z1 + 1 - k;
                if (rx0 > rx1 || rz0 > rz1) break;
                for (int z = rz0; z <= rz1; ++z)
                    for (int x = rx0; x <= rx1; ++x) set(x, g + 4 + k, z, roof);
            }
            // Einrichtung
            const int bx = b.facing == 1 ? b.x1 - 1 : b.x0 + 1, bz = b.facing == 3 ? b.z1 - 1 : b.z0 + 1;  // hintere Ecke
            const int tx = doorAlongX ? bx : b.x1 - 1, tz = doorAlongX ? b.z1 - 1 : bz;                      // andere hintere Ecke
            if (b.kind == VillageBuilding::House) {
                set(bx, g + 1, bz, BED);
                set(tx, g + 1, tz, CRAFTING_TABLE);
            } else if (library) {
                set(tx, g + 1, tz, CRAFTING_TABLE);
                // Buecherregale an der Rueckwand (zwei Hoehen), Truhe bleibt frei
                for (int z = b.z0 + 1; z <= b.z1 - 1; ++z)
                    for (int x = b.x0 + 1; x <= b.x1 - 1; ++x) {
                        const bool back = (b.facing == 0 && x == b.x0 + 1) || (b.facing == 1 && x == b.x1 - 1) ||
                                          (b.facing == 2 && z == b.z0 + 1) || (b.facing == 3 && z == b.z1 - 1);
                        if (!back) continue;
                        const bool isChest = false;
                        (void)isChest;
                        set(x, g + 1, z, BOOKSHELF);
                        set(x, g + 2, z, BOOKSHELF);
                    }
            } else {
                set(bx, g + 1, bz, FURNACE);
                set(tx, g + 1, tz, CRAFTING_TABLE);
            }
            for (const glm::ivec3& c : v.chests)
                if (c.x >= b.x0 && c.x <= b.x1 && c.z >= b.z0 && c.z <= b.z1) set(c.x, c.y, c.z, CHEST);
            // Fackeln an den vorderen Ecken
            const int fx = b.facing == 0 ? b.x1 - 1 : b.x0 + 1, fz = b.facing == 2 ? b.z1 - 1 : b.z0 + 1;
            set(fx, g + 1, fz, TORCH);
            const int fx2 = doorAlongX ? fx : b.x1 - 1, fz2 = doorAlongX ? b.z1 - 1 : fz;
            set(fx2, g + 1, fz2, TORCH);
        }
    }
}

}  // namespace oc
