#include "Entities.h"

#include <algorithm>
#include <cmath>

#include "Atlas.h"
#include "Faces.h"
#include "Models.h"
#include "Physics.h"
#include "Player.h"
#include "Trades.h"
#include "Village.h"
#include "World.h"

namespace oc {

namespace {

constexpr float PI = 3.14159265358979f;
constexpr float GRAVITY = 28.0f;

const MobSpec SPECS[5] = {
    {"Schwein", 0.8f, 0.85f, 10, 1.4f, false},
    {"Kuh", 0.85f, 1.45f, 10, 1.2f, false},
    {"Schaf", 0.8f, 1.25f, 8, 1.3f, false},
    {"Zombie", 0.6f, 1.9f, 20, 2.5f, true},
    {"Dorfbewohner", 0.6f, 1.9f, 20, 1.3f, false},
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

// Naechste Uferrichtung (Winkel), wenn man im Wasser steckt: nahe feste Zelle mit zwei freien Zellen darueber und ohne Wasser
bool shoreDirection(const World& w, const glm::vec3& pos, float& angOut) {
    const int by = static_cast<int>(std::floor(pos.y));
    for (int r = 1; r <= 9; ++r)
        for (int k = 0; k < 16; ++k) {
            const float ang = static_cast<float>(k) * PI / 8.0f;
            const int x = static_cast<int>(std::floor(pos.x + std::cos(ang) * static_cast<float>(r)));
            const int z = static_cast<int>(std::floor(pos.z + std::sin(ang) * static_cast<float>(r)));
            for (int dy = -1; dy <= 2; ++dy) {
                const int y = by + dy;
                if (w.isSolidAt(x, y, z) && !w.isSolidAt(x, y + 1, z) && !w.isSolidAt(x, y + 2, z) && !isLiquid(w.getBlock(x, y + 1, z)) &&
                    !isLiquid(w.getBlock(x, y + 2, z))) {
                    angOut = ang;
                    return true;
                }
            }
        }
    return false;
}

// A*-Suche ueber begehbare Zellen (Tueren und Tore zaehlen als begehbar, Dorfbewohner oeffnen sie).
std::vector<glm::ivec3> findPathCells(const World& w, const glm::ivec3& start, const glm::ivec3& goal, int radius) {
    auto freeCell = [&](int x, int y, int z) {
        const BlockId b = w.getBlock(x, y, z);
        if (b == AIR) return true;
        if (isLiquid(b)) return false;
        const BlockInfo& bi = blockInfo(b);
        return !bi.solid || isDoor(b) || isGate(b);
    };
    auto standable = [&](int x, int y, int z) {
        if (y < 1 || y >= CHUNK_H - 2) return false;
        if (!freeCell(x, y, z) || !freeCell(x, y + 1, z)) return false;
        const BlockId below = w.getBlock(x, y - 1, z);
        return blockInfo(below).solid && !isDoor(below) && !isGate(below) && !isFence(below) && !isWall(below);
    };
    struct Node {
        glm::ivec3 p;
        int g;
        int parent;
    };
    std::vector<Node> nodes;
    std::vector<std::pair<int, int>> open;  // (f, index)
    auto key = [](const glm::ivec3& p) { return static_cast<int64_t>((static_cast<uint64_t>(static_cast<int64_t>(p.x)) << 40) ^ (static_cast<uint64_t>(static_cast<int64_t>(p.z)) << 12) ^ static_cast<uint64_t>(static_cast<int64_t>(p.y))); };
    std::vector<int64_t> closedKeys;
    auto isClosed = [&](const glm::ivec3& p) {
        const int64_t k = key(p);
        for (int64_t c : closedKeys)
            if (c == k) return true;
        return false;
    };
    auto h = [&](const glm::ivec3& p) { return std::abs(p.x - goal.x) + std::abs(p.z - goal.z) + std::abs(p.y - goal.y); };
    nodes.push_back({start, 0, -1});
    open.push_back({h(start), 0});
    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int expanded = 0;
    while (!open.empty() && expanded < 5000) {
        size_t bi = 0;
        for (size_t i = 1; i < open.size(); ++i)
            if (open[i].first < open[bi].first) bi = i;
        const int idx = open[bi].second;
        open[bi] = open.back();
        open.pop_back();
        const glm::ivec3 cur = nodes[static_cast<size_t>(idx)].p;
        if (isClosed(cur)) continue;
        closedKeys.push_back(key(cur));
        ++expanded;
        if (cur == goal) {
            std::vector<glm::ivec3> out;
            for (int i = idx; i >= 0; i = nodes[static_cast<size_t>(i)].parent) out.push_back(nodes[static_cast<size_t>(i)].p);
            std::reverse(out.begin(), out.end());
            return out;
        }
        for (const auto& d : D) {
            const int nx = cur.x + d[0], nz = cur.z + d[1];
            if (std::abs(nx - goal.x) > radius + std::abs(start.x - goal.x) || std::abs(nz - goal.z) > radius + std::abs(start.z - goal.z)) continue;
            for (int dy : {0, 1, -1}) {
                const int ny = cur.y + dy;
                if (!standable(nx, ny, nz)) continue;
                if (dy == 1 && !freeCell(cur.x, cur.y + 2, cur.z)) continue;  // Platz zum Hochsteigen
                const glm::ivec3 np(nx, ny, nz);
                if (isClosed(np)) continue;
                const int g = nodes[static_cast<size_t>(idx)].g + 1 + (dy != 0 ? 1 : 0);
                nodes.push_back({np, g, idx});
                open.push_back({g + h(np), static_cast<int>(nodes.size()) - 1});
                break;  // eine passende Hoehe je Richtung
            }
        }
    }
    return {};
}

SkinId skinOf(MobType t) {
    switch (t) {
        case MobType::Pig: return SKIN_PIG;
        case MobType::Cow: return SKIN_COW;
        case MobType::Sheep: return SKIN_SHEEP;
        case MobType::Villager: return SKIN_VILLAGER;
        default: return SKIN_ZOMBIE;
    }
}

}  // namespace

const MobSpec& mobSpec(MobType t) { return SPECS[static_cast<int>(t)]; }

void EntityManager::clear() {
    items_.clear();
    mobs_.clear();
    playerHits_.clear();
    soundEvents_.clear();
    effects_.clear();
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
    m.idleSoundTimer = rng_.range(2.0f, 12.0f);
    mobs_.push_back(m);
    return mobs_.back();
}

int EntityManager::countMobs(bool hostile) const {
    int n = 0;
    for (const Mob& m : mobs_)
        if (!m.dead() && m.type != MobType::Villager && mobSpec(m.type).hostile == hostile) ++n;
    return n;
}

void EntityManager::emitMobSound(const Mob& m, int kind) {
    if (kind == 1 && effects_.size() < 64) effects_.push_back({0, m.pos + glm::vec3(0.0f, mobSpec(m.type).height * 0.6f, 0.0f)});
    if (kind == 2 && effects_.size() < 64) effects_.push_back({1, m.pos + glm::vec3(0.0f, 0.1f, 0.0f)});
    if (soundEvents_.size() > 64) return;
    static const SoundId ids[5][4] = {
        {SoundId::PigIdle, SoundId::PigHurt, SoundId::PigDeath, SoundId::PigIdle},
        {SoundId::CowIdle, SoundId::CowHurt, SoundId::CowDeath, SoundId::CowIdle},
        {SoundId::SheepIdle, SoundId::SheepHurt, SoundId::SheepDeath, SoundId::SheepIdle},
        {SoundId::ZombieIdle, SoundId::ZombieHurt, SoundId::ZombieDeath, SoundId::ZombieAttack},
        {SoundId::VillagerIdle, SoundId::VillagerHurt, SoundId::VillagerDeath, SoundId::VillagerIdle}};
    const float pitch = rng_.range(0.92f, 1.08f);
    soundEvents_.push_back({ids[static_cast<int>(m.type)][kind], m.pos.x, m.pos.y + mobSpec(m.type).height * 0.8f, m.pos.z,
                            1.0f, pitch});
}

void EntityManager::dropLoot(const Mob& m) {
    int n = 0;
    switch (m.type) {
        case MobType::Pig: n = rng_.irange(1, 3); break;
        case MobType::Cow:
            n = rng_.irange(1, 3);
            if (rng_.irange(0, 2) > 0)
                spawnItem({ITEM_LEATHER, rng_.irange(1, 2)}, m.pos + glm::vec3(0.0f, 0.6f, 0.0f),
                          glm::vec3(rng_.range(-1.5f, 1.5f), 4.0f, rng_.range(-1.5f, 1.5f)), 0.4f);
            break;
        case MobType::Sheep:
            n = rng_.irange(1, 2);
            spawnItem({WOOL_WHITE, rng_.irange(1, 2)}, m.pos + glm::vec3(0.0f, 0.6f, 0.0f),
                      glm::vec3(rng_.range(-1.5f, 1.5f), 4.0f, rng_.range(-1.5f, 1.5f)), 0.4f);
            break;
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
    m.sleeping = false;
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
        emitMobSound(m, 2);
    } else {
        emitMobSound(m, 1);
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
    const World::MechFlow flow = world.mechFlow(it.pos);
    if (flow.lift > 0.0f) it.vel.y = flow.lift;  // Dampflift traegt Gegenstaende nach oben
    it.onGround = false;
    bool landed = false;
    moveBodyAxis(world, it.pos, it.vel, 0, (it.vel.x + flow.belt.x) * dt, 0.25f, 0.25f, nullptr);
    moveBodyAxis(world, it.pos, it.vel, 2, (it.vel.z + flow.belt.z) * dt, 0.25f, 0.25f, nullptr);
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
        m.idleSoundTimer -= dt;
        if (m.idleSoundTimer <= 0.0f) {
            m.idleSoundTimer = rng_.range(sp.hostile ? 5.0f : 7.0f, sp.hostile ? 14.0f : 20.0f);
            const glm::vec3 d = player.pos - m.pos;
            if (!env.playerDead && d.x * d.x + d.y * d.y + d.z * d.z < 28.0f * 28.0f) emitMobSound(m, 0);
        }
    }
    m.inWater = world.inWaterAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f)) || world.inLavaAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f));
    if (!m.dead() && world.inLavaAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f))) {  // Lava: 4 Schaden pro Sekunde
        m.lavaTimer += dt;
        if (m.lavaTimer >= 0.5f) {
            m.lavaTimer = 0.0f;
            m.health -= 2;
            m.hurtTimer = 0.3f;
            if (m.health <= 0) {
                m.deathTimer = 0.0f;  // ohne Beute
                emitMobSound(m, 2);
            } else {
                emitMobSound(m, 1);
            }
        }
    }

    if (m.sleeping && !m.dead()) {  // schlaeft im Bett: bleibt liegen, steht am Morgen auf
        if (env.daylight > 0.5f && m.village && m.bedIndex >= 0 && m.bedIndex < static_cast<int>(m.village->beds.size())) {
            const Village::Bed& bed = m.village->beds[static_cast<size_t>(m.bedIndex)];
            m.sleeping = false;
            m.route = 10;
            m.pathValid = false;
            m.pos = glm::vec3(static_cast<float>(bed.head.x) + 0.5f, static_cast<float>(bed.head.y) + 0.6f, static_cast<float>(bed.head.z) + 0.5f);
        } else {
            m.vel = glm::vec3(0.0f);
            m.walkAmount = 0.0f;
            m.yaw = m.sleepYaw;
            return;
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
                emitMobSound(m, 3);
                playerHits_.push_back({3, glm::vec3(dir.x * 6.0f, 4.0f, dir.y * 6.0f)});
            }
            if (dist2d < 1.0f) speed = 0.0f;  // direkt vor dem Spieler stehen bleiben
        } else if (m.inWater && !sp.hostile && !m.sleeping && !world.inLavaAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f))) {
            // Im Wasser: zum naechsten Ufer schwimmen und hinausspringen
            float ang;
            if (shoreDirection(world, m.pos, ang)) {
                want = glm::vec2(std::cos(ang), std::sin(ang));
                speed = sp.speed * 1.7f;
                lookAtTarget = true;
                targetYaw = ang;
            }
        } else if (m.panicTimer > 0.0f) {
            want = m.fleeDir;
            speed = sp.speed * 2.2f;
            lookAtTarget = true;
            targetYaw = std::atan2(want.y, want.x);
        } else if (m.type == MobType::Villager && villagerBrain(m, dt, world, env, want, speed, lookAtTarget, targetYaw)) {
            // Tagesablauf: Arbeit, nach Hause, schlafen
        } else {
            m.stateTimer -= dt;
            if (m.state == Mob::Idle) {
                if (m.stateTimer <= 0.0f) {
                    m.state = Mob::Wander;
                    m.wanderYaw = rng_.range(0.0f, 2.0f * PI);
                    m.stateTimer = rng_.range(2.0f, 6.0f);
                }
            } else {
                if (m.hasHome) {  // Dorfbewohner bleiben im Dorf bzw. bei ihrem Arbeitsplatz
                    const float hx = m.anchorX + 0.5f - m.pos.x, hz = m.anchorZ + 0.5f - m.pos.z;
                    if (hx * hx + hz * hz > m.anchorR * m.anchorR) m.wanderYaw = std::atan2(hz, hx);
                }
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
    if (m.type == MobType::Villager && speed > 0.0f && !m.dead()) openDoorsAhead(m, world, want);

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
    // Feindliche Mobs treiben nicht: sie sinken im Wasser und gehen am Grund (Lava traegt weiterhin)
    const bool sinking = sp.hostile && m.inWater && !world.inLavaAt(m.pos + glm::vec3(0.0f, 0.3f, 0.0f));
    if (sinking) speed *= 0.6f;
    const glm::vec2 target = want * speed;
    const float accel = m.onGround ? 10.0f : 2.5f;
    const float k = 1.0f - std::exp(-accel * dt);
    m.vel.x += (target.x - m.vel.x) * k;
    m.vel.z += (target.y - m.vel.z) * k;

    if (sinking) {
        m.vel.y = std::max(-3.0f, m.vel.y - GRAVITY * dt);
    } else if (m.inWater) {
        m.vel.y -= 5.0f * dt;
        m.vel.y *= std::exp(-2.0f * dt);
        if (!m.dead()) m.vel.y += 18.0f * dt;  // treiben nach oben
        m.vel.y = std::max(-3.0f, std::min(2.5f, m.vel.y));
        if (!m.dead() && !sp.hostile && speed > 0.0f) {  // Kante voraus: hinausspringen
            const int fx = static_cast<int>(std::floor(m.pos.x + want.x * (hw + 0.35f)));
            const int fz = static_cast<int>(std::floor(m.pos.z + want.y * (hw + 0.35f)));
            const int fy = static_cast<int>(std::floor(m.pos.y + 0.1f));
            if (world.isSolidAt(fx, fy, fz) && !world.isSolidAt(fx, fy + 1, fz) && !world.isSolidAt(fx, fy + 2, fz)) m.vel.y = 8.5f;
        }
    } else {
        m.vel.y = std::max(-40.0f, m.vel.y - GRAVITY * dt);
    }

    m.onGround = false;
    bool landed = false;
    const glm::vec3 before = m.pos;
    const World::MechFlow flow = world.mechFlow(m.pos);
    if (flow.lift > 0.0f && !m.dead()) m.vel.y = flow.lift;
    bool hitX = moveBodyAxis(world, m.pos, m.vel, 0, (m.vel.x + flow.belt.x) * dt, sp.width, sp.height, nullptr, 0.6f);
    bool hitZ = moveBodyAxis(world, m.pos, m.vel, 2, (m.vel.z + flow.belt.z) * dt, sp.width, sp.height, nullptr, 0.6f);
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
        if (!isSolid(b) || isLeaves(b)) continue;
        auto freeCell = [&](int yy) {
            const BlockId f = w.getBlock(x, yy, z);
            return f == AIR || isPlant(f);
        };
        if (freeCell(y + 1) && freeCell(y + 2)) {
            outY = y + 1;
            return true;
        }
        return false;  // erste feste Flaeche von oben ist verbaut
    }
    return false;
}

