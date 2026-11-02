#include "Particles.h"

#include <algorithm>
#include <cmath>

#include "Atlas.h"
#include "World.h"

namespace oc {

float ParticleSystem::rnd() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return (seed_ & 0xFFFFFFu) / 16777216.0f;
}

void ParticleSystem::add(const Particle& p) {
    if (list_.size() >= MAX_PARTICLES) list_.erase(list_.begin(), list_.begin() + static_cast<long>(MAX_PARTICLES / 10));
    list_.push_back(p);
}

void ParticleSystem::update(float dt, const World& world) {
    for (size_t i = 0; i < list_.size();) {
        Particle& p = list_[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            list_[i] = list_.back();
            list_.pop_back();
            continue;
        }
        p.vel.y -= p.gravity * dt;
        if (p.drag > 0.0f) p.vel *= std::exp(-p.drag * dt);
        glm::vec3 np = p.pos + p.vel * dt;
        if (p.collide) {
            const int x = static_cast<int>(std::floor(np.x)), y = static_cast<int>(std::floor(np.y)), z = static_cast<int>(std::floor(np.z));
            if (world.isChunkLoaded(floorDiv(x, CHUNK_W), floorDiv(z, CHUNK_W)) && world.isSolidAt(x, y, z)) {
                // Auf der Oberflaeche liegen bleiben bzw. abprallen
                if (p.vel.y < 0.0f && !world.isSolidAt(x, y + 1, z)) {
                    np.y = static_cast<float>(y + 1) + 0.01f;
                    p.vel.y = 0.0f;
                    p.vel.x *= 0.5f;
                    p.vel.z *= 0.5f;
                } else {
                    np = p.pos;
                    p.vel = -p.vel * 0.2f;
                }
            }
        }
        p.pos = np;
        // Beleuchtung der Umgebung
        {
            const int x = static_cast<int>(std::floor(np.x)), y = static_cast<int>(std::floor(np.y)), z = static_cast<int>(std::floor(np.z));
            p.light = static_cast<float>(world.lightAt(x, y, z)) / static_cast<float>(MAX_LIGHT);
            float sky = 1.0f;
            for (int yy = y + 1; yy <= y + 14 && yy < CHUNK_H; ++yy)
                if (blockInfo(world.getBlock(x, yy, z)).blocksSky) {
                    sky = 0.0f;
                    break;
                }
            p.sky = sky;
        }
        ++i;
    }
}

void ParticleSystem::emitBlockBreak(const World& world, const glm::ivec3& cell, BlockId b) {
    (void)world;
    const BlockInfo& bi = blockInfo(b);
    for (int i = 0; i < 14; ++i) {
        Particle p;
        const Tile tile = rnd() < 0.3f ? bi.top : bi.side;
        const UVRect uv = tileUV(tile);
        const float su = (uv.u1 - uv.u0), sv = (uv.v1 - uv.v0);
        const float ox = std::floor(rnd(0.0f, 3.99f)) * 4.0f / 16.0f, oy = std::floor(rnd(0.0f, 3.99f)) * 4.0f / 16.0f;
        p.u0 = uv.u0 + su * ox;
        p.u1 = p.u0 + su * 0.25f;
        p.v0 = uv.v0 + sv * oy;
        p.v1 = p.v0 + sv * 0.25f;
        p.pos = glm::vec3(cell) + glm::vec3(rnd(0.1f, 0.9f), rnd(0.1f, 0.9f), rnd(0.1f, 0.9f));
        p.vel = glm::vec3(rnd(-2.2f, 2.2f), rnd(0.5f, 3.5f), rnd(-2.2f, 2.2f));
        p.size = rnd(0.10f, 0.18f);
        p.life = p.maxLife = rnd(0.5f, 1.1f);
        p.shade = rnd(0.7f, 1.0f);
        p.emissive = bi.emissive;
        add(p);
    }
}

namespace {
// Kleine Bereiche des Blockatlas als Farbquelle: Schnee (weiss), Wasser (blau), Lava (orange/gelb), Fleisch (rot)
void tileSample(Tile t, float& u0, float& v0, float& u1, float& v1, float fx, float fy) {
    const UVRect uv = tileUV(t);
    u0 = uv.u0 + (uv.u1 - uv.u0) * fx;
    v0 = uv.v0 + (uv.v1 - uv.v0) * fy;
    u1 = u0 + (uv.u1 - uv.u0) * 0.125f;
    v1 = v0 + (uv.v1 - uv.v0) * 0.125f;
}
}  // namespace

void ParticleSystem::emitSplash(const glm::vec3& pos, int count, float strength) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        tileSample(TILE_WATER, p.u0, p.v0, p.u1, p.v1, rnd(0.0f, 0.8f), rnd(0.0f, 0.8f));
        p.pos = pos + glm::vec3(rnd(-0.4f, 0.4f), 0.0f, rnd(-0.4f, 0.4f));
        p.vel = glm::vec3(rnd(-1.5f, 1.5f), rnd(2.0f, 3.0f + strength), rnd(-1.5f, 1.5f));
        p.size = rnd(0.07f, 0.13f);
        p.life = p.maxLife = rnd(0.5f, 0.9f);
        p.shade = 1.0f;
        p.collide = false;
        add(p);
    }
}

