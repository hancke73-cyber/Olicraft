#pragma once
#include <glm/glm.hpp>

namespace oc {

class World;

enum class CameraMode { First, ThirdBack, ThirdFront };

struct CameraPose {
    glm::vec3 pos;      // Kameraposition
    glm::vec3 forward;  // Blickrichtung der Kamera
};

// Kamera relativ zum Kopf des Spielers. In der Ansicht von hinten bzw. vorn wird der Abstand verkuerzt, wenn Bloecke
// im Weg sind, damit die Kamera nie im Gelaende steckt.
CameraPose computeCamera(const World& world, const glm::vec3& eye, const glm::vec3& look, CameraMode mode,
                         float distance = 4.0f);

CameraMode nextCameraMode(CameraMode m);

}  // namespace oc