// Folgt (und berechnet bei Bedarf) einen Weg zur Zielzelle. true: angekommen.
bool EntityManager::followPath(Mob& m, World& world, const glm::ivec3& goal, float arrive, glm::vec2& want, float& speed, bool& look, float& yaw, float dt) {
    const glm::ivec3 here(static_cast<int>(std::floor(m.pos.x)), static_cast<int>(std::floor(m.pos.y + 0.05f)), static_cast<int>(std::floor(m.pos.z)));
    const float gx = static_cast<float>(goal.x) + 0.5f, gz = static_cast<float>(goal.z) + 0.5f;
    if (std::hypot(gx - m.pos.x, gz - m.pos.z) < arrive && std::abs(here.y - goal.y) <= 1) return true;
    // Festhaengen erkennen
    m.pathTimer += dt;
    if (glm::length(m.pos - m.pathLastPos) > 0.4f) {
        m.pathLastPos = m.pos;
        m.pathTimer = 0.0f;
    }
    if (!m.pathValid || m.pathGoal != goal || m.pathTimer > 4.0f) {
        m.path = findPathCells(world, here, goal, 14);
        m.pathIdx = 0;
        m.pathGoal = goal;
        m.pathValid = true;
        m.pathTimer = 0.0f;
        m.pathLastPos = m.pos;
    }
    glm::vec2 target(gx, gz);
    if (!m.path.empty()) {
        while (m.pathIdx + 1 < m.path.size()) {
            const glm::ivec3& c = m.path[m.pathIdx];
            if (std::hypot(static_cast<float>(c.x) + 0.5f - m.pos.x, static_cast<float>(c.z) + 0.5f - m.pos.z) < 0.5f) ++m.pathIdx;
            else break;
        }
        const glm::ivec3& c = m.path[m.pathIdx];
        target = glm::vec2(static_cast<float>(c.x) + 0.5f, static_cast<float>(c.z) + 0.5f);
    }
    const float dx = target.x - m.pos.x, dz = target.y - m.pos.z;
    const float d = std::sqrt(dx * dx + dz * dz);
    if (d < 1e-3f) return false;
    want = glm::vec2(dx / d, dz / d);
    speed = mobSpec(m.type).speed;
    look = true;
    yaw = std::atan2(dz, dx);
    return false;
}

