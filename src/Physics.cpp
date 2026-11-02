#include "Physics.h"

#include <algorithm>
#include <cmath>

#include "BlockShape.h"
#include "World.h"

namespace oc {

namespace {
constexpr float EPS = 1e-4f;

struct WBox {
    float lo[3], hi[3];
};

// Alle Kollisionsboxen (Weltkoordinaten), die den Koerper beruehren koennten
int gatherBoxes(const World& world, const glm::vec3& pos, float w, float h, WBox* out, int maxOut) {
    const float hw = w * 0.5f;
    const int x0 = static_cast<int>(std::floor(pos.x - hw + EPS)), x1 = static_cast<int>(std::floor(pos.x + hw - EPS));
    const int y0 = static_cast<int>(std::floor(pos.y + EPS)) - 1, y1 = static_cast<int>(std::floor(pos.y + h - EPS));  // eine Zeile tiefer: Zaeune ragen hoch
    const int z0 = static_cast<int>(std::floor(pos.z - hw + EPS)), z1 = static_cast<int>(std::floor(pos.z + hw - EPS));
    int n = 0;
    Box tmp[MAX_BOXES];
    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) {
                const int cnt = world.collisionBoxes(x, y, z, tmp);
                for (int i = 0; i < cnt && n < maxOut; ++i) {
                    const Box& b = tmp[i];
                    WBox wb{{x + b.x0, y + b.y0, z + b.z0}, {x + b.x1, y + b.y1, z + b.z1}};
                    // ueberschneidet der Koerper die Box wirklich?
                    if (pos.x + hw - EPS > wb.lo[0] && pos.x - hw + EPS < wb.hi[0] && pos.y + h - EPS > wb.lo[1] &&
                        pos.y + EPS < wb.hi[1] && pos.z + hw - EPS > wb.lo[2] && pos.z - hw + EPS < wb.hi[2])
                        out[n++] = wb;
                }
            }
    return n;
}
}  // namespace

bool bodyCollides(const World& world, const glm::vec3& pos, float w, float h) {
    WBox boxes[8];
    return gatherBoxes(world, pos, w, h, boxes, 8) > 0;
}

bool moveBodyAxis(const World& world, glm::vec3& pos, glm::vec3& vel, int axis, float delta, float w, float h,
                  bool* landed, float stepUp) {
    const float hw = w * 0.5f;
    float remaining = delta;
    while (std::fabs(remaining) > 1e-6f) {
        float step = std::max(-0.4f, std::min(0.4f, remaining));
        remaining -= step;
        const glm::vec3 before = pos;
        pos[axis] += step;
        WBox boxes[48];
        const int n = gatherBoxes(world, pos, w, h, boxes, 48);
        if (n == 0) continue;

        // Waagerechte Bewegung gegen eine niedrige Kante (Platte, Treppenstufe): hinaufsteigen
        if (axis != 1 && stepUp > 0.0f) {
            glm::vec3 probe = before;  // stand der Koerper vor dem Schritt auf etwas?
            probe.y -= 0.06f;
            glm::vec3 raised = pos;
            raised.y += stepUp;
            WBox tmp[8];
            const bool supported = gatherBoxes(world, probe, w, h, tmp, 8) > 0;  // steht auf etwas
            if (supported && gatherBoxes(world, raised, w, h, tmp, 8) == 0) {
                glm::vec3 p = raised;
                for (float drop = 0.0f; drop < stepUp; drop += 0.02f) {
                    glm::vec3 lower = p;
                    lower.y -= 0.02f;
                    if (gatherBoxes(world, lower, w, h, tmp, 8) > 0) break;
                    p = lower;
                }
                pos = p;
                continue;
            }
        }

        // Auf die naechste Boxkante zurueckschieben
        if (axis == 1) {
            if (step > 0) {
                float lim = 1e9f;
                for (int i = 0; i < n; ++i) lim = std::min(lim, boxes[i].lo[1]);
                pos.y = lim - h - EPS;
            } else {
                float lim = -1e9f;
                for (int i = 0; i < n; ++i) lim = std::max(lim, boxes[i].hi[1]);
                pos.y = lim + EPS;
                if (landed) *landed = true;
            }
        } else {
            if (step > 0) {
                float lim = 1e9f;
                for (int i = 0; i < n; ++i) lim = std::min(lim, boxes[i].lo[axis]);
                pos[axis] = lim - hw - EPS;
            } else {
                float lim = -1e9f;
                for (int i = 0; i < n; ++i) lim = std::max(lim, boxes[i].hi[axis]);
                pos[axis] = lim + hw + EPS;
            }
        }
        vel[axis] = 0.0f;
        return true;
    }
    return false;
}

}  // namespace oc
