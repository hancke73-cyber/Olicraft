#include "Entities.h"

#include <algorithm>
#include <cmath>

#include "Atlas.h"
#include "Faces.h"
#include "Models.h"
#include "Physics.h"
#include "Player.h"
#include "World.h"

namespace oc {

namespace {

constexpr float PI = 3.14159265358979f;
constexpr float GRAVITY = 28.0f;

const MobSpec SPECS[4] = {
    {"Schwein", 0.8f, 0.85f, 10, 1.4f, false},
    {"Kuh", 0.85f, 1.45f, 10, 1.2f, false},
    {"Schaf", 0.8f, 1.25f, 8, 1.3f, false},
    {"Zombie", 0.6f, 1.9f, 20, 2.5f, true},
};

bool skyExposed(const World& w, int x, int y, int z) {
    for (int yy = y; yy < CHUNK_H; ++yy)
        if (blockInfo(w.getBlock(x, yy, z)).blocksSky) return false;
    return true;
}

glm::vec2 normalized2(glm::vec2 v, glm::vec2 fallback) {
    float l = glm::length(v);
    return l < 1e-5f ? fallback : v / l;
}

float turnToward(float cur, float target, float maxStep) {
    float d = std::fmod(target - cur + PI, 2.0f * PI);
    if (d < 0) d += 2.0f * PI;
    d -= PI;
    d = std::max(-maxStep, std::min(maxStep, d));
    return cur + d;
}

// Quader fuer Gegenstaende am Boden (Texturen aus dem Blockatlas); xf wandelt lokale Koordinaten in Kamera-Weltkoordinaten
template <class XF>
void addBox(std::vector<Vertex>& out, const glm::vec3& center, const glm::vec3& size, Tile tile, float sky, float light, XF&& xf) {
    const UVRect uv = tileUV(tile);
    for (int f = 0; f < 6; ++f) {
        const FaceDef& fd = faceDef(f);
        for (int k = 0; k < 4; ++k) {
            const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
            glm::vec3 local = center;
            for (int a = 0; a < 3; ++a) {
                float s = static_cast<float>(fd.n[a]) + (i ? 1.0f : -1.0f) * static_cast<float>(fd.t1[a]) +
                          (j ? 1.0f : -1.0f) * static_cast<float>(fd.t2[a]);
                local[a] += 0.5f * size[a] * s;
            }
            glm::vec3 p = xf(local);
            float uu, vv;
            faceUV(f, i, j, uu, vv);
            Vertex v;
            v.x = p.x;
            v.y = p.y;
            v.z = p.z;
            v.u = uv.u0 + (uv.u1 - uv.u0) * uu;
            v.v = uv.v0 + (uv.v1 - uv.v0) * vv;
            v.shade = fd.shade;
            v.sky = sky;
            v.light = light;
            out.push_back(v);
        }
    }
}

SkinId skinOf(MobType t) {
    switch (t) {
        case MobType::Pig: return SKIN_PIG;
        case MobType::Cow: return SKIN_COW;
        case MobType::Sheep: return SKIN_SHEEP;
        default: return SKIN_ZOMBIE;
    }
}

}  // namespace

const MobSpec& mobSpec(MobType t) { return SPECS[static_cast<int>(t)]; }

void EntityManager::clear() {
    items_.clear();
    mobs_.clear();
    playerHits_.clear();
}

void EntityManager::spawnItem(const ItemStack& stack, const glm::vec3& pos, const glm::vec3& vel, float pickupDelay) {
    if (stack.empty() || items_.size() > 400) return;
    ItemEntity e;
    e.pos = pos;
    e.vel = vel;
    e.stack = stack;
    e.pickupDelay = pickupDelay;
    items_.push_back(e);
}

Mob& EntityManager::spawnMob(MobType type, const glm::vec3& pos) {
    Mob m;
    m.type = type;
    m.pos = pos;
    m.health = mobSpec(type).health;
    m.yaw = rng_.range(0.0f, 2.0f * PI);
    m.wanderYaw = m.yaw;
    m.stateTimer = rng_.range(1.0f, 4.0f);
    mobs_.push_back(m);
    return mobs_.back();
}

int EntityManager::countMobs(bool hostile) const {
    int n = 0;
    for (const Mob& m : mobs_)
        if (!m.dead() && mobSpec(m.type).hostile == hostile) ++n;
    return n;
}

void EntityManager::dropLoot(const Mob& m) {
    int n = 0;
    switch (m.type) {
        case MobType::Pig:
        case MobType::Cow: n = rng_.irange(1, 3); break;
        case MobType::Sheep: n = rng_.irange(1, 2); break;
        default: n = 0; break;
    }
    if (n > 0)
        spawnItem({ITEM_MEAT, n}, m.pos + glm::vec3(0.0f, 0.5f, 0.0f),
                  glm::vec3(rng_.range(-1.5f, 1.5f), 4.0f, rng_.range(-1.5f, 1.5f)), 0.4f);
}

void EntityManager::hurtMob(Mob& m, int damage, const glm::vec3& from) {
    if (m.dead()) return;
    m.health -= damage;
    m.hurtTimer = 0.45f;
    glm::vec2 away = normalized2(glm::vec2(m.pos.x - from.x, m.pos.z - from.z),
                                 glm::vec2(std::cos(m.yaw), std::sin(m.yaw)));
    m.vel.x += away.x * 5.5f;
    m.vel.z += away.y * 5.5f;
    m.vel.y = 5.0f;
    m.onGround = false;
    if (!mobSpec(m.type).hostile) {
        m.panicTimer = 4.0f;
        m.fleeDir = away;
    }
    if (m.health <= 0) {
        m.deathTimer = 0.0f;
        dropLoot(m);
    }
}

Mob* EntityManager::raycastMob(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float& outDist) {
    Mob* best = nullptr;
    float bestT = maxDist;
    for (Mob& m : mobs_) {
        if (m.dead()) continue;
        const MobSpec& sp = mobSpec(m.type);
        const float hw = sp.width * 0.5f;
        const glm::vec3 mn(m.pos.x - hw, m.pos.y, m.pos.z - hw), mx(m.pos.x + hw, m.pos.y + sp.height, m.pos.z + hw);
        float t0 = 0.0f, t1 = bestT;
        bool hit = true;
        for (int a = 0; a < 3 && hit; ++a) {
            if (std::fabs(dir[a]) < 1e-8f) {
                if (origin[a] < mn[a] || origin[a] > mx[a]) hit = false;
            } else {
                float ta = (mn[a] - origin[a]) / dir[a], tb = (mx[a] - origin[a]) / dir[a];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                if (t0 > t1) hit = false;
            }
        }
        if (hit && t0 < bestT) {
            bestT = t0;
            best = &m;
        }
    }
    outDist = bestT;
    return best;
}

// ---------------------------------------------------------------------------------------------------------------
// Gegenstaende
// ---------------------------------------------------------------------------------------------------------------

void EntityManager::updateItem(ItemEntity& it, float dt, World& world) {
    it.age += dt;
    it.pickupDelay -= dt;
    const bool water = world.inWaterAt(it.pos + glm::vec3(0.0f, 0.15f, 0.0f));
    if (water) {
        it.vel.y -= 3.0f * dt;
        it.vel.y = std::max(-1.2f, std::min(1.5f, it.vel.y + 14.0f * dt * (it.vel.y < 0.8f ? 1.0f : 0.0f)));
        float k = std::exp(-3.0f * dt);
        it.vel.x *= k;
        it.vel.z *= k;
    } else {
        it.vel.y = std::max(-40.0f, it.vel.y - 24.0f * dt);
    }
    it.onGround = false;
    bool landed = false;
    moveBodyAxis(world, it.pos, it.vel, 0, it.vel.x * dt, 0.25f, 0.25f, nullptr);
    moveBodyAxis(world, it.pos, it.vel, 2, it.vel.z * dt, 0.25f, 0.25f, nullptr);
    moveBodyAxis(world, it.pos, it.vel, 1, it.vel.y * dt, 0.25f, 0.25f, &landed);
    it.onGround = landed;
    if (it.onGround) {
        float k = std::exp(-9.0f * dt);
        it.vel.x *= k;
        it.vel.z *= k;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Mobs
// ---------------------------------------------------------------------------------------------------------------

void EntityManager::updateMob(Mob& m, float dt, World& world, const Player& player, const EntityEnv& env) {
    const MobSpec& sp = mobSpec(m.type);
    const float hw = sp.width * 0.5f;

    if (m.dead()) {
        m.deathTimer += dt;
    } else {
        m.hurtTimer = std::max(0.0f, m.hurtTimer - dt);
        m.attackCooldown = std::max(0.0f, m.attackCooldown - dt);
        m.panicTimer = std::max(0.0f, m.panicTimer - dt);
    }
    m.inWater = world.inWaterAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f)) || world.inLavaAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f));
    if (!m.dead() && world.inLavaAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f))) {  // Lava: 4 Schaden pro Sekunde
        m.lavaTimer += dt;
        if (m.lavaTimer >= 0.5f) {
            m.lavaTimer = 0.0f;
            m.health -= 2;
            m.hurtTimer = 0.3f;
            if (m.health <= 0) m.deathTimer = 0.0f;  // ohne Beute
        }
    }

    glm::vec2 want(0.0f);
    float speed = 0.0f;
    bool lookAtTarget = false;
    float targetYaw = m.yaw;

    if (!m.dead()) {
        const glm::vec3 toPlayer = player.pos - m.pos;
        const float dist2d = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

        if (sp.hostile && !env.creative && !env.playerDead && dist2d < 26.0f && std::fabs(toPlayer.y) < 12.0f) {
            // Verfolgen
            glm::vec2 dir = normalized2(glm::vec2(toPlayer.x, toPlayer.z), glm::vec2(1, 0));
            want = dir;
            speed = sp.speed;
            lookAtTarget = true;
            targetYaw = std::atan2(dir.y, dir.x);
            if (dist2d < 1.25f && std::fabs(toPlayer.y) < 1.6f && m.attackCooldown <= 0.0f) {
                m.attackCooldown = 1.0f;
                playerHits_.push_back({3, glm::vec3(dir.x * 6.0f, 4.0f, dir.y * 6.0f)});
            }
            if (dist2d < 1.0f) speed = 0.0f;  // direkt vor dem Spieler stehen bleiben
        } else if (m.panicTimer > 0.0f) {
            want = m.fleeDir;
            speed = sp.speed * 2.2f;
            lookAtTarget = true;
            targetYaw = std::atan2(want.y, want.x);
        } else {
            m.stateTimer -= dt;
            if (m.state == Mob::Idle) {
                if (m.stateTimer <= 0.0f) {
                    m.state = Mob::Wander;
                    m.wanderYaw = rng_.range(0.0f, 2.0f * PI);
                    m.stateTimer = rng_.range(2.0f, 6.0f);
                }
            } else {
                want = glm::vec2(std::cos(m.wanderYaw), std::sin(m.wanderYaw));
                speed = sp.speed * 0.8f;
                lookAtTarget = true;
                targetYaw = m.wanderYaw;
                if (m.stateTimer <= 0.0f) {
                    m.state = Mob::Idle;
                    m.stateTimer = rng_.range(1.5f, 5.0f);
                }
            }
        }

        // Am Tag verschwinden Zombies: im Freien verbrennen sie, sonst (Hoehle, Wasser) lösen sie sich nach 10 s auf
        if (sp.hostile && env.daylight > 0.7f) {
            const bool exposed = skyExposed(world, static_cast<int>(std::floor(m.pos.x)),
                                            static_cast<int>(std::floor(m.pos.y + sp.height)),
                                            static_cast<int>(std::floor(m.pos.z)));
            if (exposed && !m.inWater) {
                m.burnTimer += dt;
                if (m.burnTimer >= 1.0f) {
                    m.burnTimer -= 1.0f;
                    m.health -= 2;
                    m.hurtTimer = 0.3f;
                    if (m.health <= 0) m.deathTimer = 0.0f;  // ohne Beute
                }
            } else {
                m.dayTimer += dt;
                if (m.dayTimer > 10.0f) m.deathTimer = 2.0f;  // wird beim naechsten Durchlauf entfernt
            }
        }
    }

    if (lookAtTarget) m.yaw = turnToward(m.yaw, targetYaw, 9.0f * dt);

    // Hindernis voraus? Eine Stufe hochspringen, sonst die Richtung wechseln
    if (!m.dead() && speed > 0.0f && m.onGround) {
        const float reach = hw + 0.3f;
        const int fx = static_cast<int>(std::floor(m.pos.x + want.x * reach));
        const int fz = static_cast<int>(std::floor(m.pos.z + want.y * reach));
        const int fy = static_cast<int>(std::floor(m.pos.y + 0.1f));
        if (world.isSolidAt(fx, fy, fz)) {
            if (!world.isSolidAt(fx, fy + 1, fz) && !world.isSolidAt(fx, fy + 2, fz)) {
                m.vel.y = 9.2f;
                m.onGround = false;
            } else if (m.state == Mob::Wander && m.panicTimer <= 0.0f) {
                m.wanderYaw += rng_.range(1.5f, 4.5f);
            }
        }
        // Wasser meiden (Wanderer)
        if (m.state == Mob::Wander && m.panicTimer <= 0.0f &&
            isLiquid(world.getBlock(fx, static_cast<int>(std::floor(m.pos.y - 0.5f)), fz)) &&
            !world.isSolidAt(fx, static_cast<int>(std::floor(m.pos.y - 1.0f)), fz))
            m.wanderYaw += rng_.range(1.5f, 4.5f);
    }

    // Geschwindigkeit
    const glm::vec2 target = want * speed;
    const float accel = m.onGround ? 10.0f : 2.5f;
    const float k = 1.0f - std::exp(-accel * dt);
    m.vel.x += (target.x - m.vel.x) * k;
    m.vel.z += (target.y - m.vel.z) * k;

    if (m.inWater) {
        m.vel.y -= 5.0f * dt;
        m.vel.y *= std::exp(-2.0f * dt);
        if (!m.dead()) m.vel.y += 18.0f * dt;  // treiben nach oben
        m.vel.y = std::max(-3.0f, std::min(2.5f, m.vel.y));
    } else {
        m.vel.y = std::max(-40.0f, m.vel.y - GRAVITY * dt);
    }

    m.onGround = false;
    bool landed = false;
    const glm::vec3 before = m.pos;
    bool hitX = moveBodyAxis(world, m.pos, m.vel, 0, m.vel.x * dt, sp.width, sp.height, nullptr);
    bool hitZ = moveBodyAxis(world, m.pos, m.vel, 2, m.vel.z * dt, sp.width, sp.height, nullptr);
    moveBodyAxis(world, m.pos, m.vel, 1, m.vel.y * dt, sp.width, sp.height, &landed);
    m.onGround = landed;

    // Festgefahren? Dann in Wanderrichtung etwas Neues versuchen
    if (speed > 0.0f && (hitX || hitZ) && glm::length(glm::vec2(m.pos.x - before.x, m.pos.z - before.z)) < 1e-3f) {
        m.stuckTimer += dt;
        if (m.stuckTimer > 0.6f && !lookAtTarget) {
            m.wanderYaw += rng_.range(1.5f, 4.5f);
            m.stuckTimer = 0.0f;
        }
    } else {
        m.stuckTimer = 0.0f;
    }

    // Laufanimation
    const float moved = glm::length(glm::vec2(m.pos.x - before.x, m.pos.z - before.z)) / std::max(dt, 1e-4f);
    const float target_amt = std::min(1.0f, moved / 1.5f);
    m.walkAmount += (target_amt - m.walkAmount) * std::min(1.0f, 8.0f * dt);
    m.walkPhase += moved * dt * (sp.hostile ? 5.5f : 7.5f);
}