// Tagesablauf eines Dorfbewohners: tagsueber am Arbeitsplatz, abends ins Haus, nachts im Bett.
// Rueckgabe true: Ziel gesetzt (sonst laeuft er normal umher, im Bereich um den Arbeitsplatz).
bool EntityManager::villagerBrain(Mob& m, float dt, World& world, const EntityEnv& env, glm::vec2& want, float& speed, bool& look, float& yaw) {
    (void)dt;
    if (!m.village) return false;
    const Village& v = *m.village;
    const MobSpec& sp = mobSpec(m.type);
    const float day = env.daylight;
    const bool hasBed = m.bedIndex >= 0 && m.bedIndex < static_cast<int>(v.beds.size());
    // 0 Arbeit, 1 Heimweg (noch wach), 2 Schlafen
    int sch = 0;
    if (hasBed) {
        if (m.sleeping) sch = day < 0.5f ? 2 : 0;
        else sch = day > 0.5f ? 0 : (day > 0.28f ? 1 : 2);
    }
    const Village::Bed* bed = hasBed ? &v.beds[static_cast<size_t>(m.bedIndex)] : nullptr;
    const VillageBuilding* house = bed ? &v.buildings[static_cast<size_t>(bed->building)] : nullptr;
    auto goTo = [&](float tx, float tz, float arrive, float spd) {  // geradeaus (kurze Strecken)
        const float dx = tx - m.pos.x, dz = tz - m.pos.z;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (d < arrive) return true;
        want = glm::vec2(dx / d, dz / d);
        speed = spd;
        look = true;
        yaw = std::atan2(dz, dx);
        return false;
    };
    auto walkTo = [&](const glm::ivec3& goal, float arrive) { return followPath(m, world, goal, arrive, want, speed, look, yaw, dt); };

    if (sch == 0) {
        if (m.sleeping) {  // aufwachen: neben dem Bett aufstehen und das Haus verlassen
            m.sleeping = false;
            m.route = 10;
            m.pos = glm::vec3(static_cast<float>(bed->head.x) + 0.5f, static_cast<float>(bed->head.y) + 0.6f, static_cast<float>(bed->head.z) + 0.5f);
        }
        if (m.route >= 10 && house) {  // Haus verlassen: bis vor die Tuer
            if (walkTo(glm::ivec3(house->door.x, house->groundY + 1, house->door.y), 0.6f)) m.route = 0;
            return true;
        }
        m.route = 0;
        const glm::ivec3 ws = v.workSpot(m.profession);
        m.anchorX = static_cast<float>(ws.x);
        m.anchorZ = static_cast<float>(ws.z);
        m.anchorR = 5.0f;
        const float dx = static_cast<float>(ws.x) + 0.5f - m.pos.x, dz = static_cast<float>(ws.z) + 0.5f - m.pos.z;
        if (dx * dx + dz * dz > 6.0f * 6.0f) {
            walkTo(ws, 1.5f);
            return true;
        }
        return false;  // beim Arbeitsplatz umherlaufen
    }

    // Heimweg und Schlafen: Weg bis in die Zelle hinter der Tuer, dann zum Bettkopf
    if (m.route >= 10) m.route = 0;
    if (m.route < 3) {
        const glm::ivec2 in = house->cell(1, 0);
        if (walkTo(glm::ivec3(in.x, house->groundY + 1, in.y), 0.5f)) m.route = 3;
        return true;
    }
    const float bx = static_cast<float>(bed->head.x) + 0.5f, bz = static_cast<float>(bed->head.z) + 0.5f;
    if (!goTo(bx, bz, 0.5f, sp.speed * 0.8f)) return true;
    if (sch == 2) {  // hinlegen: quer zur Blickrichtung, Fuesse am Fussteil
        m.sleeping = true;
        const float dx = static_cast<float>(bed->head.x - bed->foot.x), dz = static_cast<float>(bed->head.z - bed->foot.z);
        m.sleepYaw = std::atan2(-dx, dz);
        m.pos = glm::vec3(static_cast<float>(bed->foot.x) + 0.5f - dx * 0.2f, static_cast<float>(bed->foot.y) + 9.0f / 16.0f + 0.32f,
                          static_cast<float>(bed->foot.z) + 0.5f - dz * 0.2f);
        m.vel = glm::vec3(0.0f);
        (void)world;
        return true;
    }
    speed = 0.0f;  // zu Hause wach: stehen bleiben
    return true;
}

