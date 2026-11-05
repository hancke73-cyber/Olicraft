#include "World.h"

#include "BlockShape.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "Crafting.h"
#include "Mesher.h"

namespace oc {

namespace {
using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
}  // namespace

World::World(int32_t seed) : terrain_(seed), padded_(PAD_VOLUME, AIR), paddedLight_(PAD_VOLUME, 0) {}

World::~World() = default;

Chunk* World::find(int cx, int cz) const {
    auto it = chunks_.find(ChunkKey{cx, cz});
    return it == chunks_.end() ? nullptr : it->second.get();
}

BlockId World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_H) return AIR;
    int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    const Chunk* c = find(cx, cz);
    if (!c) return AIR;
    return c->get(x - cx * CHUNK_W, y, z - cz * CHUNK_W);
}

int World::collisionBoxes(int x, int y, int z, Box* out) const {
    if (y < 0) {
        out[0] = Box{0, 0, 0, 1, 1, 1};
        return 1;
    }
    if (y >= CHUNK_H) return 0;
    const int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    const Chunk* c = find(cx, cz);
    if (!c) {
        out[0] = Box{0, 0, 0, 1, 1, 1};
        return 1;
    }
    const BlockId b = c->get(x - cx * CHUNK_W, y, z - cz * CHUNK_W);
    if (b == AIR) return 0;
    const BlockInfo& bi = blockInfo(b);
    if (bi.shape == SH_FULL) {
        if (!bi.solid) return 0;
        out[0] = Box{0, 0, 0, 1, 1, 1};
        return 1;
    }
    auto get = [this](int gx, int gy, int gz) { return getBlock(gx, gy, gz); };
    return blockBoxes(b, x, y, z, get, false, out);
}

bool World::isSolidAt(int x, int y, int z) const {
    if (y < 0) return true;
    if (y >= CHUNK_H) return false;
    int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    const Chunk* c = find(cx, cz);
    if (!c) return true;
    return isSolid(c->get(x - cx * CHUNK_W, y, z - cz * CHUNK_W));
}

void World::markDirty(int cx, int cz, bool urgent) {
    if (Chunk* c = find(cx, cz)) {
        c->dirty = true;
        if (urgent) c->urgent = true;
    }
}

static uint64_t furnaceKey(int x, int y, int z);

bool World::setBlockImpl(int x, int y, int z, BlockId b, bool urgent) {
    if (y < 0 || y >= CHUNK_H) return false;
    int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    Chunk* c = find(cx, cz);
    if (!c) return false;
    int lx = x - cx * CHUNK_W, lz = z - cz * CHUNK_W;
    const BlockId oldB = c->get(lx, y, lz);
    if (oldB == b) return false;
    c->set(lx, y, lz, b);
    c->modified = true;
    if (isSign(oldB) && !isSign(b)) signs_.erase(furnaceKey(x, y, z));  // der Text verschwindet mit dem Schild
    if (isMachineBlock(b) && !findMachine(x, y, z)) machineAt(x, y, z);  // neue Maschine bekommt ihren Zustand
    if (isCrop(oldB) || isCrop(b)) {
        const int32_t idx = Chunk::index(lx, y, lz);
        if (isCrop(oldB) && !isCrop(b)) c->crops.erase(std::remove(c->crops.begin(), c->crops.end(), idx), c->crops.end());
        else if (!isCrop(oldB) && isCrop(b)) c->crops.push_back(idx);
    }
    lightUrgent_ = urgent;
    relightAfterChange(x, y, z, oldB, b);

    // Betroffene Chunks (inkl. Diagonalen wegen Ambient Occlusion) neu bauen
    int dxs[3] = {0, 0, 0}, dzs[3] = {0, 0, 0};
    int nx = 1, nz = 1;
    if (lx == 0) dxs[nx++] = -1;
    if (lx == CHUNK_W - 1) dxs[nx++] = 1;
    if (lz == 0) dzs[nz++] = -1;
    if (lz == CHUNK_W - 1) dzs[nz++] = 1;
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < nz; ++j) markDirty(cx + dxs[i], cz + dzs[j], urgent);
    return true;
}

bool World::setBlock(int x, int y, int z, BlockId b) {
    if (!setBlockImpl(x, y, z, b, true)) return false;
    scheduleFluidAround(x, y, z);
    checkTorchSupports(x, y, z);
    return true;
}

// ---------------------------------------------------------------------------
// Licht (Flood-Fill wie in Minecraft: Ausbreitung -1 je Block, Entfernen mit Nachfuellen)
// ---------------------------------------------------------------------------

namespace {
const int DIRS6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
}

int World::lightAt(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_H) return 0;
    const int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    const Chunk* c = find(cx, cz);
    return c ? c->light[Chunk::index(x - cx * CHUNK_W, y, z - cz * CHUNK_W)] : 0;
}

void World::setLight(int x, int y, int z, uint8_t v) {
    if (y < 0 || y >= CHUNK_H) return;
    const int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    Chunk* c = find(cx, cz);
    if (!c) return;
    const int lx = x - cx * CHUNK_W, lz = z - cz * CHUNK_W;
    uint8_t& cell = c->light[Chunk::index(lx, y, lz)];
    if (cell == v) return;
    cell = v;
    // geaendertes Licht: Mesh dieses Chunks und der Nachbarn am Rand neu bauen
    for (int dz = (lz == 0 ? -1 : 0); dz <= (lz == CHUNK_W - 1 ? 1 : 0); ++dz)
        for (int dx = (lx == 0 ? -1 : 0); dx <= (lx == CHUNK_W - 1 ? 1 : 0); ++dx) markDirty(cx + dx, cz + dz, lightUrgent_);
}

