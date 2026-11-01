#include "Physics.h"

#include <algorithm>
#include <cmath>

#include "World.h"

namespace oc {

namespace {
constexpr float EPS = 1e-4f;
}

bool bodyCollides(const World& world, const glm::vec3& pos, float w, float h) {
    const float hw = w * 0.5f;
    int x0 = static_cast<int>(std::floor(pos.x - hw + EPS)), x1 = static_cast<int>(std::floor(pos.x + hw - EPS));
    int y0 = static_cast<int>(std::floor(pos.y + EPS)), y1 = static_cast<int>(std::floor(pos.y + h - EPS));
    int z0 = static_cast<int>(std::floor(pos.z - hw + EPS)), z1 = static_cast<int>(std::floor(pos.z + hw - EPS));
    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (world.isSolidAt(x, y, z)) return true;
    return false;
}

bool moveBodyAxis(const World& world, glm::vec3& pos, glm::vec3& vel, int axis, float delta, float w, float h,
                  bool* landed) {
    const float hw = w * 0.5f;
    float remaining = delta;
    while (std::fabs(remaining) > 1e-6f) {
        float step = std::max(-0.4f, std::min(0.4f, remaining));
        remaining -= step;
        pos[axis] += step;
        if (!bodyCollides(world, pos, w, h)) continue;

        // Auf die Blockkante zurueckschieben
        if (axis == 1) {
            if (step > 0) {
                int cell = static_cast<int>(std::floor(pos.y + h - EPS));
                pos.y = static_cast<float>(cell) - h - EPS;
            } else {
                int cell = static_cast<int>(std::floor(pos.y + EPS));
                pos.y = static_cast<float>(cell + 1) + EPS;
                if (landed) *landed = true;
            }
        } else {
            if (step > 0) {
                int cell = static_cast<int>(std::floor(pos[axis] + hw - EPS));
                pos[axis] = static_cast<float>(cell) - hw - EPS;
            } else {
                int cell = static_cast<int>(std::floor(pos[axis] - hw + EPS));
                pos[axis] = static_cast<float>(cell + 1) + hw + EPS;
            }
        }
        vel[axis] = 0.0f;
        return true;
    }
    return false;
}

}  // namespace oc
