#include "Vehicles.h"

#include <algorithm>
#include <cmath>

#include "VehicleModels.h"
#include "Physics.h"
#include "World.h"

namespace oc {

namespace {

// Oberflaeche des Wassers unter (x, z) in der Naehe von y: y der Wasseroberkante, sonst false
bool waterSurface(const World& w, float x, float z, float y, float& surface) {
    const int bx = static_cast<int>(std::floor(x)), bz = static_cast<int>(std::floor(z));
    for (int yy = static_cast<int>(std::floor(y)) + 1; yy >= static_cast<int>(std::floor(y)) - 2; --yy) {
        if (isWater(w.getBlock(bx, yy, bz)) && !isWater(w.getBlock(bx, yy + 1, bz))) {
            surface = static_cast<float>(yy) + 0.88f;
            return true;
        }
    }
    return false;
}

}  // namespace

int balloonSizeOf(ItemId item) { return item == ITEM_BALLOON_S ? 0 : (item == ITEM_BALLOON_M ? 1 : (item == ITEM_BALLOON_L ? 2 : -1)); }
ItemId balloonItemOf(int size) { return size == 0 ? ITEM_BALLOON_S : (size == 1 ? ITEM_BALLOON_M : ITEM_BALLOON_L); }

void vehicleHull(const Vehicle& v, glm::vec3& lo, glm::vec3& hi) {
    if (v.type == VehicleType::Boat) {
        lo = glm::vec3(-1.5f, -0.1f, -0.7f);
        hi = glm::vec3(1.7f, 1.3f, 0.7f);
        return;
    }
    const float gl = MODULE_LENGTH * static_cast<float>(v.modules) * 0.5f + 1.5f;  // Gondeln und Propeller
    float half = gl, top = 0.8f, wide = 0.9f;
    if (v.balloon >= 0) {
        half = std::max(half, balloonLength(v.balloon) * 0.5f);
        top = balloonCenterY(v.balloon) + balloonDiameter(v.balloon) * 0.5f;
        wide = std::max(wide, balloonDiameter(v.balloon) * 0.5f);
    }
    lo = glm::vec3(-half, -0.7f, -wide);
    hi = glm::vec3(half, top, wide);
}

glm::vec3 vehicleSeat(const Vehicle& v) {
    const glm::vec3 fwd(std::cos(v.yaw), 0.0f, std::sin(v.yaw));
    if (v.type == VehicleType::Boat) return v.pos + fwd * (-0.7f) + glm::vec3(0.0f, 0.3f, 0.0f);
    return v.pos + fwd * (MODULE_LENGTH * static_cast<float>(v.modules - 1) * 0.5f) + glm::vec3(0.0f, -0.55f, 0.0f);
}

std::vector<ItemStack> vehicleDrops(const Vehicle& v) {
    std::vector<ItemStack> out;
    if (v.type == VehicleType::Boat) {
        out.push_back({ITEM_STEAMBOAT, 1, 0});
    } else {
        out.push_back({ITEM_GONDOLA, v.modules, 0});
        if (v.balloon >= 0) out.push_back({balloonItemOf(v.balloon), 1, 0});
        if (v.propeller) out.push_back({ITEM_PROPELLER, 1, 0});
    }
    for (const ItemStack& s : v.engine.slots)
        if (!s.empty()) out.push_back(s);
    return out;
}

Vehicle& VehicleManager::spawn(VehicleType type, const glm::vec3& pos, float yaw) {
    Vehicle v;
    v.id = nextId_++;
    v.type = type;
    v.pos = pos;
    v.yaw = yaw;
    v.engine.kind = MachineKind::Boiler;
    list_.push_back(v);
    return list_.back();
}

Vehicle& VehicleManager::add(const Vehicle& v) {
    list_.push_back(v);
    list_.back().id = nextId_++;
    list_.back().engine.kind = MachineKind::Boiler;
    return list_.back();
}

Vehicle* VehicleManager::find(int id) {
    for (Vehicle& v : list_)
        if (v.id == id) return &v;
    return nullptr;
}

void VehicleManager::remove(int id) {
    list_.erase(std::remove_if(list_.begin(), list_.end(), [id](const Vehicle& v) { return v.id == id; }), list_.end());
}

// ---------------------------------------------------------------------------------------------------------------
// Bewegung
// ---------------------------------------------------------------------------------------------------------------

void VehicleManager::update(float dt, World& world, int riderId, const VehicleInput& in) {
    if (!steampunkEnabled()) return;  // Erweiterung aus: Fahrzeuge ruhen
    for (Vehicle& v : list_) {
        const int bx = static_cast<int>(std::floor(v.pos.x)), bz = static_cast<int>(std::floor(v.pos.z));
        if (!world.isChunkLoaded(floorDiv(bx, CHUNK_W), floorDiv(bz, CHUNK_W))) continue;  // im nicht geladenen Bereich ruhen sie
        const bool rider = v.id == riderId;
        v.occupied = rider;
        v.hurtTimer = std::max(0.0f, v.hurtTimer - dt);

        // Bordkessel: nur mit Fahrer wird Dampf verlangt
        Machine& e = v.engine;
        boilerLoadBucket(e);
        const float gas = rider ? std::fabs(in.forward) : 0.0f;
        float demand = rider ? VEHICLE_IDLE_STEAM + VEHICLE_FULL_STEAM * std::max(gas, std::fabs(in.lift) * 0.5f) : 0.0f;
        if (demand > 0.0f) boilerIgnite(e);
        const float supply = boilerProducing(e) ? BOILER_STEAM : 0.0f;
        const float sat = demand > 0.0f ? std::min(1.0f, supply / demand) : 0.0f;
        e.load = supply > 0.0f ? std::min(1.0f, demand / supply) : 0.0f;
        boilerBurn(e, e.load, dt);
        e.steam = sat;
        e.working = supply > 0.0f && e.load > 0.0f;
        v.thrust = rider ? in.forward * sat : 0.0f;
        v.propAngle += dt * (std::fabs(v.thrust) * 40.0f + (e.burning() ? 2.0f : 0.0f));

        const glm::vec3 fwd(std::cos(v.yaw), 0.0f, std::sin(v.yaw));
        float sp = glm::dot(glm::vec3(v.vel.x, 0.0f, v.vel.z), fwd);

        if (v.type == VehicleType::Boat) {
            float surface = 0.0f;
            const bool inWater = waterSurface(world, v.pos.x, v.pos.z, v.pos.y, surface);
            if (inWater) {
                sp += (v.thrust * 4.5f - 0.8f * sp) * dt;
                if (rider && std::fabs(sp) > 0.05f) v.yaw += in.turn * 1.2f * std::min(1.0f, 0.35f + std::fabs(sp) / 2.5f) * (sp < 0.0f ? -1.0f : 1.0f) * dt;
                else if (rider) v.yaw += in.turn * 0.5f * (sat > 0.0f ? 1.0f : 0.0f) * dt;  // Wenden auf der Stelle mit Schub
            } else {
                sp *= std::exp(-3.0f * dt);  // gestrandet: bremst ab
            }
            const glm::vec3 f2(std::cos(v.yaw), 0.0f, std::sin(v.yaw));
            v.vel = glm::vec3(f2.x * sp, 0.0f, f2.z * sp);
            // Schritt nur, wenn dahinter wieder Wasser ist (das Boot verlaesst das Wasser nicht)
            const glm::vec3 step = v.vel * dt;
            float s2 = 0.0f;
            glm::vec3 np = v.pos + step;
            if (waterSurface(world, np.x, np.z, v.pos.y, s2) || !inWater) {
                glm::vec3 p = v.pos, vv = v.vel;
                const bool hx = moveBodyAxis(world, p, vv, 0, step.x, 1.3f, 0.7f, nullptr);
                const bool hz = moveBodyAxis(world, p, vv, 2, step.z, 1.3f, 0.7f, nullptr);
                if (hx || hz) sp *= 0.3f;
                v.pos.x = p.x;
                v.pos.z = p.z;
            } else {
                sp = 0.0f;
            }
            if (waterSurface(world, v.pos.x, v.pos.z, v.pos.y, surface)) {
                v.pos.y += (surface - 0.45f - v.pos.y) * std::min(1.0f, 8.0f * dt);
            } else {
                glm::vec3 p = v.pos, vv(0.0f, v.vel.y - 20.0f * dt, 0.0f);
                bool landed = false;
                moveBodyAxis(world, p, vv, 1, vv.y * dt, 1.3f, 0.7f, &landed);
                v.pos.y = p.y;
                v.vel.y = landed ? 0.0f : vv.y;
            }
            continue;
        }

        // Luftschiff
        glm::vec3 hlo, hhi;
        vehicleHull(v, hlo, hhi);
        const float h = hhi.y - hlo.y;
        const bool fly = v.flyable();
        if (rider) v.anchored = false;
        if (v.anchored) {
            v.vel = glm::vec3(0.0f);
            continue;
        }
        float vy = v.vel.y;
        if (fly) {
            sp += (v.thrust * 2.6f - 0.5f * sp) * dt;
            if (rider) v.yaw += in.turn * 0.55f * std::min(1.0f, 0.3f + std::fabs(sp) / 1.5f) * (sp < 0.0f ? -1.0f : 1.0f) * dt;
            const float target = !rider ? (e.burning() ? 0.0f : -0.8f) : (in.lift > 0.0f ? 2.5f * sat : (in.lift < 0.0f ? -2.5f : (sat > 0.0f ? 0.0f : -0.8f)));
            vy += (target - vy) * std::min(1.0f, 2.5f * dt);
        } else {
            sp *= std::exp(-2.0f * dt);
            vy = std::max(-25.0f, vy - 20.0f * dt);  // ohne Ballon oder Propeller bleibt es am Boden
        }
        const glm::vec3 f2(std::cos(v.yaw), 0.0f, std::sin(v.yaw));
        v.vel = glm::vec3(f2.x * sp, vy, f2.z * sp);
        glm::vec3 p = v.pos + glm::vec3(0.0f, hlo.y, 0.0f), vv = v.vel;  // Koerper steht auf der Unterkante des Rumpfes
        bool landed = false;
        const bool hx = moveBodyAxis(world, p, vv, 0, v.vel.x * dt, 2.2f, h, nullptr);
        const bool hz = moveBodyAxis(world, p, vv, 2, v.vel.z * dt, 2.2f, h, nullptr);
        moveBodyAxis(world, p, vv, 1, v.vel.y * dt, 2.2f, h, &landed);
        if (hx || hz) v.vel.x = v.vel.z = 0.0f;
        v.pos = p - glm::vec3(0.0f, hlo.y, 0.0f);
        if (p.y > static_cast<float>(CHUNK_H) - 4.0f) v.pos.y = static_cast<float>(CHUNK_H) - 4.0f;
        if (landed || vv.y == 0.0f) v.vel.y = 0.0f;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Strahl
// ---------------------------------------------------------------------------------------------------------------

Vehicle* VehicleManager::raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float& outDist) {
    Vehicle* best = nullptr;
    float bestT = maxDist;
    for (Vehicle& v : list_) {
        const float c = std::cos(v.yaw), s = std::sin(v.yaw);
        auto toLocal = [&](const glm::vec3& d) { return glm::vec3(d.x * c + d.z * s, d.y, -d.x * s + d.z * c); };
        const glm::vec3 o = toLocal(origin - v.pos), d = toLocal(dir);
        glm::vec3 lo, hi;
        vehicleHull(v, lo, hi);
        float t0 = 0.0f, t1 = bestT;
        bool hit = true;
        for (int a = 0; a < 3 && hit; ++a) {
            if (std::fabs(d[a]) < 1e-6f) {
                if (o[a] < lo[a] || o[a] > hi[a]) hit = false;
            } else {
                float ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                if (t0 > t1) hit = false;
            }
        }
        if (hit && t0 < bestT) {
            bestT = t0;
            best = &v;
        }
    }
    outDist = bestT;
    return best;
}

// ---------------------------------------------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------------------------------------------

void VehicleManager::buildMesh(const World& world, const glm::vec3& camPos, float time, EntityBatches& out) const {
    for (const Vehicle& v : list_) appendVehicleMesh(v, world, camPos, time, out);
}

}  // namespace oc