void World::propagateLight(std::vector<glm::ivec3>& q) {
    for (size_t head = 0; head < q.size(); ++head) {
        const glm::ivec3 p = q[head];
        const int L = lightAt(p.x, p.y, p.z);
        if (L <= 1) continue;
        for (const auto& d : DIRS6) {
            const int nx = p.x + d[0], ny = p.y + d[1], nz = p.z + d[2];
            if (ny < 0 || ny >= CHUNK_H) continue;
            const int cx = floorDiv(nx, CHUNK_W), cz = floorDiv(nz, CHUNK_W);
            const Chunk* nc = find(cx, cz);
            if (!nc) continue;
            if (isOpaque(nc->get(nx - cx * CHUNK_W, ny, nz - cz * CHUNK_W))) continue;
            if (lightAt(nx, ny, nz) < L - 1) {
                setLight(nx, ny, nz, static_cast<uint8_t>(L - 1));
                q.emplace_back(nx, ny, nz);
            }
        }
    }
}

void World::removeLight(int x, int y, int z) {
    const int old = lightAt(x, y, z);
    if (old == 0) return;
    setLight(x, y, z, 0);
    struct Node {
        glm::ivec3 p;
        int level;
    };
    std::vector<Node> rq{{glm::ivec3(x, y, z), old}};
    std::vector<glm::ivec3> readd;
    for (size_t head = 0; head < rq.size(); ++head) {
        const Node n = rq[head];
        for (const auto& d : DIRS6) {
            const int nx = n.p.x + d[0], ny = n.p.y + d[1], nz = n.p.z + d[2];
            if (ny < 0 || ny >= CHUNK_H) continue;
            const int l = lightAt(nx, ny, nz);
            if (l == 0) continue;
            const BlockId nb = getBlock(nx, ny, nz);
            const int e = lightEmission(nb);
            if (l < n.level && e < l) {
                setLight(nx, ny, nz, 0);
                rq.push_back({glm::ivec3(nx, ny, nz), l});
            } else {
                readd.emplace_back(nx, ny, nz);  // staerkere Quelle (oder Lichtquelle): von dort wieder ausbreiten
            }
        }
    }
    propagateLight(readd);
}

void World::relightAfterChange(int x, int y, int z, BlockId oldB, BlockId newB) {
    const bool opaqueChanged = isOpaque(oldB) != isOpaque(newB);
    const int eOld = lightEmission(oldB), eNew = lightEmission(newB);
    if (!opaqueChanged && eOld == eNew) return;
    removeLight(x, y, z);
    std::vector<glm::ivec3> q;
    if (eNew > 0) {
        setLight(x, y, z, static_cast<uint8_t>(eNew));
        q.emplace_back(x, y, z);
    }
    if (!isOpaque(newB)) {  // Licht der Nachbarn kann jetzt hineinfliessen
        for (const auto& d : DIRS6)
            if (lightAt(x + d[0], y + d[1], z + d[2]) > 1) q.emplace_back(x + d[0], y + d[1], z + d[2]);
    }
    propagateLight(q);
}

void World::lightChunkInit(Chunk& c, bool fullScan) {
    std::fill(c.light.begin(), c.light.end(), static_cast<uint8_t>(0));
    std::vector<glm::ivec3> q;
    const int ox = c.cx * CHUNK_W, oz = c.cz * CHUNK_W;
    const int yMax = fullScan ? CHUNK_H - 1 : LAVA_LEVEL + 1;
    for (int y = 0; y <= yMax; ++y)
        for (int z = 0; z < CHUNK_W; ++z)
            for (int x = 0; x < CHUNK_W; ++x) {
                const int e = lightEmission(c.get(x, y, z));
                if (e > 0) {
                    c.light[Chunk::index(x, y, z)] = static_cast<uint8_t>(e);
                    q.emplace_back(ox + x, y, oz + z);
                }
            }
    // Licht der geladenen Nachbarchunks fliesst ueber die gemeinsamen Raender herein
    for (const auto& d : DIRS6) {
        if (d[1] != 0) continue;
        const Chunk* nb = find(c.cx + d[0], c.cz + d[2]);
        if (!nb) continue;
        for (int y = 0; y < CHUNK_H; ++y)
            for (int i = 0; i < CHUNK_W; ++i) {
                // Randspalte des Nachbarn, die an diesen Chunk grenzt
                const int lx = d[0] > 0 ? 0 : (d[0] < 0 ? CHUNK_W - 1 : i);
                const int lz = d[2] > 0 ? 0 : (d[2] < 0 ? CHUNK_W - 1 : i);
                if (nb->light[Chunk::index(lx, y, lz)] > 1) q.emplace_back(nb->cx * CHUNK_W + lx, y, nb->cz * CHUNK_W + lz);
            }
    }
    lightUrgent_ = false;
    propagateLight(q);
}

