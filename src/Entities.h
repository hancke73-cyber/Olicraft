#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Chunk.h"
#include "Inventory.h"
#include "Items.h"

namespace oc {

class World;
class Player;

// Einfacher, schneller Zufallsgenerator fuer Spielmechanik
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 0x9E3779B9u) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uniform() { return (next() & 0xFFFFFFu) / 16777216.0f; }
    float range(float a, float b) { return a + (b - a) * uniform(); }
    int irange(int a, int b) { return a + static_cast<int>(next() % static_cast<uint32_t>(b - a + 1)); }
};

struct ItemEntity {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    ItemStack stack;
    float age = 0.0f;
    float pickupDelay = 0.5f;
    bool onGround = false;
};

enum class MobType : uint8_t { Pig, Cow, Sheep, Zombie };

struct MobSpec {
    const char* name;
    float width, height;
    int health;
    float speed;
    bool hostile;
};
const MobSpec& mobSpec(MobType t);

struct Mob {
    MobType type = MobType::Pig;
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float yaw = 0.0f;  // Blickrichtung (Bogenmass), vorwaerts = (cos, 0, sin)
    int health = 10;
    bool onGround = false;
    bool inWater = false;

    enum State : uint8_t { Idle, Wander } state = Idle;
    float stateTimer = 0.0f;
    float wanderYaw = 0.0f;
    float panicTimer = 0.0f;  // Flucht nach einem Treffer
    glm::vec2 fleeDir{1.0f, 0.0f};
    float hurtTimer = 0.0f;
    float attackCooldown = 0.0f;
    float burnTimer = 0.0f;
    float lavaTimer = 0.0f;
    float dayTimer = 0.0f;  // Sekunden bei Tageslicht ohne freien Himmel
    float walkPhase = 0.0f;
    float walkAmount = 0.0f;
    float deathTimer = -1.0f;  // >= 0: tot, liegt am Boden und verschwindet
    float stuckTimer = 0.0f;

    bool dead() const { return deathTimer >= 0.0f; }
};

struct PlayerHit {
    int damage;
    glm::vec3 knockback;
};

struct EntityEnv {
    float daylight = 1.0f;
    bool creative = false;
    bool playerDead = false;
    bool allowSpawns = true;
};

class EntityManager {
public:
    explicit EntityManager(uint32_t seed = 1) : rng_(seed) {}

    void clear();
    void update(float dt, World& world, const Player& player, Inventory& inventory, const EntityEnv& env);

    void spawnItem(const ItemStack& stack, const glm::vec3& pos, const glm::vec3& vel, float pickupDelay = 0.5f);
    Mob& spawnMob(MobType type, const glm::vec3& pos);

    // Naechster Mob entlang des Strahls (Abstand in outDist), nullptr wenn keiner getroffen wird
    Mob* raycastMob(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float& outDist);
    // Schaden mit Rueckstoss; bei Tod fallen Gegenstaende
    void hurtMob(Mob& m, int damage, const glm::vec3& from);

    // Vom Spieler erlittene Treffer (werden vom Spiel abgeholt)
    std::vector<PlayerHit>& playerHits() { return playerHits_; }

    // Geometrie fuer die Darstellung (Positionen relativ zur Kamera). "hurt" = rot getoent.
    void buildMesh(const World& world, const glm::vec3& camPos, float time, std::vector<Vertex>& normal,
                   std::vector<Vertex>& hurt) const;

    const std::vector<Mob>& mobs() const { return mobs_; }
    std::vector<Mob>& mobs() { return mobs_; }
    const std::vector<ItemEntity>& items() const { return items_; }
    int countMobs(bool hostile) const;

private:
    void updateItem(ItemEntity& it, float dt, World& world);
    void updateMob(Mob& m, float dt, World& world, const Player& player, const EntityEnv& env);
    void trySpawn(World& world, const Player& player, const EntityEnv& env);
    void dropLoot(const Mob& m);

    std::vector<ItemEntity> items_;
    std::vector<Mob> mobs_;
    std::vector<PlayerHit> playerHits_;
    Rng rng_;
    float spawnTimer_ = 2.0f;
};

}  // namespace oc
