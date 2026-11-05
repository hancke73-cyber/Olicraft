// Fahrzeuge im Spiel: setzen, einsteigen, fahren, Bordkessel-Fenster, abbauen
#include <cmath>
#include <cstdio>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "Game.h"

namespace oc {

namespace {
constexpr float PI = 3.14159265358979f;
}

Machine* Game::activeMachine() {
    if (engineVehicleId_ != 0) {
        Vehicle* v = vehicles_.find(engineVehicleId_);
        return v ? &v->engine : nullptr;
    }
    return world_->findMachine(machinePos_.x, machinePos_.y, machinePos_.z);
}

void Game::openVehicleEngine(int id) {
    if (!vehicles_.find(id)) return;
    openInventory(true);
    furnaceWindow_ = true;
    machineWindow_ = true;
    engineVehicleId_ = id;
}

// Name und Zustand des Fahrzeugs fuer die Anzeige am Fadenkreuz
std::array<std::string, 3> Game::vehicleInfo(const Vehicle& v) const {
    std::array<std::string, 3> out;
    auto pct = [](float f) { return std::to_string(static_cast<int>(std::lround(std::max(0.0f, std::min(1.0f, f)) * 100.0f))) + " %"; };
    if (v.type == VehicleType::Boat) {
        out[0] = "Dampfboot";
    } else {
        out[0] = "Luftschiff";
        std::string parts = std::to_string(v.modules) + (v.modules == 1 ? " Gondel" : " Gondeln");
        parts += v.balloon < 0 ? ", kein Ballon" : (v.balloon == 0 ? ", kleiner Ballon" : (v.balloon == 1 ? ", mittlerer Ballon" : ", grosser Ballon"));
        parts += v.propeller ? "" : ", kein Propeller";
        out[2] = parts;
    }
    const Machine& e = v.engine;
    out[1] = "Wasser " + pct(e.water / BOILER_WATER_MAX) + (e.burning() ? "   Kessel brennt" : (e.slots[0].empty() ? "   Brennstoff fehlt" : ""));
    if (v.type == VehicleType::Boat && out[2].empty()) out[2] = ridingId_ == v.id ? "F steigt aus, E Kessel" : "Rechtsklick: einsteigen";
    if (v.type == VehicleType::Airship && ridingId_ == v.id) out[2] = v.flyable() ? "Leertaste hoch, Umschalt runter, F aus" : "Fliegt nicht: Ballon, Propeller oder groesserer Ballon fehlt";
    return out;
}

void Game::mountVehicle(Vehicle& v) {
    ridingId_ = v.id;
    v.anchored = false;
    player_.flying = false;
    player_.pos = vehicleSeat(v);
    player_.vel = glm::vec3(0.0f);
    player_.resetFall();
    showMessage(v.type == VehicleType::Boat ? "Dampfboot: W/S Gas, A/D lenken, F aussteigen, E Kessel" : "Luftschiff: W/S Gas, A/D lenken, Leertaste/Umschalt Hoehe, F aussteigen, E Kessel");
}

void Game::dismountVehicle() {
    if (ridingId_ == 0) return;
    if (Vehicle* v = vehicles_.find(ridingId_)) {
        v->occupied = false;
        if (v->type == VehicleType::Airship) v->anchored = true;  // das Seil haelt es fest
        // neben das Fahrzeug stellen, auf der Seite des Fahrers
        const glm::vec3 side(-std::sin(v->yaw), 0.0f, std::cos(v->yaw));
        glm::vec3 p = vehicleSeat(*v) + side * (v->type == VehicleType::Boat ? 1.5f : 1.8f);
        if (v->type == VehicleType::Boat) p.y = v->pos.y + 0.9f;
        player_.pos = p;
    }
    player_.vel = glm::vec3(0.0f);
    player_.resetFall();
    ridingId_ = 0;
}

// Vor dem Spielerupdate: Fahrzeuge bewegen. true, wenn der Spieler sitzt (dann entfaellt seine eigene Physik)
bool Game::updateVehicles(float dt, const MoveInput& in) {
    VehicleInput vin;
    vin.forward = in.forward;
    vin.turn = in.strafe;
    vin.lift = (in.jump ? 1.0f : 0.0f) - (in.sneak ? 1.0f : 0.0f);
    if (!steampunkEnabled() && ridingId_ != 0) dismountVehicle();  // ohne die Erweiterung ruhen die Fahrzeuge
    vehicles_.update(dt, *world_, ridingId_, vin);
    if (ridingId_ == 0) return false;
    Vehicle* v = vehicles_.find(ridingId_);
    if (!v || dead_) {
        ridingId_ = 0;
        return false;
    }
    player_.pos = vehicleSeat(*v);
    player_.vel = glm::vec3(0.0f);
    player_.onGround = true;
    player_.inWater = false;
    player_.resetFall();
    return true;
}

// Boot auf Wasser oder Gondel auf festen Boden setzen
bool Game::useVehicleItem(ItemStack& sel) {
    const RayHit hit = world_->raycast(player_.eyePos(), player_.front(), REACH_VEHICLE, true);
    if (!hit.hit) return false;
    const BlockId hb = world_->getBlock(hit.block.x, hit.block.y, hit.block.z);
    const float yaw = glm::radians(player_.yaw);
    if (sel.id == ITEM_STEAMBOAT) {
        if (!isWater(hb) || isWater(world_->getBlock(hit.block.x, hit.block.y + 1, hit.block.z)) || world_->isSolidAt(hit.block.x, hit.block.y + 1, hit.block.z)) {
            showMessage("Das Boot gehoert auf eine Wasserflaeche");
            return false;
        }
        vehicles_.spawn(VehicleType::Boat, glm::vec3(hit.block.x + 0.5f, hit.block.y + 0.88f - 0.45f, hit.block.z + 0.5f), yaw);
    } else {
        if (isLiquid(hb) || hit.normal.y <= 0 || world_->isSolidAt(hit.block.x, hit.block.y + 1, hit.block.z) || world_->isSolidAt(hit.block.x, hit.block.y + 2, hit.block.z)) {
            showMessage("Die Gondel gehoert auf festen Boden mit Platz darueber");
            return false;
        }
        Vehicle& v = vehicles_.spawn(VehicleType::Airship, glm::vec3(hit.block.x + 0.5f, hit.block.y + 1.0f + 0.71f, hit.block.z + 0.5f), yaw);
        v.anchored = true;
    }
    playSound(SoundId::BlockWood, glm::vec3(hit.block) + glm::vec3(0.5f), 0.9f, 0.8f);
    if (!creative_) inventory_.take(slot_, 1);
    placeTimer_ = 0.3f;
    return true;
}

// Rechtsklick auf ein Fahrzeug: mit Bauteil in der Hand ausbauen, sonst einsteigen; mit Umschalt der Bordkessel
void Game::interactVehicle(Vehicle& v) {
    placeTimer_ = 0.35f;
    if (!steampunkEnabled()) return;
    ItemStack& sel = inventory_.slots[slot_];
    const bool shift = glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (shift) {
        openVehicleEngine(v.id);
        return;
    }
    if (v.type == VehicleType::Airship && !sel.empty()) {
        const int size = balloonSizeOf(sel.id);
        if (size >= 0) {
            if (v.balloon >= 0) {
                showMessage("Es hat schon einen Ballon");
                return;
            }
            if (v.modules > size + 1) {
                showMessage("Dieser Ballon ist zu klein fuer die Gondeln");
                return;
            }
            v.balloon = size;
            if (!creative_) inventory_.take(slot_, 1);
            return;
        }
        if (sel.id == ITEM_PROPELLER) {
            if (v.propeller) {
                showMessage("Es hat schon einen Propeller");
                return;
            }
            v.propeller = true;
            if (!creative_) inventory_.take(slot_, 1);
            return;
        }
        if (sel.id == ITEM_GONDOLA) {
            const int limit = v.balloon >= 0 ? std::min(AIRSHIP_MAX_MODULES, v.balloon + 1) : AIRSHIP_MAX_MODULES;
            if (v.modules >= limit) {
                showMessage(v.balloon >= 0 ? "Der Ballon traegt keine weitere Gondel" : "Hoechstens drei Gondeln");
                return;
            }
            ++v.modules;
            if (!creative_) inventory_.take(slot_, 1);
            return;
        }
    }
    mountVehicle(v);
}

// Linksklick: Fahrzeug beschaedigen; bei 0 faellt es in Teilen heraus
void Game::hitVehicle(Vehicle& v) {
    if (attackTimer_ > 0.0f) return;
    attackTimer_ = 0.4f;
    v.hurtTimer = 0.25f;
    v.health -= creative_ ? VEHICLE_HEALTH : std::max(2, attackDamage(inventory_.slots[slot_]));
    useHeldTool(1);
    playSound(SoundId::BlockWood, v.pos, 0.8f, 1.1f);
    if (v.health > 0) return;
    for (const ItemStack& s : vehicleDrops(v))
        entities_.spawnItem(s, v.pos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 2.5f, 0.0f), 0.4f);
    if (v.id == ridingId_) dismountVehicle();
    if (v.id == engineVehicleId_) closeInventory();
    vehicles_.remove(v.id);
}

}  // namespace oc