// Fackeln fallen ab, wenn ihr Traeger fehlt; Pflanzen darueber brechen, wenn ihr Boden fehlt
void World::checkTorchSupports(int x, int y, int z) {
    for (int yy = y + 1; yy < CHUNK_H; ++yy) {
        const BlockId above = getBlock(x, yy, z);
        if (!isPlant(above)) break;
        if (plantCanStandOn(above, getBlock(x, yy - 1, z))) break;
        setBlockImpl(x, yy, z, AIR, true);
        plantDrops_.push_back({glm::ivec3(x, yy, z), above});
    }
    for (const auto& d : DIRS6) {
        const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
        const BlockId b = getBlock(nx, ny, nz);
        if (!isTorch(b)) continue;
        int sx, sy, sz;
        torchSupportOffset(b, sx, sy, sz);
        if (!isOpaque(getBlock(nx + sx, ny + sy, nz + sz))) {
            setBlockImpl(nx, ny, nz, AIR, true);
            torchDrops_.emplace_back(nx, ny, nz);
        }
    }
    // Laternen, Tueren
    for (const auto& d : DIRS6) {
        const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
        const BlockId b = getBlock(nx, ny, nz);
        if (isLantern(b)) {
            const BlockId sup = getBlock(nx, ny + (b == LANTERN ? -1 : 1), nz);
            if (!blockInfo(sup).solid || isLantern(sup)) {
                setBlockImpl(nx, ny, nz, AIR, true);
                plantDrops_.push_back({glm::ivec3(nx, ny, nz), b});
            }
        } else if (isLadder(b)) {
            static const int SUP[4][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
            const int f = ladderFacing(b);
            const BlockId sup = getBlock(nx + SUP[f][0], ny, nz + SUP[f][2]);
            if (!isFullSolidBlock(sup) && !blockInfo(sup).solid) {
                setBlockImpl(nx, ny, nz, AIR, true);
                plantDrops_.push_back({glm::ivec3(nx, ny, nz), b});
            }
        } else if (isSign(b)) {
            static const int BACK[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};  // Wandschild: Traeger gegenueber der Vorderseite
            bool ok;
            if (signIsWall(b)) {
                const int f = signFacing(b);
                const BlockId sup = getBlock(nx + BACK[f][0], ny, nz + BACK[f][1]);
                ok = blockInfo(sup).solid && !isSign(sup);
            } else {
                const BlockId sup = getBlock(nx, ny - 1, nz);
                ok = blockInfo(sup).solid;
            }
            if (!ok) {
                setBlockImpl(nx, ny, nz, AIR, true);
                plantDrops_.push_back({glm::ivec3(nx, ny, nz), b});
            }
        } else if (isBedPart(b)) {
            static const int DIR[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            const int f = bedFacing(b);
            const int sgn = bedIsHead(b) ? -1 : 1;
            const BlockId other = getBlock(nx + sgn * DIR[f][0], ny, nz + sgn * DIR[f][1]);
            if (!isBedPart(other) || bedIsHead(other) == bedIsHead(b) || bedFacing(other) != f) setBlockImpl(nx, ny, nz, AIR, true);  // Haelfte ohne Partner verschwindet
        } else if (isDoor(b)) {
            const bool upper = doorUpper(b);
            const BlockId other = getBlock(nx, ny + (upper ? -1 : 1), nz);
            const bool pairOk = isDoor(other) && doorUpper(other) != upper && doorMaterial(other) == doorMaterial(b);
            const BlockId below = getBlock(nx, ny - 1, nz);
            const bool floorOk = upper || (blockInfo(below).solid && !isDoor(below) && blockInfo(below).shape != SH_LANTERN);
            if (!pairOk || !floorOk) {
                setBlockImpl(nx, ny, nz, AIR, true);
                if (!upper) {
                    plantDrops_.push_back({glm::ivec3(nx, ny, nz), b});
                    if (isDoor(other) && doorUpper(other)) setBlockImpl(nx, ny + 1, nz, AIR, true);  // obere Haelfte mit entfernen
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Fluessigkeiten (Wasser und Lava)
// ---------------------------------------------------------------------------

namespace {
constexpr float FLUID_INTERVAL[2] = {0.25f, 1.5f};
inline bool kindOf(BlockId b, int kind) { return kind == 0 ? isWater(b) : isLava(b); }
inline BlockId sourceOf(int kind) { return kind == 0 ? WATER : LAVA; }
inline BlockId fallOf(int kind) { return kind == 0 ? WATER_FALL : LAVA_FALL; }
inline BlockId firstFlowOf(int kind) { return kind == 0 ? WATER_1 : LAVA_1; }
inline int maxLevelOf(int kind) { return kind == 0 ? MAX_WATER_LEVEL : MAX_LAVA_LEVEL; }
}  // namespace

static uint64_t fluidKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x) & 0xFFFFFFu) << 32) |
           (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0xFFFFFFu) << 8) | static_cast<uint64_t>(y & 0xFF);
}

void World::scheduleFluid(int x, int y, int z) {
    if (y < 0 || y >= CHUNK_H) return;
    for (FluidQueue& q : fluid_)
        if (q.set.insert(fluidKey(x, y, z)).second) q.pending.emplace_back(x, y, z);
}

void World::scheduleFluidAround(int x, int y, int z) {
    static const int D[7][3] = {{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const auto& d : D) scheduleFluid(x + d[0], y + d[1], z + d[2]);
}

BlockId World::blockOrSolid(int x, int y, int z) const {
    if (y < 0) return STONE;
    if (y >= CHUNK_H) return AIR;
    int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    const Chunk* c = find(cx, cz);
    if (!c) return STONE;
    return c->get(x - cx * CHUNK_W, y, z - cz * CHUNK_W);
}

// Gewuenschter Zustand einer Zelle aus ihrer Nachbarschaft (Pull-Modell wie in Minecraft)
BlockId World::computeFluid(int kind, int x, int y, int z) const {
    if (kindOf(blockOrSolid(x, y + 1, z), kind)) return fallOf(kind);

    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int best = 99, sources = 0;
    for (const auto& d : D) {
        const int nx = x + d[0], nz = z + d[1];
        BlockId nb = blockOrSolid(nx, y, nz);
        if (!kindOf(nb, kind)) continue;
        int lvl;
        if (nb == sourceOf(kind)) {
            lvl = 1;
            ++sources;
        } else {
            // Stroemung und fallende Fluessigkeit breiten sich nur seitlich aus, wenn darunter nichts zum Fallen ist
            if (blockOrSolid(nx, y - 1, nz) == AIR) continue;
            lvl = nb == fallOf(kind) ? 1 : flowLevel(nb) + 1;
        }
        best = std::min(best, lvl);
    }
    if (best > maxLevelOf(kind)) return AIR;
    BlockId below = blockOrSolid(x, y - 1, z);
    // Wasser: zwei Quellen ergeben eine neue (Lava nicht)
    if (kind == 0 && sources >= 2 && (isSolid(below) || below == WATER)) return WATER;
    return static_cast<BlockId>(firstFlowOf(kind) + best - 1);
}

// Wasser neben Lava: Lavaquelle wird Obsidian, fliessende Lava Bruchstein
void World::reactFluids(int x, int y, int z) {
    static const int D[7][3] = {{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const auto& d : D) {
        const int cx = x + d[0], cy = y + d[1], cz = z + d[2];
        BlockId b = getBlock(cx, cy, cz);
        if (!isLava(b)) continue;
        bool wet = false;
        for (const auto& e : D) {
            if (e[0] == 0 && e[1] == 0 && e[2] == 0) continue;
            if (isWater(getBlock(cx + e[0], cy + e[1], cz + e[2]))) {
                wet = true;
                break;
            }
        }
        if (!wet) continue;
        setBlockImpl(cx, cy, cz, b == LAVA ? OBSIDIAN : COBBLE, false);
        scheduleFluidAround(cx, cy, cz);
    }
}

void World::updateFluidCell(int kind, int x, int y, int z) {
    int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    // nur wenn die Nachbarschaft geladen ist (sonst fliesst es in ungeladene Chunks)
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (!find(cx + dx, cz + dz)) return;

    reactFluids(x, y, z);

    BlockId cur = getBlock(x, y, z);
    if (cur != AIR && !(kindOf(cur, kind) && cur != sourceOf(kind))) return;  // fest, andere Fluessigkeit oder Quelle
    BlockId next = computeFluid(kind, x, y, z);
    if (next == cur) return;
    setBlockImpl(x, y, z, next, false);
    scheduleFluidAround(x, y, z);
    reactFluids(x, y, z);
}

// Tuer oder Tor samt allen verbundenen Nachbarn (gleiche Art, gleicher Zustand) umschalten. Rueckgabe: Anzahl umgeschalteter Bloecke.
int World::toggleOpenGroup(int x, int y, int z) {
    const BlockId start = getBlock(x, y, z);
    if (!isDoor(start) && !isGate(start)) return 0;
    auto same = [&](BlockId a, BlockId b) {
        if (isDoor(a) && isDoor(b))
            return doorMaterial(a) == doorMaterial(b) && doorFacing(a) == doorFacing(b) && doorOpen(a) == doorOpen(b);
        if (isGate(a) && isGate(b)) return gateMaterial(a) == gateMaterial(b) && gateAxis(a) == gateAxis(b) && gateOpen(a) == gateOpen(b);
        return false;
    };
    std::vector<glm::ivec3> todo{{x, y, z}}, done;
    auto contains = [&](const glm::ivec3& p) {
        for (const glm::ivec3& q : done)
            if (q == p) return true;
        for (const glm::ivec3& q : todo)
            if (q == p) return true;
        return false;
    };
    for (size_t i = 0; i < todo.size() && done.size() + todo.size() < 160; ++i) {
        const glm::ivec3 p = todo[i];
        done.push_back(p);
        for (const auto& d : DIRS6) {
            const glm::ivec3 q(p.x + d[0], p.y + d[1], p.z + d[2]);
            if (contains(q)) continue;
            if (same(start, getBlock(q.x, q.y, q.z))) todo.push_back(q);
        }
    }
    // todo enthaelt alles (done wird parallel gefuellt); gemeinsam umschalten
    int n = 0;
    for (const glm::ivec3& p : todo) {
        const BlockId b = getBlock(p.x, p.y, p.z);
        BlockId nb = b;
        if (isDoor(b)) nb = makeDoor(doorMaterial(b), doorUpper(b), !doorOpen(b), doorFacing(b));
        else if (isGate(b)) nb = makeGate(gateMaterial(b), !gateOpen(b), gateAxis(b));
        if (nb != b && setBlock(p.x, p.y, p.z, nb)) ++n;
    }
    return n;
}

void World::tickCrops(float dt) {
    cropTimer_ += dt;
    if (cropTimer_ < 1.0f) return;
    cropTimer_ -= 1.0f;
    for (auto& kv : chunks_) {
        Chunk& c = *kv.second;
        if (c.crops.empty()) continue;
        const std::vector<int32_t> list = c.crops;  // Kopie, weil das Wachsen die Liste nicht aendert, setBlock aber koennte
        for (int32_t idx : list) {
            cropRng_ ^= cropRng_ << 13;
            cropRng_ ^= cropRng_ >> 17;
            cropRng_ ^= cropRng_ << 5;
            if (cropRng_ % 45u != 0u) continue;
            const int lx = idx % CHUNK_W, lz = (idx / CHUNK_W) % CHUNK_W, y = idx / (CHUNK_W * CHUNK_W);
            const BlockId b = c.get(lx, y, lz);
            if (!isCrop(b) || cropFullyGrown(b)) continue;
            const int x = c.cx * CHUNK_W + lx, z = c.cz * CHUNK_W + lz;
            // Ohne Wasser in der Naehe waechst es nur halb so oft
            bool wet = false;
            for (int dz = -4; dz <= 4 && !wet; ++dz)
                for (int dx = -4; dx <= 4 && !wet; ++dx)
                    for (int dy = -1; dy <= 0 && !wet; ++dy) wet = isWater(getBlock(x + dx, y - 1 + dy, z + dz));
            if (!wet && (cropRng_ >> 8) % 2u == 0u) continue;
            setBlockImpl(x, y, z, static_cast<BlockId>(b + 1), false);
        }
    }
}

void World::tick(float dt) {
    tickFurnaces(dt);
    tickMachines(dt);
    tickCrops(dt);
    for (int kind = 0; kind < 2; ++kind) {
        FluidQueue& q = fluid_[kind];
        q.accum += dt;
        int steps = 0;
        while (q.accum >= FLUID_INTERVAL[kind] && steps < 3) {
            q.accum -= FLUID_INTERVAL[kind];
            ++steps;
            std::vector<glm::ivec3> cur;
            cur.swap(q.pending);
            q.set.clear();
            const size_t limit = 6000;
            for (size_t i = 0; i < cur.size(); ++i) {
                if (i >= limit) {  // Rest im naechsten Schritt
                    scheduleFluid(cur[i].x, cur[i].y, cur[i].z);
                    continue;
                }
                updateFluidCell(kind, cur[i].x, cur[i].y, cur[i].z);
            }
        }
        if (steps == 3) q.accum = 0.0f;  // nicht aufholen, wenn das Spiel stark hakte
    }
}

bool World::inFluidAt(const glm::vec3& p, bool lava) const {
    int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y)), z = static_cast<int>(std::floor(p.z));
    BlockId b = getBlock(x, y, z);
    if (lava ? !isLava(b) : !isWater(b)) return false;
    float top = isLiquid(getBlock(x, y + 1, z)) ? 1.0f : liquidHeight(b);
    return p.y - static_cast<float>(y) < top;
}

bool World::inWaterAt(const glm::vec3& p) const { return inFluidAt(p, false); }
bool World::inLavaAt(const glm::vec3& p) const { return inFluidAt(p, true); }

Chunk& World::ensureChunk(int cx, int cz) {
    if (Chunk* c = find(cx, cz)) return *c;
    auto chunk = std::make_unique<Chunk>(cx, cz);
    auto st = stored_.find(ChunkKey{cx, cz});
    if (st != stored_.end()) {
        chunk->blocks = std::move(st->second);
        chunk->modified = true;
        stored_.erase(st);
    } else {
        terrain_.generate(*chunk);
    }
    Chunk& ref = *chunk;
    for (int i = 0; i < Chunk::VOLUME; ++i)
        if (isCrop(ref.blocks[static_cast<size_t>(i)])) ref.crops.push_back(i);
    const bool restored = ref.modified;
    chunks_.emplace(ChunkKey{cx, cz}, std::move(chunk));
    lightChunkInit(ref, restored);
    // Nachbarn muessen ihre Raender ggf. neu berechnen (sie warten ohnehin auf vollstaendige Nachbarschaft)
    return ref;
}

bool World::neighborsLoaded(int cx, int cz) const {
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (!find(cx + dx, cz + dz)) return false;
    return true;
}

void World::meshChunk(Chunk& c) {
    const Chunk* nb[3][3];
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) nb[dz + 1][dx + 1] = find(c.cx + dx, c.cz + dz);

    for (int y = 0; y < CHUNK_H; ++y) {
        for (int pz = 0; pz < PAD_W; ++pz) {
            int lz = pz - 1;
            int dz = lz < 0 ? -1 : (lz >= CHUNK_W ? 1 : 0);
            lz -= dz * CHUNK_W;
            for (int px = 0; px < PAD_W; ++px) {
                int lx = px - 1;
                int dx = lx < 0 ? -1 : (lx >= CHUNK_W ? 1 : 0);
                lx -= dx * CHUNK_W;
                const Chunk* src = nb[dz + 1][dx + 1];
                padded_[padIndex(px, y, pz)] = src ? src->get(lx, y, lz) : static_cast<BlockId>(AIR);
                paddedLight_[padIndex(px, y, pz)] = src ? src->light[Chunk::index(lx, y, lz)] : static_cast<uint8_t>(0);
            }
        }
    }

    static MeshOutput out;
    buildChunkMesh(padded_.data(), paddedLight_.data(), out);
    c.opaque.upload(out.opaque);
    c.water.upload(out.water);
    c.dirty = false;
    c.urgent = false;
    c.meshed = true;
}

size_t World::pendingMeshCount() const {
    size_t n = 0;
    for (auto& kv : chunks_)
        if (kv.second->dirty) ++n;
    return n;
}

void World::update(const glm::vec3& center, int R, double budgetMs) {
    const auto t0 = Clock::now();
    const int pcx = floorDiv(static_cast<int>(std::floor(center.x)), CHUNK_W);
    const int pcz = floorDiv(static_cast<int>(std::floor(center.z)), CHUNK_W);

    // 1. Spieleraktionen sofort umsetzen (kein Budget)
    if (meshing_) {
        for (auto& kv : chunks_) {
            Chunk& c = *kv.second;
            if (c.urgent && c.dirty && neighborsLoaded(c.cx, c.cz)) meshChunk(c);
            else if (c.urgent && !c.dirty) c.urgent = false;
        }
    }

    // 2. Weit entfernte Chunks entladen
    {
        const int unloadR = R + 3;
        std::vector<ChunkKey> remove;
        for (auto& kv : chunks_) {
            int dx = kv.first.cx - pcx, dz = kv.first.cz - pcz;
            if (dx * dx + dz * dz > unloadR * unloadR) remove.push_back(kv.first);
        }
        for (const ChunkKey& k : remove) {
            auto it = chunks_.find(k);
            if (it->second->modified) stored_[k] = std::move(it->second->blocks);
            chunks_.erase(it);
        }
    }

    // 3. Fehlende Chunks erzeugen (nahe zuerst)
    {
        const int genR = R + 2;
        struct Cand {
            int d2, cx, cz;
        };
        std::vector<Cand> missing;
        for (int dz = -genR; dz <= genR; ++dz)
            for (int dx = -genR; dx <= genR; ++dx) {
                int d2 = dx * dx + dz * dz;
                if (d2 > genR * genR) continue;
                if (!find(pcx + dx, pcz + dz)) missing.push_back({d2, pcx + dx, pcz + dz});
            }
        std::sort(missing.begin(), missing.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
        for (const Cand& m : missing) {
            ensureChunk(m.cx, m.cz);
            // Nachbarn eines frisch erzeugten Chunks, die schon gemeshed waren, haben fehlende Randdaten gehabt
            // (sie werden erst gemeshed, wenn alle Nachbarn existieren), daher ist hier nichts zu invalidieren.
            if (msSince(t0) > budgetMs * 0.5) break;
        }
    }

    // 4. Meshes bauen (nahe zuerst)
    if (meshing_) {
        struct Cand {
            int d2;
            Chunk* c;
        };
        std::vector<Cand> todo;
        for (auto& kv : chunks_) {
            Chunk& c = *kv.second;
            if (!c.dirty) continue;
            int dx = c.cx - pcx, dz = c.cz - pcz;
            int d2 = dx * dx + dz * dz;
            if (d2 > R * R) continue;
            if (!neighborsLoaded(c.cx, c.cz)) continue;
            todo.push_back({d2, &c});
        }
        std::sort(todo.begin(), todo.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
        for (const Cand& t : todo) {
            meshChunk(*t.c);
            if (msSince(t0) > budgetMs) break;
        }
    }
}

void World::loadAround(const glm::vec3& center, int radius) {
    const int pcx = floorDiv(static_cast<int>(std::floor(center.x)), CHUNK_W);
    const int pcz = floorDiv(static_cast<int>(std::floor(center.z)), CHUNK_W);
    for (int dz = -radius - 1; dz <= radius + 1; ++dz)
        for (int dx = -radius - 1; dx <= radius + 1; ++dx) ensureChunk(pcx + dx, pcz + dz);
    if (!meshing_) return;
    for (int dz = -radius; dz <= radius; ++dz)
        for (int dx = -radius; dx <= radius; ++dx) {
            Chunk* c = find(pcx + dx, pcz + dz);
            if (c && c->dirty) meshChunk(*c);
        }
}

glm::vec3 World::findSpawn() {
    for (int ring = 0; ring < 400; ++ring) {
        const int r = ring * 6;
        for (int i = -ring; i <= ring; ++i) {
            for (int s = 0; s < 4; ++s) {
                int x, z;
                switch (s) {
                    case 0: x = i * 6; z = -r; break;
                    case 1: x = i * 6; z = r; break;
                    case 2: x = -r; z = i * 6; break;
                    default: x = r; z = i * 6; break;
                }
                if (ring == 0 && s > 0) continue;
                TerrainGenerator::Column col = terrain_.column(x, z);
                if (col.height <= SEA_LEVEL + 1 || col.height > SEA_LEVEL + 30) continue;
                int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
                for (int dz = -1; dz <= 1; ++dz)
                    for (int dx = -1; dx <= 1; ++dx) ensureChunk(cx + dx, cz + dz);
                // oberster fester Block
                int y = CHUNK_H - 1;
                while (y > 0 && getBlock(x, y, z) == AIR) --y;
                BlockId top = getBlock(x, y, z);
                if (top == GRASS || top == SAND || top == SNOW || top == DIRT) {
                    // freier Platz um den Startpunkt (kein Baum, kein Dach, kein Wasser/Haenge direkt daneben)
                    bool open = true;
                    for (int dz = -3; dz <= 3 && open; ++dz)
                        for (int dx = -3; dx <= 3 && open; ++dx)
                            for (int yy = y + 1; yy <= y + 10 && open; ++yy)
                                open = getBlock(x + dx, yy, z + dz) == AIR;
                    if (open)
                        return glm::vec3(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 1.0f,
                                         static_cast<float>(z) + 0.5f);
                }
            }
        }
    }
    return glm::vec3(0.5f, static_cast<float>(SEA_LEVEL + 10), 0.5f);
}

RayHit World::raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist, bool liquids) const {
    RayHit result;
    glm::vec3 d = glm::normalize(dir);
    glm::ivec3 cell(static_cast<int>(std::floor(origin.x)), static_cast<int>(std::floor(origin.y)),
                    static_cast<int>(std::floor(origin.z)));
    glm::ivec3 step(d.x > 0 ? 1 : -1, d.y > 0 ? 1 : -1, d.z > 0 ? 1 : -1);
    glm::vec3 tDelta, tMax;
    for (int a = 0; a < 3; ++a) {
        if (d[a] == 0.0f) {
            tDelta[a] = 1e30f;
            tMax[a] = 1e30f;
        } else {
            tDelta[a] = std::fabs(1.0f / d[a]);
            float nextBoundary = static_cast<float>(cell[a] + (step[a] > 0 ? 1 : 0));
            tMax[a] = (nextBoundary - origin[a]) / d[a];
        }
    }
    glm::ivec3 normal(0);
    float t = 0.0f, entry = 0.0f;
    for (int i = 0; i < 256; ++i) {
        BlockId b = getBlock(cell.x, cell.y, cell.z);
        if (b != AIR && (liquids || !blockInfo(b).liquid)) {
            if (blockInfo(b).shape == SH_FULL) {
                result.hit = true;
                result.block = cell;
                result.normal = normal;
                result.dist = entry;
                return result;
            }
            // Teilform: nur treffen, wenn der Strahl eine ihrer Boxen schneidet
            Box boxes[MAX_BOXES];
            auto get = [this](int gx, int gy, int gz) { return getBlock(gx, gy, gz); };
            const int nb = blockBoxes(b, cell.x, cell.y, cell.z, get, true, boxes);
            float bestT = 1e30f;
            glm::ivec3 bestN(0);
            for (int i = 0; i < nb; ++i) {
                const float lo[3] = {cell.x + boxes[i].x0, cell.y + boxes[i].y0, cell.z + boxes[i].z0};
                const float hi[3] = {cell.x + boxes[i].x1, cell.y + boxes[i].y1, cell.z + boxes[i].z1};
                float t0 = 0.0f, t1 = maxDist;
                int axisIn = -1, signIn = 0;
                bool miss = false;
                for (int a = 0; a < 3 && !miss; ++a) {
                    if (std::fabs(d[a]) < 1e-9f) {
                        if (origin[a] < lo[a] || origin[a] > hi[a]) miss = true;
                        continue;
                    }
                    float ta = (lo[a] - origin[a]) / d[a], tb = (hi[a] - origin[a]) / d[a];
                    int sgn = -1;  // Eintritt durch die Seite lo (Normale -)
                    if (ta > tb) {
                        std::swap(ta, tb);
                        sgn = 1;
                    }
                    if (ta > t0) {
                        t0 = ta;
                        axisIn = a;
                        signIn = sgn;
                    }
                    t1 = std::min(t1, tb);
                    if (t0 > t1) miss = true;
                }
                if (miss || t0 > maxDist) continue;
                if (t0 < bestT) {
                    bestT = t0;
                    bestN = glm::ivec3(0);
                    if (axisIn >= 0) bestN[axisIn] = signIn;
                    else bestN = normal;  // Start im Inneren
                }
            }
            if (bestT < 1e29f) {
                result.hit = true;
                result.block = cell;
                result.normal = bestN;
                result.dist = bestT;
                return result;
            }
        }
        int a = 0;
        if (tMax.y < tMax.x) a = 1;
        if (tMax.z < tMax[a]) a = 2;
        t = tMax[a];
        if (t > maxDist) break;
        entry = t;
        cell[a] += step[a];
        tMax[a] += tDelta[a];
        normal = glm::ivec3(0);
        normal[a] = -step[a];
    }
    return result;
}

void World::exportModified(std::vector<SavedChunk>& out) const {
    for (auto& kv : chunks_) {
        if (!kv.second->modified) continue;
        out.push_back({kv.first.cx, kv.first.cz, rleEncode(kv.second->blocks)});
    }
    for (auto& kv : stored_) out.push_back({kv.first.cx, kv.first.cz, rleEncode(kv.second)});
}

void World::importChunks(const std::vector<SavedChunk>& in) {
    for (const SavedChunk& sc : in) {
        std::vector<BlockId> blocks;
        if (!rleDecode(sc.rle, blocks, static_cast<size_t>(Chunk::VOLUME))) continue;
        stored_[ChunkKey{sc.cx, sc.cz}] = std::move(blocks);
    }
}

}  // namespace oc

// ---------------------------------------------------------------------------
// Oefen
// ---------------------------------------------------------------------------

namespace oc {

static uint64_t furnaceKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x) & 0xFFFFFFu) << 32) |
           (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0xFFFFFFu) << 8) | static_cast<uint64_t>(y & 0xFF);
}

