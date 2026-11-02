#include "World.h"

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

World::World(int32_t seed) : terrain_(seed), padded_(PAD_VOLUME, AIR) {}

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

bool World::setBlockImpl(int x, int y, int z, BlockId b, bool urgent) {
    if (y < 0 || y >= CHUNK_H) return false;
    int cx = floorDiv(x, CHUNK_W), cz = floorDiv(z, CHUNK_W);
    Chunk* c = find(cx, cz);
    if (!c) return false;
    int lx = x - cx * CHUNK_W, lz = z - cz * CHUNK_W;
    if (c->get(lx, y, lz) == b) return false;
    c->set(lx, y, lz, b);
    c->modified = true;

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
    return true;
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

void World::tick(float dt) {
    tickFurnaces(dt);
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
    chunks_.emplace(ChunkKey{cx, cz}, std::move(chunk));
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
            }
        }
    }

    static MeshOutput out;
    buildChunkMesh(padded_.data(), out);
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

RayHit World::raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist) const {
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
    float t = 0.0f;
    for (int i = 0; i < 256; ++i) {
        BlockId b = getBlock(cell.x, cell.y, cell.z);
        if (b != AIR && !blockInfo(b).liquid) {
            result.hit = true;
            result.block = cell;
            result.normal = normal;
            return result;
        }
        int a = 0;
        if (tMax.y < tMax.x) a = 1;
        if (tMax.z < tMax[a]) a = 2;
        t = tMax[a];
        if (t > maxDist) break;
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

void World::importFurnaces(const std::vector<FurnaceSave>& in) {
    for (const FurnaceSave& fs : in) furnaces_[furnaceKey(fs.x, fs.y, fs.z)] = FurnaceEntry{glm::ivec3(fs.x, fs.y, fs.z), fs.furnace};
}

}  // namespace oc