void ParticleSystem::emitSmoke(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        tileSample(TILE_SNOW, p.u0, p.v0, p.u1, p.v1, rnd(0.0f, 0.8f), rnd(0.0f, 0.8f));
        p.pos = pos + glm::vec3(rnd(-0.08f, 0.08f), 0.0f, rnd(-0.08f, 0.08f));
        p.vel = glm::vec3(rnd(-0.15f, 0.15f), rnd(0.5f, 0.9f), rnd(-0.15f, 0.15f));
        p.gravity = 0.0f;
        p.size = rnd(0.12f, 0.2f);
        p.life = p.maxLife = rnd(0.9f, 1.6f);
        p.shade = 0.35f;
        p.collide = false;
        add(p);
    }
}

void ParticleSystem::emitFlame(const glm::vec3& pos) {
    Particle p;
    tileSample(TILE_LAVA, p.u0, p.v0, p.u1, p.v1, rnd(0.0f, 0.8f), rnd(0.0f, 0.8f));
    p.pos = pos + glm::vec3(rnd(-0.04f, 0.04f), 0.0f, rnd(-0.04f, 0.04f));
    p.vel = glm::vec3(rnd(-0.05f, 0.05f), rnd(0.35f, 0.7f), rnd(-0.05f, 0.05f));
    p.gravity = 0.0f;
    p.size = rnd(0.05f, 0.09f);
    p.life = p.maxLife = rnd(0.3f, 0.55f);
    p.emissive = true;
    p.collide = false;
    add(p);
}

void ParticleSystem::emitBlood(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        tileSample(TILE_MEAT, p.u0, p.v0, p.u1, p.v1, rnd(0.2f, 0.6f), rnd(0.2f, 0.6f));
        p.pos = pos + glm::vec3(rnd(-0.25f, 0.25f), rnd(-0.2f, 0.2f), rnd(-0.25f, 0.25f));
        p.vel = glm::vec3(rnd(-2.0f, 2.0f), rnd(1.0f, 3.0f), rnd(-2.0f, 2.0f));
        p.size = rnd(0.06f, 0.11f);
        p.life = p.maxLife = rnd(0.5f, 0.9f);
        p.shade = 0.9f;
        add(p);
    }
}

void ParticleSystem::emitPuff(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        tileSample(TILE_SNOW, p.u0, p.v0, p.u1, p.v1, rnd(0.0f, 0.8f), rnd(0.0f, 0.8f));
        p.pos = pos + glm::vec3(rnd(-0.4f, 0.4f), rnd(0.0f, 0.8f), rnd(-0.4f, 0.4f));
        p.vel = glm::vec3(rnd(-0.6f, 0.6f), rnd(0.2f, 0.9f), rnd(-0.6f, 0.6f));
        p.gravity = -0.2f;
        p.drag = 1.5f;
        p.size = rnd(0.2f, 0.35f);
        p.life = p.maxLife = rnd(0.6f, 1.0f);
        p.shade = 0.8f;
        p.collide = false;
        add(p);
    }
}

void ParticleSystem::emitEmber(const glm::vec3& pos) {
    Particle p;
    tileSample(TILE_LAVA, p.u0, p.v0, p.u1, p.v1, rnd(0.0f, 0.8f), rnd(0.0f, 0.8f));
    p.pos = pos + glm::vec3(rnd(-0.4f, 0.4f), 0.0f, rnd(-0.4f, 0.4f));
    p.vel = glm::vec3(rnd(-0.3f, 0.3f), rnd(1.0f, 2.2f), rnd(-0.3f, 0.3f));
    p.gravity = 1.5f;
    p.size = rnd(0.05f, 0.09f);
    p.life = p.maxLife = rnd(0.6f, 1.2f);
    p.emissive = true;
    p.collide = false;
    add(p);
}

void ParticleSystem::emitSpark(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        tileSample(TILE_GOLD_INGOT, p.u0, p.v0, p.u1, p.v1, rnd(0.3f, 0.6f), rnd(0.3f, 0.5f));
        p.pos = pos + glm::vec3(rnd(-0.2f, 0.2f), rnd(-0.1f, 0.3f), rnd(-0.2f, 0.2f));
        p.vel = glm::vec3(rnd(-0.8f, 0.8f), rnd(0.5f, 1.8f), rnd(-0.8f, 0.8f));
        p.size = rnd(0.04f, 0.07f);
        p.life = p.maxLife = rnd(0.3f, 0.6f);
        p.emissive = true;
        p.collide = false;
        add(p);
    }
}

void ParticleSystem::build(const glm::vec3& camPos, const glm::vec3& right, const glm::vec3& up, std::vector<Vertex>& out) const {
    out.clear();
    out.reserve(list_.size() * 4);
    for (const Particle& p : list_) {
        const float k = p.shrink ? std::max(0.0f, p.life / p.maxLife) : 1.0f;
        const float h = p.size * (0.4f + 0.6f * k) * 0.5f;
        const glm::vec3 c = p.pos - camPos;
        const glm::vec3 r = right * h, u = up * h;
        const glm::vec3 corners[4] = {c - r - u, c + r - u, c + r + u, c - r + u};
        const float uu[4] = {p.u0, p.u1, p.u1, p.u0}, vv[4] = {p.v0, p.v0, p.v1, p.v1};
        for (int i = 0; i < 4; ++i) {
            Vertex v;
            v.x = corners[i].x;
            v.y = corners[i].y;
            v.z = corners[i].z;
            v.u = uu[i];
            v.v = vv[i];
            v.shade = p.emissive ? 1.0f : p.shade;
            v.sky = p.emissive ? 2.0f : p.sky;
            v.light = p.emissive ? 1.0f : p.light;
            out.push_back(v);
        }
    }
}

}  // namespace oc