Furnace& World::furnaceAt(int x, int y, int z) {
    auto it = furnaces_.find(furnaceKey(x, y, z));
    if (it == furnaces_.end()) it = furnaces_.emplace(furnaceKey(x, y, z), FurnaceEntry{glm::ivec3(x, y, z), Furnace()}).first;
    return it->second.f;
}

Furnace* World::findFurnace(int x, int y, int z) {
    auto it = furnaces_.find(furnaceKey(x, y, z));
    return it == furnaces_.end() ? nullptr : &it->second.f;
}

bool World::removeFurnace(int x, int y, int z, Furnace* out) {
    auto it = furnaces_.find(furnaceKey(x, y, z));
    if (it == furnaces_.end()) return false;
    if (out) *out = it->second.f;
    furnaces_.erase(it);
    return true;
}

void World::tickFurnaces(float dt) {
    std::vector<uint64_t> orphans;
    for (auto& kv : furnaces_) {
        FurnaceEntry& e = kv.second;
        const glm::ivec3 p = e.pos;
        const int cx = floorDiv(p.x, CHUNK_W), cz = floorDiv(p.z, CHUNK_W);
        if (!find(cx, cz)) continue;  // Chunk nicht geladen: Ofen ruht
        const BlockId b = getBlock(p.x, p.y, p.z);
        if (b != FURNACE && b != FURNACE_LIT) {
            orphans.push_back(kv.first);
            continue;
        }
        Furnace& f = e.f;
        const SmeltRecipe* r = f.input.empty() ? nullptr : smeltRecipe(f.input.id);
        const bool canSmelt =
            r && (f.output.empty() || (f.output.id == r->output.id && f.output.damage == 0 &&
                                       f.output.count + r->output.count <= itemInfo(r->output.id).maxStack));
        if (f.burnTime > 0.0f) f.burnTime = std::max(0.0f, f.burnTime - dt);
        if (f.burnTime <= 0.0f && canSmelt && !f.fuel.empty() && fuelSeconds(f.fuel.id) > 0.0f) {
            f.burnTime = f.burnTotal = fuelSeconds(f.fuel.id);
            if (--f.fuel.count <= 0) f.fuel.clear();
        }
        if (f.burnTime > 0.0f && canSmelt) {
            f.cookTime += dt;
            if (f.cookTime >= SMELT_SECONDS) {
                f.cookTime -= SMELT_SECONDS;
                if (--f.input.count <= 0) f.input.clear();
                if (f.output.empty()) f.output = r->output;
                else f.output.count += r->output.count;
            }
        } else if (f.cookTime > 0.0f) {
            f.cookTime = std::max(0.0f, f.cookTime - 2.0f * dt);
        }
        const BlockId want = f.lit() ? FURNACE_LIT : FURNACE;
        if (b != want) setBlockImpl(p.x, p.y, p.z, want, false);
    }
    for (uint64_t k : orphans) furnaces_.erase(k);
}