// Tueren und Tore vor dem Dorfbewohner oeffnen (sie schliessen sich spaeter wieder)
void EntityManager::openDoorsAhead(Mob& m, World& world, const glm::vec2& want) {
    const float len = std::sqrt(want.x * want.x + want.y * want.y);
    if (len < 1e-3f) return;
    const float hw = mobSpec(m.type).width * 0.5f;
    for (float reach : {hw + 0.25f, hw + 0.9f}) {
        const int x = static_cast<int>(std::floor(m.pos.x + want.x / len * reach));
        const int z = static_cast<int>(std::floor(m.pos.z + want.y / len * reach));
        for (int dy = 0; dy <= 1; ++dy) {
            const int y = static_cast<int>(std::floor(m.pos.y + 0.1f)) + dy;
            const BlockId b = world.getBlock(x, y, z);
            if ((isDoor(b) && !doorOpen(b)) || (isGate(b) && !gateOpen(b))) {
                if (world.toggleOpenGroup(x, y, z) > 0) {
                    openedDoors_.push_back({glm::ivec3(x, y, z), 3.0f});
                    return;
                }
            }
        }
    }
}

void EntityManager::updateOpenedDoors(float dt, World& world, const Player& player) {
    for (size_t i = 0; i < openedDoors_.size();) {
        OpenedDoor& d = openedDoors_[i];
        d.timer -= dt;
        bool remove = false;
        if (d.timer <= 0.0f) {
            // nur schliessen, wenn niemand in der Tuer steht
            bool blocked = false;
            const glm::vec3 c(static_cast<float>(d.pos.x) + 0.5f, static_cast<float>(d.pos.y), static_cast<float>(d.pos.z) + 0.5f);
            auto near = [&](const glm::vec3& p) { return std::fabs(p.x - c.x) < 1.1f && std::fabs(p.z - c.z) < 1.1f && p.y > c.y - 1.8f && p.y < c.y + 2.0f; };
            if (near(player.pos)) blocked = true;
            for (const Mob& o : mobs_) blocked = blocked || near(o.pos);
            const BlockId b = world.getBlock(d.pos.x, d.pos.y, d.pos.z);
            if (!(isDoor(b) || isGate(b))) remove = true;
            else if (!blocked) {
                if ((isDoor(b) && doorOpen(b)) || (isGate(b) && gateOpen(b))) world.toggleOpenGroup(d.pos.x, d.pos.y, d.pos.z);
                remove = true;
            } else {
                d.timer = 1.0f;
            }
        }
        if (remove) {
            openedDoors_[i] = openedDoors_.back();
            openedDoors_.pop_back();
        } else {
            ++i;
        }
    }
}

