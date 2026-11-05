// Fabrik: setzt Fahrzeugteile aus Komponenten zusammen. Die Fabrik hat den Inhalt einer Truhe:
// Plaetze 0..17 sind der Eingang, 18..26 der Ausgang. Sie arbeitet nur mit Antrieb (Welle zum Motor).
#include <algorithm>

#include "Machines.h"
#include "World.h"

namespace oc {

const std::vector<FactoryRecipe>& factoryRecipes() {
    static const std::vector<FactoryRecipe> list = {
        // Ergebnis, Zutaten, Sekunden
        {{ITEM_BALLOON_S, 1, 0}, {{ITEM_CLOTH, 8}, {ITEM_ROPE, 4}, {ITEM_RIB, 4}}, 3, 25.0f},
        {{ITEM_BALLOON_M, 1, 0}, {{ITEM_CLOTH, 16}, {ITEM_ROPE, 8}, {ITEM_RIB, 8}, {ITEM_BRASS_PLATE, 2}}, 4, 40.0f},
        {{ITEM_BALLOON_L, 1, 0}, {{ITEM_CLOTH, 28}, {ITEM_ROPE, 12}, {ITEM_RIB, 14}, {ITEM_BRASS_PLATE, 4}}, 4, 60.0f},
        {{ITEM_GONDOLA, 1, 0}, {{ITEM_RIB, 6}, {ITEM_BRASS_PLATE, 4}, {GLASS, 2}, {ITEM_GEAR, 2}}, 4, 25.0f},
        {{ITEM_PROPELLER, 1, 0}, {{ITEM_PISTON, 2}, {ITEM_GEAR, 4}, {ITEM_BRASS_PLATE, 3}, {MOTOR, 1}, {BOILER, 1}}, 5, 35.0f},
        {{ITEM_STEAMBOAT, 1, 0}, {{ITEM_RIB, 8}, {PLANKS, 6}, {ITEM_BRASS_PLATE, 3}, {BOILER, 1}, {ITEM_PISTON, 2}, {ITEM_GEAR, 2}}, 6, 40.0f},
    };
    return list;
}

namespace {

int countOf(const Chest& c, ItemId id) {
    int n = 0;
    for (int i = 0; i < FACTORY_INPUT_SLOTS; ++i)
        if (c.slots[static_cast<size_t>(i)].id == id) n += c.slots[static_cast<size_t>(i)].count;
    return n;
}

// Passt das Ergebnis in den Ausgang (Plaetze 18..26)?
bool outputFits(const Chest& c, const ItemStack& r) {
    for (int i = FACTORY_INPUT_SLOTS; i < Chest::SIZE; ++i) {
        const ItemStack& s = c.slots[static_cast<size_t>(i)];
        if (s.empty() || (s.id == r.id && s.count + r.count <= itemInfo(r.id).maxStack)) return true;
    }
    return false;
}

void take(Chest& c, ItemId id, int count) {
    for (int i = 0; i < FACTORY_INPUT_SLOTS && count > 0; ++i) {
        ItemStack& s = c.slots[static_cast<size_t>(i)];
        if (s.id != id || s.empty()) continue;
        const int n = std::min(count, s.count);
        s.count -= n;
        count -= n;
        if (s.count <= 0) s.clear();
    }
}

}  // namespace

int World::factoryJob(const glm::ivec3& pos) {
    const Chest& c = chestAt(pos.x, pos.y, pos.z);
    const std::vector<FactoryRecipe>& list = factoryRecipes();
    for (size_t r = 0; r < list.size(); ++r) {
        bool ok = true;
        for (int k = 0; k < list[r].partCount && ok; ++k) ok = countOf(c, list[r].parts[k].item) >= list[r].parts[k].count;
        if (ok && outputFits(c, list[r].result)) return static_cast<int>(r);
    }
    return -1;
}

void World::stepFactory(MachineEntry& e, float dt) {
    Machine& m = e.m;
    const int job = factoryJob(e.pos);
    const float power = powerAt(e.pos.x, e.pos.y, e.pos.z);
    m.working = false;
    m.load = power;
    if (job < 0) {  // nichts zu bauen: ein angefangenes Teil geht verloren, wenn Zutaten fehlen
        m.job = -1;
        m.progress = 0.0f;
        return;
    }
    if (job != m.job) {
        if (m.job != -1) m.progress = 0.0f;  // anderes Rezept: von vorn (nach dem Laden bleibt der Fortschritt)
        m.job = job;
    }
    if (power <= 0.0f) return;
    const FactoryRecipe& r = factoryRecipes()[static_cast<size_t>(job)];
    m.working = true;
    m.progress += dt * power / r.seconds;
    if (m.progress < 1.0f) return;
    m.progress = 0.0f;
    Chest& c = chestAt(e.pos.x, e.pos.y, e.pos.z);
    for (int k = 0; k < r.partCount; ++k) take(c, r.parts[k].item, r.parts[k].count);
    for (int i = FACTORY_INPUT_SLOTS; i < Chest::SIZE; ++i) {
        ItemStack& s = c.slots[static_cast<size_t>(i)];
        if (s.empty()) {
            s = r.result;
            break;
        }
        if (s.id == r.result.id && s.count + r.result.count <= itemInfo(r.result.id).maxStack) {
            s.count += r.result.count;
            break;
        }
    }
}

}  // namespace oc
