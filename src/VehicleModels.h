#pragma once
#include "Vehicles.h"

namespace oc {

class World;

// Abmessungen der Luftschiffe: Ballon 0 ist ein runder Heissluftballon, 1 und 2 sind Zeppeline
constexpr float MODULE_LENGTH = 2.4f;  // eine Gondel
float balloonLength(int size);
float balloonDiameter(int size);
float balloonCenterY(int size);  // Hoehe der Ballonmitte ueber der Gondelmitte

// Haengt die Quader des Fahrzeugs (Boot oder Luftschiff) an die Entitaetengeometrie (Positionen relativ zur Kamera)
void appendVehicleMesh(const Vehicle& v, const World& world, const glm::vec3& camPos, float time, EntityBatches& out);

}  // namespace oc
