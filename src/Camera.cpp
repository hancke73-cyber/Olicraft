#include "Camera.h"

#include <algorithm>
#include <cmath>

#include "World.h"

namespace oc {

CameraMode nextCameraMode(CameraMode m) {
    switch (m) {
        case CameraMode::First: return CameraMode::ThirdBack;
        case CameraMode::ThirdBack: return CameraMode::ThirdFront;
        default: return CameraMode::First;
    }
}

CameraPose computeCamera(const World& world, const glm::vec3& eye, const glm::vec3& look, CameraMode mode, float distance) {
    CameraPose pose{eye, look};
    if (mode == CameraMode::First) return pose;

    const glm::vec3 dir = glm::normalize(look);
    const glm::vec3 away = mode == CameraMode::ThirdBack ? -dir : dir;  // Richtung vom Kopf zur Kamera

    // Seitliche Hilfsachsen: mehrere Strahlen mit kleinem Versatz, damit die Kamera auch nicht an Kanten klemmt
    glm::vec3 right = glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f));
    if (glm::dot(right, right) < 1e-6f) right = glm::vec3(1.0f, 0.0f, 0.0f);
    right = glm::normalize(right);
    const glm::vec3 up = glm::normalize(glm::cross(right, dir));

    const float margin = 0.3f;
    float allowed = distance;
    for (const glm::vec3& off : {glm::vec3(0.0f), right * 0.2f, -right * 0.2f, up * 0.2f, -up * 0.2f}) {
        const RayHit h = world.raycast(eye + off, away, distance + margin);
        if (h.hit) allowed = std::min(allowed, std::max(0.0f, h.dist - margin));
    }
    pose.pos = eye + away * allowed;
    pose.forward = away * -1.0f;  // Blick zurueck auf den Kopf
    if (mode == CameraMode::ThirdBack) pose.forward = dir;  // von hinten: gleiche Blickrichtung wie der Spieler
    return pose;
}

}  // namespace oc
