#pragma once
#include <glm/glm.hpp>

namespace oc {

class World;

// Achsenparallele Kollisionsbox (Fuss-Mitte pos, Breite w, Hoehe h) gegen die Bloecke der Welt.
bool bodyCollides(const World& world, const glm::vec3& pos, float w, float h);

// Bewegt den Koerper um delta entlang einer Achse (0 = x, 1 = y, 2 = z) in kleinen Schritten.
// Bei Kollision wird zurueckgeschoben und vel[axis] auf 0 gesetzt. Rueckgabe: true bei Kollision.
// *landed wird true, wenn der Koerper von oben auf festen Boden trifft.
// stepUp > 0: waagerechte Bewegung steigt auf Kanten bis zu dieser Hoehe hinauf (Platten, Treppen).
bool moveBodyAxis(const World& world, glm::vec3& pos, glm::vec3& vel, int axis, float delta, float w, float h,
                  bool* landed, float stepUp = 0.0f);

}  // namespace oc