// ---------------------------------------------------------------------------------------------------------------
// Spawnen
// ---------------------------------------------------------------------------------------------------------------

static bool findGround(const World& w, int x, int z, int yTop, int yBottom, int& outY) {
    if (!w.isChunkLoaded(floorDiv(x, CHUNK_W), floorDiv(z, CHUNK_W))) return false;
    yTop = std::min(yTop, CHUNK_H - 3);
    yBottom = std::max(yBottom, 1);
    for (int y = yTop; y >= yBottom; --y) {
        BlockId b = w.getBlock(x, y, z);
        if (!isSolid(b) || b == LEAVES) continue;
        if (w.getBlock(x, y + 1, z) == AIR && w.getBlock(x, y + 2, z) == AIR) {
            outY = y + 1;
            return true;
        }
        return false;  // erste feste Flaeche von oben ist verbaut
    }
    return false;
}

void EntityManager::trySpawn(World& world, const Player& player, const EntityEnv& env) {
    const bool night = env.daylight < 0.3f;
    for (int attempt = 0; attempt < 6; ++attempt) {
        // Zombies erscheinen nur nachts (draussen und in Hoehlen)
        const bool hostile = !env.creative && night && rng_.uniform() < 0.6f;
        if (hostile ? countMobs(true) >= 8 : countMobs(false) >= 12) continue;
        if (!hostile && env.daylight < 0.4f) continue;

        const float ang = rng_.range(0.0f, 2.0f * PI);
        const float dist = hostile ? rng_.range(16.0f, 34.0f) : rng_.range(18.0f, 40.0f);
        const int x = static_cast<int>(std::floor(player.pos.x + std::cos(ang) * dist));
        const int z = static_cast<int>(std::floor(player.pos.z + std::sin(ang) * dist));
        int gy = 0;
        const int py = static_cast<int>(player.pos.y);

        if (hostile) {
            const bool cave = rng_.uniform() < 0.4f;
            if (cave) {
                if (!findGround(world, x, z, py + 12, py - 30, gy)) continue;
                if (skyExposed(world, x, gy, z)) continue;  // Hoehle: kein freier Himmel
            } else {
                if (!findGround(world, x, z, CHUNK_H - 3, 1, gy)) continue;
                if (!skyExposed(world, x, gy, z)) continue;
            }
            if (isLiquid(world.getBlock(x, gy - 1, z))) continue;
            if (world.lightAt(x, gy, z) > 6) continue;  // Fackeln und Lava halten Zombies fern
            Mob& m = spawnMob(MobType::Zombie, glm::vec3(x + 0.5f, static_cast<float>(gy), z + 0.5f));
            (void)m;
            return;
        }

        if (!findGround(world, x, z, CHUNK_H - 3, 1, gy)) continue;
        if (world.getBlock(x, gy - 1, z) != GRASS || !skyExposed(world, x, gy, z)) continue;
        static const MobType kinds[3] = {MobType::Pig, MobType::Cow, MobType::Sheep};
        const MobType t = kinds[rng_.irange(0, 2)];
        const int group = rng_.irange(1, 3);
        for (int g = 0; g < group && countMobs(false) < 12; ++g) {
            const int gx = x + rng_.irange(-2, 2), gz = z + rng_.irange(-2, 2);
            int y2 = 0;
            if (!findGround(world, gx, gz, gy + 3, gy - 3, y2)) continue;
            if (world.getBlock(gx, y2 - 1, gz) != GRASS) continue;
            spawnMob(t, glm::vec3(gx + 0.5f, static_cast<float>(y2), gz + 0.5f));
        }
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------

void EntityManager::update(float dt, World& world, const Player& player, Inventory& inv, const EntityEnv& env) {
    dt = std::min(dt, 0.05f);
    const glm::vec3 pc = player.pos + glm::vec3(0.0f, 0.9f, 0.0f);

    // Gegenstaende
    for (size_t i = 0; i < items_.size();) {
        ItemEntity& it = items_[i];
        const int cx = floorDiv(static_cast<int>(std::floor(it.pos.x)), CHUNK_W);
        const int cz = floorDiv(static_cast<int>(std::floor(it.pos.z)), CHUNK_W);
        if (world.isChunkLoaded(cx, cz)) updateItem(it, dt, world);
        bool remove = it.age > 300.0f || it.pos.y < -20.0f || world.inLavaAt(it.pos + glm::vec3(0.0f, 0.1f, 0.0f));  // Lava vernichtet
        if (!remove && !env.playerDead && it.pickupDelay <= 0.0f &&
            glm::length(pc - (it.pos + glm::vec3(0.0f, 0.1f, 0.0f))) < 1.5f) {
            int left = inv.add(it.stack.id, it.stack.count);
            if (left <= 0) remove = true;
            else it.stack.count = left;
        }
        if (remove) {
            items_[i] = items_.back();
            items_.pop_back();
        } else {
            ++i;
        }
    }

    // Mobs
    for (size_t i = 0; i < mobs_.size();) {
        Mob& m = mobs_[i];
        const int cx = floorDiv(static_cast<int>(std::floor(m.pos.x)), CHUNK_W);
        const int cz = floorDiv(static_cast<int>(std::floor(m.pos.z)), CHUNK_W);
        const float dx = m.pos.x - player.pos.x, dz = m.pos.z - player.pos.z;
        bool remove = m.pos.y < -20.0f || (dx * dx + dz * dz) > 100.0f * 100.0f || (m.dead() && m.deathTimer > 1.0f);
        if (!remove && world.isChunkLoaded(cx, cz)) updateMob(m, dt, world, player, env);
        if (remove) {
            mobs_[i] = mobs_.back();
            mobs_.pop_back();
        } else {
            ++i;
        }
    }

    // Spawnen
    if (env.allowSpawns && !env.playerDead) {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.0f) {
            spawnTimer_ = 1.5f;
            trySpawn(world, player, env);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------------------------------------------

void EntityManager::buildMesh(const World& world, const glm::vec3& camPos, float time, EntityBatches& out) const {
    // Mobs
    for (const Mob& m : mobs_) {
        const MobSpec& sp = mobSpec(m.type);
        const bool flash = m.hurtTimer > 0.0f || m.dead();
        const SkinId skin = skinOf(m.type);
        const int bx = static_cast<int>(std::floor(m.pos.x)), bz = static_cast<int>(std::floor(m.pos.z));
        Pose pose;
        pose.pos = m.pos - camPos;
        pose.yaw = m.yaw;
        pose.walkPhase = m.walkPhase;
        pose.walkAmount = m.walkAmount;
        pose.time = time;
        pose.roll = m.dead() ? std::min(1.0f, m.deathTimer / 0.4f) * (PI * 0.5f) : 0.0f;
        pose.sky = skyExposed(world, bx, static_cast<int>(std::floor(m.pos.y + sp.height)), bz) ? 1.0f : 0.0f;
        pose.light = static_cast<float>(world.lightAt(bx, static_cast<int>(std::floor(m.pos.y + sp.height * 0.5f)), bz)) / MAX_LIGHT;
        appendModel(skin, pose, flash ? out.skinHurt[skin] : out.skin[skin]);
    }

    // Gegenstaende
    for (size_t idx = 0; idx < items_.size(); ++idx) {
        const ItemEntity& it = items_[idx];
        const ItemInfo& info = itemInfo(it.stack.id);
        const float spin = time * 1.6f + static_cast<float>(idx) * 1.3f;
        const float bob = 0.06f * std::sin(time * 2.4f + static_cast<float>(idx));
        const float cs = std::cos(spin), sn = std::sin(spin);
        const int bx = static_cast<int>(std::floor(it.pos.x)), bz = static_cast<int>(std::floor(it.pos.z));
        const float sky = skyExposed(world, bx, static_cast<int>(std::floor(it.pos.y + 0.3f)), bz) ? 1.0f : 0.0f;
        const float blockLight = static_cast<float>(world.lightAt(bx, static_cast<int>(std::floor(it.pos.y + 0.3f)), bz)) / MAX_LIGHT;
        const glm::vec3 o = it.pos - camPos + glm::vec3(0.0f, 0.2f + bob, 0.0f);
        auto xf = [&](glm::vec3 l) {
            float x = l.x * cs - l.z * sn, z = l.x * sn + l.z * cs;
            return glm::vec3(o.x + x, o.y + l.y, o.z + z);
        };
        if (info.isBlock && !info.flatIcon) {
            addBox(out.items, glm::vec3(0.0f), glm::vec3(0.26f), info.iconSide, sky, blockLight, xf);
        } else {
            addBox(out.items, glm::vec3(0.0f), glm::vec3(0.42f, 0.42f, 0.04f), info.iconSide, sky, blockLight, xf);
        }
    }
}

}  // namespace oc
