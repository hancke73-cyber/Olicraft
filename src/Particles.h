#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Chunk.h"

namespace oc {

class World;

// Einfache Partikel (Bloeckchen beim Abbauen, Spritzer, Rauch, Flammen, Blut ...), werden als Billboards gezeichnet.
struct Particle {
    glm::vec3 pos{0.0f}, vel{0.0f};
    float life = 1.0f, maxLife = 1.0f;
    float size = 0.1f;          // Kantenlaenge in Bloecken (bei Beginn)
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;  // Ausschnitt im Blockatlas
    float gravity = 12.0f;
    float drag = 0.0f;
    float shade = 1.0f;
    bool emissive = false;      // leuchtet (unabhaengig vom Licht)
    bool shrink = true;         // wird mit der Zeit kleiner
    bool collide = true;        // prallt am Gelaende ab / verschwindet darin
    float sky = 1.0f, light = 0.0f;
};

class ParticleSystem {
public:
    static constexpr size_t MAX_PARTICLES = 2000;

    void clear() { list_.clear(); }
    size_t size() const { return list_.size(); }
    const std::vector<Particle>& particles() const { return list_; }

    void update(float dt, const World& world);

    // Ausloeser
    void emitBlockBreak(const World& world, const glm::ivec3& cell, BlockId b);
    void emitSplash(const glm::vec3& pos, int count, float strength);
    void emitSmoke(const glm::vec3& pos, int count = 1);
    void emitFlame(const glm::vec3& pos);
    void emitBlood(const glm::vec3& pos, int count);
    void emitPuff(const glm::vec3& pos, int count);       // Staubwolke (Tod eines Mobs)
    void emitEmber(const glm::vec3& pos);                 // Funken ueber Lava
    void emitSpark(const glm::vec3& pos, int count);      // Aufheben, Herstellen

    // Billboards in kamerarelativen Koordinaten (4 Ecken je Partikel, gleiche Reihenfolge wie die Blockflaechen)
    void build(const glm::vec3& camPos, const glm::vec3& right, const glm::vec3& up, std::vector<Vertex>& out) const;

private:
    float rnd();
    float rnd(float a, float b) { return a + (b - a) * rnd(); }
    void add(const Particle& p);
    std::vector<Particle> list_;
    uint32_t seed_ = 0x1234567u;
};

}  // namespace oc