void World::exportFurnaces(std::vector<FurnaceSave>& out) const {
    for (const auto& kv : furnaces_) {
        const FurnaceEntry& e = kv.second;
        FurnaceSave fs;
        fs.x = e.pos.x;
        fs.y = e.pos.y;
        fs.z = e.pos.z;
        fs.furnace = e.f;
        out.push_back(fs);
    }
}

const SignText* World::findSign(int x, int y, int z) const {
    auto it = signs_.find(furnaceKey(x, y, z));
    return it == signs_.end() ? nullptr : &it->second.text;
}

void World::setSign(int x, int y, int z, const SignText& t) {
    if (t.empty()) {
        signs_.erase(furnaceKey(x, y, z));
        return;
    }
    if (!isSign(getBlock(x, y, z))) return;
    signs_[furnaceKey(x, y, z)] = SignEntry{glm::ivec3(x, y, z), t};
}

void World::exportSigns(std::vector<SignSave>& out) const {
    for (const auto& kv : signs_) {
        SignSave s;
        s.x = kv.second.pos.x;
        s.y = kv.second.pos.y;
        s.z = kv.second.pos.z;
        s.text = kv.second.text;
        out.push_back(s);
    }
}

void World::importSigns(const std::vector<SignSave>& in) {
    for (const SignSave& s : in)
        if (!s.text.empty()) signs_[furnaceKey(s.x, s.y, s.z)] = SignEntry{glm::ivec3(s.x, s.y, s.z), s.text};
}