void EntityManager::spawnVillagers(World& world, const Player& player) {
    const int px = static_cast<int>(std::floor(player.pos.x)), pz = static_cast<int>(std::floor(player.pos.z));
    for (const Village* v : world.terrain().villages().villagesIn(px - 56, pz - 56, px + 56, pz + 56)) {
        const uint32_t id = static_cast<uint32_t>(v->id);
        if (std::find(villageDone_.begin(), villageDone_.end(), id) != villageDone_.end()) continue;
        // Alle Spawnpunkte muessen geladen sein, damit die Bewohner nicht in der Luft erscheinen
        bool ready = true;
        for (const glm::ivec3& sp : v->villagerSpawns)
            ready = ready && world.isChunkLoaded(floorDiv(sp.x, CHUNK_W), floorDiv(sp.z, CHUNK_W));
        for (const Village::AnimalSpawn& a : v->animalSpawns)
            ready = ready && world.isChunkLoaded(floorDiv(a.pos.x, CHUNK_W), floorDiv(a.pos.z, CHUNK_W));
        if (!ready) continue;
        villageDone_.push_back(id);
        int index = 0;
        for (const glm::ivec3& sp : v->villagerSpawns) {
            int y = sp.y;
            while (y > 1 && !isSolid(world.getBlock(sp.x, y - 1, sp.z))) --y;  // auf den Boden setzen
            Mob& m = spawnMob(MobType::Villager, glm::vec3(sp.x + 0.5f, static_cast<float>(y), sp.z + 0.5f));
            m.hasHome = true;
            m.homeX = v->center.x;
            m.homeZ = v->center.y;
            m.villageId = v->id;
            m.profession = professionFor(v->id, index);
            m.village = v;
            m.bedIndex = v->beds.empty() ? -1 : index % static_cast<int>(v->beds.size());
            m.anchorX = static_cast<float>(v->center.x);
            m.anchorZ = static_cast<float>(v->center.y);
            ++index;
        }
        // Tiere im Stall
        for (const Village::AnimalSpawn& a : v->animalSpawns) {
            static const MobType kinds[3] = {MobType::Pig, MobType::Cow, MobType::Sheep};
            spawnMob(kinds[a.type % 3], glm::vec3(a.pos.x + 0.5f, static_cast<float>(a.pos.y), a.pos.z + 0.5f));
        }
    }
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
            const int before = it.stack.count;
            int left = inv.add(it.stack.id, it.stack.count);
            if (left < before && soundEvents_.size() <= 64)
                soundEvents_.push_back({SoundId::ItemPickup, pc.x, pc.y, pc.z, 0.6f, rng_.range(0.9f, 1.3f)});
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
        bool remove = m.pos.y < -20.0f || ((dx * dx + dz * dz) > 100.0f * 100.0f && m.type != MobType::Villager) || (m.dead() && m.deathTimer > 1.0f);
        if (!remove && world.isChunkLoaded(cx, cz)) updateMob(m, dt, world, player, env);
        if (remove) {
            mobs_[i] = mobs_.back();
            mobs_.pop_back();
        } else {
            ++i;
        }
    }

    updateOpenedDoors(dt, world, player);

    // Dorfbewohner
    if (env.allowSpawns) {
        villageTimer_ -= dt;
        if (villageTimer_ <= 0.0f) {
            villageTimer_ = 2.0f;
            spawnVillagers(world, player);
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
        const SkinId skin = m.type == MobType::Villager ? villagerSkinFor(m.profession) : skinOf(m.type);
        const int bx = static_cast<int>(std::floor(m.pos.x)), bz = static_cast<int>(std::floor(m.pos.z));
        Pose pose;
        pose.pos = m.pos - camPos;
        pose.yaw = m.yaw;
        pose.walkPhase = m.walkPhase;
        pose.walkAmount = m.walkAmount;
        pose.time = time;
        pose.roll = m.dead() ? std::min(1.0f, m.deathTimer / 0.4f) * (PI * 0.5f) : (m.sleeping ? PI * 0.5f : 0.0f);
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
