#include "Machines.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "Crafting.h"
#include "World.h"

namespace oc {

namespace {

// Schluessel wie bei Oefen und Truhen (gleiche Formel wie in World.cpp)
uint64_t machineKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x) & 0xFFFFFFu) << 32) |
           (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0xFFFFFFu) << 8) | static_cast<uint64_t>(y & 0xFF);
}

const int DIR6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}};  // wie die Bohrerrichtungen 0..5

}  // namespace

MachineKind machineKindOf(BlockId id) {
    if (isBoiler(id)) return MachineKind::Boiler;
    if (id == STILL) return MachineKind::Still;
    if (isDrill(id)) return MachineKind::Drill;
    return MachineKind::Motor;
}

float boilerFuelSeconds(ItemId item) {
    if (item == ITEM_TAR) return 160.0f;
    if (item == ITEM_FUEL_OIL) return 360.0f;
    return fuelSeconds(item) * 1.5f;
}

const StillRecipe* stillRecipe(ItemId input) {
    static const StillRecipe RECIPES[] = {
        {ITEM_COAL, {ITEM_TAR, 1}, {}, 6.0f},
        {LOG, {ITEM_TAR, 1}, {}, 6.0f},
        {BIRCH_LOG, {ITEM_TAR, 1}, {}, 6.0f},
        {SPRUCE_LOG, {ITEM_TAR, 1}, {}, 6.0f},
        {ITEM_TAR, {ITEM_FUEL_OIL, 1}, {}, 8.0f},
        {ITEM_BUCKET_WATER, {ITEM_BUCKET_DISTILLED, 1}, {ITEM_SALT, 1}, 10.0f},
    };
    for (const StillRecipe& r : RECIPES)
        if (r.input == input) return &r;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------------------------------------------

Machine* World::findMachine(int x, int y, int z) {
    auto it = machines_.find(machineKey(x, y, z));
    return it == machines_.end() ? nullptr : &it->second.m;
}

Machine& World::machineAt(int x, int y, int z) {
    auto it = machines_.find(machineKey(x, y, z));
    if (it == machines_.end()) {
        Machine m;
        m.kind = machineKindOf(getBlock(x, y, z));
        it = machines_.emplace(machineKey(x, y, z), MachineEntry{glm::ivec3(x, y, z), m}).first;
    }
    return it->second.m;
}

bool World::removeMachine(int x, int y, int z, Machine* out) {
    auto it = machines_.find(machineKey(x, y, z));
    if (it == machines_.end()) return false;
    if (out) *out = it->second.m;
    machines_.erase(it);
    return true;
}

float World::powerAt(int x, int y, int z) const {
    auto it = power_.find(machineKey(x, y, z));
    return it == power_.end() ? 0.0f : it->second;
}

World::MechFlow World::mechFlow(const glm::vec3& feet) const {
    MechFlow f;
    const int x = static_cast<int>(std::floor(feet.x)), z = static_cast<int>(std::floor(feet.z));
    const int by = static_cast<int>(std::floor(feet.y - 0.1f));
    const BlockId below = getBlock(x, by, z);
    if (isConveyor(below) && feet.y - static_cast<float>(by) < 0.6f) {
        static const float DX[4] = {1.0f, -1.0f, 0.0f, 0.0f}, DZ[4] = {0.0f, 0.0f, 1.0f, -1.0f};
        const int fc = conveyorFacing(below);
        const float p = powerAt(x, by, z);
        f.belt = glm::vec3(DX[fc] * BELT_SPEED * p, 0.0f, DZ[fc] * BELT_SPEED * p);
    }
    for (const float dy : {0.1f, 0.9f}) {
        const int y = static_cast<int>(std::floor(feet.y + dy));
        if (getBlock(x, y, z) != LIFT) continue;
        f.inLift = true;
        f.lift = std::max(f.lift, LIFT_SPEED * std::min(1.0f, powerAt(x, y, z)));
    }
    return f;
}

void World::exportMachines(std::vector<MachineSave>& out) const {
    for (const auto& kv : machines_) {
        MachineSave ms;
        ms.x = kv.second.pos.x;
        ms.y = kv.second.pos.y;
        ms.z = kv.second.pos.z;
        ms.m = kv.second.m;
        out.push_back(ms);
    }
}

void World::importMachines(const std::vector<MachineSave>& in) {
    for (const MachineSave& ms : in) machines_[machineKey(ms.x, ms.y, ms.z)] = MachineEntry{glm::ivec3(ms.x, ms.y, ms.z), ms.m};
}

// ---------------------------------------------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------------------------------------------

bool World::drillTarget(const glm::ivec3& pos, int facing, glm::ivec3& out) const {
    if (facing < 0 || facing > 5) return false;
    for (int d = 1; d <= DRILL_RANGE; ++d) {
        const glm::ivec3 p(pos.x + DIR6[facing][0] * d, pos.y + DIR6[facing][1] * d, pos.z + DIR6[facing][2] * d);
        if (p.y < 1 || p.y >= CHUNK_H) return false;
        if (!isChunkLoaded(floorDiv(p.x, CHUNK_W), floorDiv(p.z, CHUNK_W))) return false;
        const BlockId b = getBlock(p.x, p.y, p.z);
        if (b == AIR || isLiquid(b)) continue;
        const BlockInfo& bi = blockInfo(b);
        if (!bi.solid) continue;  // Pflanzen, Fackeln, Schilder und Co. stehen nicht im Weg
        // Dinge mit eigenem Inhalt, Maschinenteile und Unzerstoerbares halten den Bohrer an
        if (!bi.breakable || bi.minTier > 3 || isSteamPart(b) || isMechPart(b) || b == CHEST || b == FURNACE || b == FURNACE_LIT) return false;
        out = p;
        return true;
    }
    return false;
}

void World::tickMachines(float dt) {
    if (machines_.empty()) {
        if (!power_.empty()) power_.clear();
        machineAccum_ = 0.0f;
        return;
    }
    machineAccum_ += dt;
    const float STEP = 0.1f;
    int steps = 0;
    while (machineAccum_ >= STEP && steps < 5) {
        machineAccum_ -= STEP;
        ++steps;
        stepMachines(STEP);
    }
    if (steps == 5) machineAccum_ = 0.0f;
}

namespace {

struct Comp {
    float demand = 0.0f;   // Bedarf (mechanisch bzw. Dampf)
    float supply = 0.0f;   // Angebot (mechanische Leistung bzw. Dampf)
    int producers = 0;     // Motoren bzw. brennende Kessel
    float util = 0.0f;     // mechanisch: Auslastung der Motoren 0..1
    float sat = 1.0f;      // Versorgung 0..1
};

}  // namespace

void World::stepMachines(float dt) {
    // Maschinen im geladenen Bereich einsammeln, verwaiste (Block weg) entfernen
    std::vector<MachineEntry*> list;
    std::vector<uint64_t> orphans;
    for (auto& kv : machines_) {
        MachineEntry& e = kv.second;
        if (!isChunkLoaded(floorDiv(e.pos.x, CHUNK_W), floorDiv(e.pos.z, CHUNK_W))) continue;  // Chunk nicht geladen: Maschine ruht
        const BlockId b = getBlock(e.pos.x, e.pos.y, e.pos.z);
        if (!isMachineBlock(b) || machineKindOf(b) != e.m.kind) {
            orphans.push_back(kv.first);
            continue;
        }
        list.push_back(&e);
    }
    for (uint64_t k : orphans) machines_.erase(k);
    power_.clear();

    // Netze: Zusammenhangskomponenten ueber Beruehrung, einmal fuer Dampf (Rohre) und einmal fuer Antrieb (Wellen)
    auto flood = [&](const glm::ivec3& start, bool (*part)(BlockId), std::unordered_map<uint64_t, int>& compOf, int id, std::vector<glm::ivec3>& cells) {
        std::vector<glm::ivec3> stack{start};
        compOf[machineKey(start.x, start.y, start.z)] = id;
        while (!stack.empty() && cells.size() < 4096) {
            const glm::ivec3 p = stack.back();
            stack.pop_back();
            cells.push_back(p);
            for (const auto& d : DIR6) {
                const glm::ivec3 q(p.x + d[0], p.y + d[1], p.z + d[2]);
                if (q.y < 0 || q.y >= CHUNK_H) continue;
                const uint64_t k = machineKey(q.x, q.y, q.z);
                if (compOf.count(k)) continue;
                if (!part(getBlock(q.x, q.y, q.z))) continue;
                compOf[k] = id;
                stack.push_back(q);
            }
        }
    };
    auto isMech = [](BlockId b) { return isMechPart(b); };
    auto isSteam = [](BlockId b) { return isSteamPart(b); };

    std::unordered_map<uint64_t, int> mechOf, steamOf;
    std::vector<Comp> mech, steam;
    std::vector<std::vector<glm::ivec3>> mechCells;

    // Bohrer: Ziel suchen (bestimmt den Bedarf)
    std::vector<char> drillHasTarget(list.size(), 0);
    std::vector<glm::ivec3> drillTargets(list.size());
    for (size_t i = 0; i < list.size(); ++i) {
        MachineEntry* e = list[i];
        if (e->m.kind != MachineKind::Drill) continue;
        drillHasTarget[i] = drillTarget(e->pos, drillFacing(getBlock(e->pos.x, e->pos.y, e->pos.z)), drillTargets[i]) ? 1 : 0;
    }

    // Antriebsnetze aufbauen (von Motoren und Bohrern aus)
    for (MachineEntry* e : list) {
        if (e->m.kind != MachineKind::Motor && e->m.kind != MachineKind::Drill) continue;
        const uint64_t k = machineKey(e->pos.x, e->pos.y, e->pos.z);
        if (mechOf.count(k)) continue;
        const int id = static_cast<int>(mech.size());
        mech.emplace_back();
        mechCells.emplace_back();
        flood(e->pos, isMech, mechOf, id, mechCells.back());
    }
    for (size_t ci = 0; ci < mech.size(); ++ci)
        for (const glm::ivec3& p : mechCells[ci]) {
            const BlockId b = getBlock(p.x, p.y, p.z);
            if (isConveyor(b)) mech[ci].demand += CONVEYOR_POWER;
            else if (b == LIFT) mech[ci].demand += LIFT_POWER;
        }
    for (size_t i = 0; i < list.size(); ++i) {
        MachineEntry* e = list[i];
        const auto it = mechOf.find(machineKey(e->pos.x, e->pos.y, e->pos.z));
        if (it == mechOf.end()) continue;
        Comp& c = mech[static_cast<size_t>(it->second)];
        if (e->m.kind == MachineKind::Drill && drillHasTarget[i]) c.demand += DRILL_POWER;
        if (e->m.kind == MachineKind::Motor) ++c.producers;
    }
    for (Comp& c : mech) {
        const float cap = c.producers * MOTOR_POWER;
        c.util = (c.demand > 0.0f && cap > 0.0f) ? std::min(1.0f, c.demand / cap) : 0.0f;
    }

    // Dampfnetze (von Kesseln, Motoren und Destillen aus) und ihr Bedarf
    std::vector<int> steamIdOf(list.size(), -1);
    for (size_t i = 0; i < list.size(); ++i) {
        MachineEntry* e = list[i];
        if (e->m.kind == MachineKind::Drill) continue;
        const uint64_t k = machineKey(e->pos.x, e->pos.y, e->pos.z);
        auto it = steamOf.find(k);
        if (it == steamOf.end()) {
            const int id = static_cast<int>(steam.size());
            steam.emplace_back();
            std::vector<glm::ivec3> cells;
            flood(e->pos, isSteam, steamOf, id, cells);
            it = steamOf.find(k);
        }
        steamIdOf[i] = it->second;
    }
    // Destille: laeuft sie? (Rezept vorhanden und Ausgabe passt)
    auto stillCanRun = [](const Machine& m, const StillRecipe*& r) {
        r = m.slots[0].empty() ? nullptr : stillRecipe(m.slots[0].id);
        if (!r) return false;
        auto fits = [](const ItemStack& slot, const ItemStack& out) {
            return out.empty() || slot.empty() || (slot.id == out.id && slot.count + out.count <= itemInfo(out.id).maxStack);
        };
        return fits(m.slots[1], r->out1) && fits(m.slots[2], r->out2);
    };
    for (size_t i = 0; i < list.size(); ++i) {
        MachineEntry* e = list[i];
        if (steamIdOf[i] < 0) continue;
        Comp& s = steam[static_cast<size_t>(steamIdOf[i])];
        if (e->m.kind == MachineKind::Motor) {
            const auto mi = mechOf.find(machineKey(e->pos.x, e->pos.y, e->pos.z));
            const float u = mi == mechOf.end() ? 0.0f : mech[static_cast<size_t>(mi->second)].util;
            s.demand += MOTOR_IDLE_STEAM + MOTOR_STEAM_AT_FULL * u;
        } else if (e->m.kind == MachineKind::Still) {
            const StillRecipe* r = nullptr;
            if (stillCanRun(e->m, r)) s.demand += STILL_STEAM;
        }
    }
    // Kessel: Wassereimer leeren, bei Bedarf anzuenden, Angebot zaehlen
    for (size_t i = 0; i < list.size(); ++i) {
        Machine& m = list[i]->m;
        if (m.kind != MachineKind::Boiler) continue;
        ItemStack& bucket = m.slots[1];
        if ((bucket.id == ITEM_BUCKET_WATER || bucket.id == ITEM_BUCKET_DISTILLED) && !bucket.empty() && m.water + BOILER_BUCKET_ML <= BOILER_WATER_MAX + 0.001f) {
            const float add = bucket.id == ITEM_BUCKET_DISTILLED ? 1.0f : 0.0f;
            m.purity = (m.purity * m.water + add * BOILER_BUCKET_ML) / (m.water + BOILER_BUCKET_ML);
            m.water += BOILER_BUCKET_ML;
            bucket = {ITEM_BUCKET, 1, 0};
        }
        Comp& s = steam[static_cast<size_t>(steamIdOf[i])];
        if (!m.burning() && s.demand > 0.0f && m.water > 0.0f && !m.slots[0].empty() && boilerFuelSeconds(m.slots[0].id) > 0.0f) {
            m.burnTime = m.burnTotal = boilerFuelSeconds(m.slots[0].id);
            if (--m.slots[0].count <= 0) m.slots[0].clear();
        }
        if (m.burning() && m.water > 0.0f) {
            s.supply += BOILER_STEAM;
            ++s.producers;
        }
    }
    for (Comp& s : steam) s.sat = s.demand > 0.0f ? (s.supply > 0.0f ? std::min(1.0f, s.supply / s.demand) : 0.0f) : 1.0f;

    // Kessel verbrauchen Brennstoff und Wasser je nach Last, Motoren liefern Leistung
    for (size_t i = 0; i < list.size(); ++i) {
        MachineEntry* e = list[i];
        Machine& m = e->m;
        if (steamIdOf[i] < 0) continue;
        const Comp& s = steam[static_cast<size_t>(steamIdOf[i])];
        m.steam = s.sat;
        if (m.kind == MachineKind::Boiler) {
            const bool producing = m.burning() && m.water > 0.0f;
            m.load = producing && s.supply > 0.0f ? std::min(1.0f, s.demand / s.supply) : 0.0f;
            if (m.burning()) m.burnTime = std::max(0.0f, m.burnTime - dt * std::max(0.1f, m.load));
            if (producing) m.water = std::max(0.0f, m.water - dt * BOILER_STEAM * m.load * BOILER_WATER_PER_STEAM / (1.0f + m.purity));
            m.working = producing && m.load > 0.0f;
            const BlockId want = m.burning() ? BOILER_LIT : BOILER;
            if (getBlock(e->pos.x, e->pos.y, e->pos.z) != want) setBlockImpl(e->pos.x, e->pos.y, e->pos.z, want, false);
        } else if (m.kind == MachineKind::Motor) {
            const auto mi = mechOf.find(machineKey(e->pos.x, e->pos.y, e->pos.z));
            if (mi != mechOf.end()) mech[static_cast<size_t>(mi->second)].supply += MOTOR_POWER * s.sat;
        }
    }
    for (size_t ci = 0; ci < mech.size(); ++ci) {
        Comp& c = mech[ci];
        c.sat = c.supply > 0.0f ? (c.demand > 0.0f ? std::min(1.0f, c.supply / c.demand) : 1.0f) : 0.0f;
        for (const glm::ivec3& p : mechCells[ci]) power_[machineKey(p.x, p.y, p.z)] = c.sat;
    }

    // Maschinen arbeiten
    for (size_t i = 0; i < list.size(); ++i) {
        MachineEntry* e = list[i];
        Machine& m = e->m;
        if (m.kind == MachineKind::Motor) {
            const auto mi = mechOf.find(machineKey(e->pos.x, e->pos.y, e->pos.z));
            const float u = mi == mechOf.end() ? 0.0f : mech[static_cast<size_t>(mi->second)].util;
            m.steam = steamIdOf[i] >= 0 ? steam[static_cast<size_t>(steamIdOf[i])].sat : 0.0f;
            m.load = m.steam * (0.15f + 0.85f * u);
            m.working = m.steam > 0.0f;
            m.spin += dt * m.load * 8.0f;
        } else if (m.kind == MachineKind::Still) {
            const StillRecipe* r = nullptr;
            if (stillCanRun(m, r) && m.steam > 0.0f) {
                m.load = m.steam;
                m.working = true;
                m.progress += dt * m.steam / r->seconds;
                if (m.progress >= 1.0f) {
                    m.progress = 0.0f;
                    if (--m.slots[0].count <= 0) m.slots[0].clear();
                    for (int k = 0; k < 2; ++k) {
                        const ItemStack& out = k == 0 ? r->out1 : r->out2;
                        if (out.empty()) continue;
                        ItemStack& slot = m.slots[1 + k];
                        if (slot.empty()) slot = {out.id, out.count, 0};
                        else slot.count += out.count;
                    }
                }
            } else {
                m.load = 0.0f;
                m.working = false;
                if (!r) m.progress = 0.0f;
            }
        } else if (m.kind == MachineKind::Drill) {
            const auto mi = mechOf.find(machineKey(e->pos.x, e->pos.y, e->pos.z));
            const float r = mi == mechOf.end() ? 0.0f : mech[static_cast<size_t>(mi->second)].sat;
            m.load = r;
            m.working = drillHasTarget[i] && r > 0.0f;
            if (!m.working) {
                if (!drillHasTarget[i]) m.progress = 0.0f;
                continue;
            }
            const glm::ivec3 t = drillTargets[i];
            const BlockId tb = getBlock(t.x, t.y, t.z);
            m.progress += dt * r / (0.6f + blockInfo(tb).hardness * 0.6f);
            if (m.progress < 1.0f) continue;
            m.progress = 0.0f;
            ItemId drop = canHarvest(tb, ItemStack{ITEM_IRON_PICKAXE, 1, 0}) ? blockDropItem(tb) : ITEM_NONE;
            setBlock(t.x, t.y, t.z, AIR);
            if (drop == ITEM_NONE) continue;
            int left = 1;
            const int facing = drillFacing(getBlock(e->pos.x, e->pos.y, e->pos.z));
            for (int d = 0; d < 6 && left > 0; ++d) {
                if (d == facing) continue;
                const int nx = e->pos.x + DIR6[d][0], ny = e->pos.y + DIR6[d][1], nz = e->pos.z + DIR6[d][2];
                if (getBlock(nx, ny, nz) != CHEST) continue;
                left = chestAt(nx, ny, nz).add(drop, left, 0);
            }
            if (left > 0) machineDrops_.push_back({e->pos, ItemStack{drop, left, 0}});
        }
    }
}

}  // namespace oc