Chest& World::chestAt(int x, int y, int z) {
    auto it = chests_.find(furnaceKey(x, y, z));
    if (it == chests_.end()) {
        Chest fresh;
        // Truhen in Doerfern enthalten beim ersten Oeffnen Beute
        const VillageGenerator& vg = terrain_.villages();
        if (vg.isVillageChest(x, y, z))
            for (const Village* v : vg.villagesIn(x, z, x, z))
                for (const VillageGenerator::Loot& l : vg.lootFor(*v, glm::ivec3(x, y, z))) fresh.add(l.item, l.count);
        it = chests_.emplace(furnaceKey(x, y, z), ChestEntry{glm::ivec3(x, y, z), fresh}).first;
    }
    return it->second.c;
}

Chest* World::findChest(int x, int y, int z) {
    auto it = chests_.find(furnaceKey(x, y, z));
    return it == chests_.end() ? nullptr : &it->second.c;
}

bool World::removeChest(int x, int y, int z, Chest* out) {
    auto it = chests_.find(furnaceKey(x, y, z));
    if (it == chests_.end()) return false;
    if (out) *out = it->second.c;
    chests_.erase(it);
    return true;
}

void World::exportChests(std::vector<ChestSave>& out) const {
    for (const auto& kv : chests_) {
        ChestSave cs;
        cs.x = kv.second.pos.x;
        cs.y = kv.second.pos.y;
        cs.z = kv.second.pos.z;
        cs.chest = kv.second.c;
        // Leere Dorftruhen bleiben gespeichert, sonst fuellten sie sich beim naechsten Oeffnen neu
        if (!cs.chest.isEmpty() || terrain_.villages().isVillageChest(cs.x, cs.y, cs.z)) out.push_back(cs);
    }
}

void World::importChests(const std::vector<ChestSave>& in) {
    for (const ChestSave& cs : in) chests_[furnaceKey(cs.x, cs.y, cs.z)] = ChestEntry{glm::ivec3(cs.x, cs.y, cs.z), cs.chest};
}

void World::importFurnaces(const std::vector<FurnaceSave>& in) {
    for (const FurnaceSave& fs : in) furnaces_[furnaceKey(fs.x, fs.y, fs.z)] = FurnaceEntry{glm::ivec3(fs.x, fs.y, fs.z), fs.furnace};
}

}  // namespace oc
