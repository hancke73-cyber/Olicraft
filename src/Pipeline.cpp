// Wasserleitung: Pumpen saugen aus Quellbloecken in ihrer Naehe und liefern ueber Wasserrohre an Kessel und Auslaesse.
// Wird von stepMachines aufgerufen: planPipelines vor der Antriebsberechnung (Bedarf), runPipelines danach (Lieferung).
#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "Machines.h"
#include "World.h"

namespace oc {

bool World::pumpIntake(const glm::ivec3& pump, glm::ivec3& out) const {
    for (const auto& d : DIR6) {
        const glm::ivec3 q(pump.x + d[0], pump.y + d[1], pump.z + d[2]);
        if (getBlock(q.x, q.y, q.z) == WATER) {
            out = q;
            return true;
        }
    }
    return false;
}

// Zwei Quellen als waagerechte Nachbarn ergeben eine unendliche Quelle (wie beim Wasserfluss): das Meer versiegt nicht
bool World::isInfiniteSource(const glm::ivec3& p) const {
    int n = 0;
    for (int i = 0; i < 4; ++i)
        if (getBlock(p.x + DIR6[i][0], p.y, p.z + DIR6[i][2]) == WATER) ++n;
    return n >= 2;
}

void World::planPipelines(const std::vector<MachineEntry*>& list) {
    waterNets_.clear();
    std::unordered_set<uint64_t> seen;
    for (MachineEntry* start : list) {
        if (start->m.kind != MachineKind::Pump || seen.count(machineKey(start->pos.x, start->pos.y, start->pos.z))) continue;
        WaterNet net;
        std::unordered_set<uint64_t> boilerSeen;
        std::vector<glm::ivec3> stack{start->pos};
        seen.insert(machineKey(start->pos.x, start->pos.y, start->pos.z));
        int visited = 0;
        while (!stack.empty() && visited < 4096) {
            const glm::ivec3 p = stack.back();
            stack.pop_back();
            ++visited;
            const BlockId pb = getBlock(p.x, p.y, p.z);
            if (pb == PUMP) {
                const auto it = machines_.find(machineKey(p.x, p.y, p.z));
                if (it != machines_.end()) net.pumps.push_back(&it->second);
            } else if (pb == OUTLET) {
                net.outlets.push_back(p);
                continue;  // der Auslass ist Endpunkt
            }
            for (const auto& d : DIR6) {
                const glm::ivec3 q(p.x + d[0], p.y + d[1], p.z + d[2]);
                const BlockId qb = getBlock(q.x, q.y, q.z);
                const uint64_t k = machineKey(q.x, q.y, q.z);
                if (isBoiler(qb)) {
                    if (boilerSeen.insert(k).second) net.boilers.push_back(q);
                } else if ((qb == WATER_PIPE || qb == PUMP || qb == OUTLET) && seen.insert(k).second) {
                    stack.push_back(q);
                }
            }
        }
        // Braucht jemand Wasser?
        bool need = false;
        for (const glm::ivec3& b : net.boilers) {
            const auto it = machines_.find(machineKey(b.x, b.y, b.z));
            if (it != machines_.end() && it->second.m.water < BOILER_WATER_MAX - 1.0f) need = true;
        }
        for (const glm::ivec3& o : net.outlets)
            if (getBlock(o.x, o.y - 1, o.z) == AIR) need = true;
        for (MachineEntry* p : net.pumps) {
            glm::ivec3 src;
            p->m.wantsPower = need && pumpIntake(p->pos, src);
        }
        waterNets_.push_back(std::move(net));
    }
}

void World::runPipelines(float dt) {
    for (WaterNet& net : waterNets_) {
        float flow = 0.0f;
        for (MachineEntry* p : net.pumps) {
            glm::ivec3 src;
            const bool ok = p->m.wantsPower && pumpIntake(p->pos, src);
            const float f = ok ? PUMP_FLOW * dt * powerAt(p->pos.x, p->pos.y, p->pos.z) : 0.0f;
            p->m.working = f > 0.0f;
            p->m.load = ok ? powerAt(p->pos.x, p->pos.y, p->pos.z) : 0.0f;
            flow += f;
        }
        if (net.pumps.empty()) continue;
        const float carry = net.pumps[0]->m.pumped;
        float budget = flow + carry;
        const float available = budget;
        // Kessel zuerst: sie fuellen ihren Tank (Meerwasser ist unrein, mischt sich mit destilliertem)
        for (const glm::ivec3& b : net.boilers) {
            const auto it = machines_.find(machineKey(b.x, b.y, b.z));
            if (it == machines_.end() || budget <= 0.0f) continue;
            Machine& m = it->second.m;
            const float give = std::min(budget, BOILER_WATER_MAX - m.water);
            if (give <= 0.0f) continue;
            m.purity = m.purity * m.water / (m.water + give);
            m.water += give;
            budget -= give;
        }
        // Auslaesse setzen eine Quelle darunter, solange dort Luft ist
        for (const glm::ivec3& o : net.outlets) {
            if (budget < OUTLET_COST || getBlock(o.x, o.y - 1, o.z) != AIR) continue;
            setBlock(o.x, o.y - 1, o.z, WATER);
            budget -= OUTLET_COST;
        }
        // Vorrat nur begrenzt mitnehmen, endliche Quellen verbrauchen sich
        const float used = available - budget;
        const float usedFromFlow = flow > 0.0f ? std::min(flow, used) : 0.0f;
        for (size_t i = 0; i < net.pumps.size(); ++i) net.pumps[i]->m.pumped = 0.0f;
        net.pumps[0]->m.pumped = std::min(budget, WATER_SOURCE_ML);
        if (usedFromFlow <= 0.0f) continue;
        for (MachineEntry* p : net.pumps) {
            glm::ivec3 src;
            if (!p->m.working || !pumpIntake(p->pos, src) || isInfiniteSource(src)) continue;
            p->m.drained += usedFromFlow * (PUMP_FLOW * dt * powerAt(p->pos.x, p->pos.y, p->pos.z)) / std::max(flow, 1e-6f);
            if (p->m.drained >= WATER_SOURCE_ML) {
                p->m.drained -= WATER_SOURCE_ML;
                setBlock(src.x, src.y, src.z, AIR);
            }
        }
    }
}

}  // namespace oc
