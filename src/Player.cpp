#include "Player.h"

#include <algorithm>
#include <cmath>

#include "Block.h"
#include "Physics.h"
#include "World.h"

namespace oc {

namespace {
constexpr float GRAVITY = 28.0f;
constexpr float JUMP_SPEED = 8.4f;  // Sprunghoehe etwa 1.3 Bloecke: ueber 1 Block, aber nicht ueber Zaeune (1.5)
constexpr float TERMINAL = 55.0f;
constexpr float WALK = 4.3f;
constexpr float SPRINT = 5.9f;
constexpr float FLY = 11.0f;
constexpr float EPS = 1e-4f;
}  // namespace

glm::vec3 Player::front() const {
    float y = glm::radians(yaw), p = glm::radians(pitch);
    return glm::normalize(glm::vec3(std::cos(y) * std::cos(p), std::sin(p), std::sin(y) * std::cos(p)));
}

void Player::look(float dx, float dy, float sensitivity) {
    yaw += dx * sensitivity;
    pitch -= dy * sensitivity;
    pitch = std::max(-89.9f, std::min(89.9f, pitch));
    if (yaw > 360.0f || yaw < -360.0f) yaw = std::fmod(yaw, 360.0f);
}

bool Player::intersectsBlock(int x, int y, int z) const {
    const float hw = WIDTH * 0.5f;
    return pos.x + hw > x && pos.x - hw < x + 1 && pos.y + HEIGHT > y && pos.y < y + 1 && pos.z + hw > z &&
           pos.z - hw < z + 1;
}

bool Player::intersectsBoxes(int x, int y, int z, const Box* boxes, int n) const {
    const float hw = WIDTH * 0.5f;
    for (int i = 0; i < n; ++i) {
        const Box& b = boxes[i];
        if (pos.x + hw > x + b.x0 && pos.x - hw < x + b.x1 && pos.y + HEIGHT > y + b.y0 && pos.y < y + b.y1 &&
            pos.z + hw > z + b.z0 && pos.z - hw < z + b.z1)
            return true;
    }
    return false;
}

void Player::moveAxis(int axis, float delta, const World& world) {
    bool landed = false;
    moveBodyAxis(world, pos, vel, axis, delta, WIDTH, HEIGHT, &landed, axis == 1 ? 0.0f : 0.6f);
    if (landed) onGround = true;
}

bool Player::bodyInWater(const World& world) const {
    return world.inWaterAt(glm::vec3(pos.x, pos.y + 0.4f, pos.z));
}

void Player::update(float dt, const MoveInput& in, const World& world) {
    dt = std::min(dt, 0.05f);

    // Warten, bis der Chunk unter dem Spieler geladen ist
    int cx = floorDiv(static_cast<int>(std::floor(pos.x)), CHUNK_W);
    int cz = floorDiv(static_cast<int>(std::floor(pos.z)), CHUNK_W);
    if (!world.isChunkLoaded(cx, cz)) return;

    inLava = world.inLavaAt(glm::vec3(pos.x, pos.y + 0.4f, pos.z));
    inWater = bodyInWater(world) || inLava;  // Lava bremst wie Wasser, nur staerker
    boost_ = std::max(0.0f, boost_ - dt);

    // Wunsch-Geschwindigkeit in der Horizontalen
    const float yawRad = glm::radians(yaw);
    const glm::vec3 fwd(std::cos(yawRad), 0.0f, std::sin(yawRad));
    const glm::vec3 right(-std::sin(yawRad), 0.0f, std::cos(yawRad));
    glm::vec3 wish = fwd * in.forward + right * in.strafe;
    if (glm::dot(wish, wish) > 1.0f) wish = glm::normalize(wish);

    float speed = flying ? FLY : (in.sprint ? SPRINT : WALK);
    if (flying && in.sprint) speed *= 2.0f;
    if (inWater && !flying) speed *= inLava ? 0.3f : 0.6f;
    const glm::vec3 target = wish * speed;

    const float accel = flying ? 8.0f : (onGround ? 14.0f : (inWater ? 5.0f : 3.0f));
    const float k = 1.0f - std::exp(-accel * dt);
    vel.x += (target.x - vel.x) * k;
    vel.z += (target.z - vel.z) * k;

    // Leiter: an der Leiter klettern (Sprung oder Vorwaertsgehen hoch, Schleichen haelt, sonst langsam runter)
    {
        auto ladderAt = [&](float y) {
            return isLadder(world.getBlock(static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(y)), static_cast<int>(std::floor(pos.z))));
        };
        onLadder = !flying && (ladderAt(pos.y + 0.1f) || ladderAt(pos.y + 0.9f) || ladderAt(pos.y + 1.5f));
    }
    if (onLadder) {
        vel.x = std::max(-1.6f, std::min(1.6f, vel.x));
        vel.z = std::max(-1.6f, std::min(1.6f, vel.z));
        vel.y = (in.jump || in.forward > 0.1f) ? 3.2f : (in.sneak ? 0.0f : -2.4f);
    } else if (flying) {
        float ty = (in.jump ? FLY * 0.8f : 0.0f) - (in.sneak ? FLY * 0.8f : 0.0f);
        if (in.sprint) ty *= 1.5f;
        vel.y += (ty - vel.y) * (1.0f - std::exp(-8.0f * dt));
    } else if (inWater) {
        // Uferkante voraus (hoechstens ein Block ueber den Fuessen, darueber frei): mit Sprungtaste aus dem Wasser druecken
        if (in.jump && glm::dot(wish, wish) > 0.01f) {
            const glm::vec3 dir = glm::normalize(wish);
            const int fx = static_cast<int>(std::floor(pos.x + dir.x * (WIDTH * 0.5f + 0.25f)));
            const int fz = static_cast<int>(std::floor(pos.z + dir.z * (WIDTH * 0.5f + 0.25f)));
            const int fy = static_cast<int>(std::floor(pos.y + 0.1f));
            if (world.isSolidAt(fx, fy, fz) && !world.isSolidAt(fx, fy + 1, fz) && !world.isSolidAt(fx, fy + 2, fz)) {
                vel.y = std::max(vel.y, 8.0f);
                boost_ = 0.3f;
            }
        }
        if (boost_ > 0.0f) {
            vel.y -= GRAVITY * 0.6f * dt;  // Schub ueber die Kante, danach normale Wasserphysik
        } else {
            vel.y -= 6.0f * dt;
            vel.y *= std::exp((inLava ? -4.5f : -2.5f) * dt);
            if (in.jump) vel.y += (inLava ? 14.0f : 22.0f) * dt;
            vel.y = std::max(-4.0f, std::min(inLava ? 1.8f : 3.5f, vel.y));
        }
    } else {
        vel.y -= GRAVITY * dt;
        vel.y = std::max(-TERMINAL, vel.y);
        if (in.jump && onGround) vel.y = JUMP_SPEED;
    }

    const float startY = pos.y;
    onGround = false;
    headInWater = world.inWaterAt(eyePos());  // Lava ertraenkt nicht, sie verbrennt
    moveAxis(0, vel.x * dt, world);
    moveAxis(2, vel.z * dt, world);
    moveAxis(1, vel.y * dt, world);

    if (flying && onGround) flying = false;  // Landen beendet den Flug
    if (onLadder) peakY_ = pos.y;             // an der Leiter gibt es keinen Fallschaden

    // Fallhoehe fuer Fallschaden
    if (onGround) {
        if (airborne_) landedFall = std::max(landedFall, peakY_ - pos.y);
        airborne_ = false;
        peakY_ = pos.y;
    } else {
        airborne_ = true;
        peakY_ = std::max(peakY_, startY);
        if (inWater || flying) peakY_ = pos.y;  // Wasser faengt den Sturz ab
    }

    if (pos.y < -20.0f) {  // Sicherheitsnetz
        pos.y = static_cast<float>(CHUNK_H);
        vel = glm::vec3(0.0f);
    }
}

}  // namespace oc
