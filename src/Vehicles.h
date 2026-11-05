#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Entities.h"
#include "Machines.h"

namespace oc {

class World;

// Dampffahrzeuge: eigene Figuren mit Bordkessel. Boot auf dem Wasser, Luftschiff aus Gondeln, Ballon und Propeller.
enum class VehicleType : uint8_t { Boat = 0, Airship = 1 };

constexpr int VEHICLE_HEALTH = 20;
constexpr int AIRSHIP_MAX_MODULES = 3;
constexpr float VEHICLE_IDLE_STEAM = 0.3f;   // Dampfbedarf mit Fahrer ohne Gas
constexpr float VEHICLE_FULL_STEAM = 4.0f;   // zusaetzlicher Bedarf bei vollem Gas

struct Vehicle {
    int id = 0;
    VehicleType type = VehicleType::Boat;
    glm::vec3 pos{0.0f};  // Boot: Unterkante des Rumpfes, Luftschiff: Mitte der Gondeln
    glm::vec3 vel{0.0f};
    float yaw = 0.0f;  // Bogenmass, vorwaerts = (cos, 0, sin)
    int balloon = -1;  // Luftschiff: -1 ohne Ballon, 0 klein, 1 mittel, 2 gross
    bool propeller = false;
    int modules = 1;   // Luftschiff: Gondeln hintereinander
    int health = VEHICLE_HEALTH;
    Machine engine;    // Bordkessel: [0] Brennstoff, [1] Wassereimer
    bool anchored = false;  // Luftschiff: mit dem Seil am Boden festgemacht
    // abgeleitet, nicht gespeichert
    bool occupied = false;
    float thrust = 0.0f;   // -1..1 wirksamer Vortrieb
    float propAngle = 0.0f;
    float hurtTimer = 0.0f;

    // Das Luftschiff fliegt nur mit Ballon, Propeller und einem Ballon, der die Gondeln tragen kann
    bool flyable() const { return type == VehicleType::Airship && balloon >= 0 && propeller && modules <= balloon + 1; }
};

// Eingabe des Fahrers: Gas -1..1, Lenkung -1..1 (rechts positiv), Hoehe -1..1 (Luftschiff)
struct VehicleInput {
    float forward = 0.0f, turn = 0.0f, lift = 0.0f;
};

// Abmessungen (lokal, Mitte = pos): Box fuer Treffer und Anzeige
void vehicleHull(const Vehicle& v, glm::vec3& lo, glm::vec3& hi);
// Sitz des Fahrers (Fusspunkt des Spielers)
glm::vec3 vehicleSeat(const Vehicle& v);
// Gegenstaende, die beim Abbauen herausfallen (Fahrzeug in Teilen, Brennstoff und Eimer im Kessel)
std::vector<ItemStack> vehicleDrops(const Vehicle& v);
// Ballongroesse (0..2) eines Ballon-Gegenstands, sonst -1
int balloonSizeOf(ItemId item);
ItemId balloonItemOf(int size);

class VehicleManager {
public:
    void clear() { list_.clear(); nextId_ = 1; }
    Vehicle& spawn(VehicleType type, const glm::vec3& pos, float yaw);
    Vehicle& add(const Vehicle& v);  // aus dem Spielstand: bekommt eine neue Kennung
    Vehicle* find(int id);
    const std::vector<Vehicle>& list() const { return list_; }
    std::vector<Vehicle>& list() { return list_; }
    void remove(int id);

    // riderId: Fahrzeug mit Fahrer (0 = keins); in zeigt die Eingabe des Fahrers
    void update(float dt, World& world, int riderId, const VehicleInput& in);
    // Naechstes Fahrzeug entlang des Strahls
    Vehicle* raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float& outDist);
    void buildMesh(const World& world, const glm::vec3& camPos, float time, EntityBatches& out) const;

private:
    std::vector<Vehicle> list_;
    int nextId_ = 1;
};

}  // namespace oc
