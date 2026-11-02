#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Chunk.h"
#include "Inventory.h"
#include "Items.h"
#include "Skins.h"
#include "Sounds.h"

namespace oc {

class World;
class Player;
struct Village;

// Geometrie fuer die Darstellung (Positionen relativ zur Kamera), getrennt nach Textur
struct EntityBatches {
    std::vector<Vertex> items;                  // Gegenstaende am Boden (Blockatlas)
    std::vector<Vertex> skin[SKIN_COUNT];       // Figuren je Skin
    std::vector<Vertex> skinHurt[SKIN_COUNT];   // getroffene Figuren (rot getoent)
    void clear() {
        items.clear();
        for (int i = 0; i < SKIN_COUNT; ++i) {
            skin[i].clear();
            skinHurt[i].clear();
        }
    }
};

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

enum class MobType : uint8_t { Pig, Cow, Sheep, Zombie, Villager };

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
    float idleSoundTimer = 4.0f;  // bis zum naechsten Ruf
    // Dorfbewohner: Heimatdorf (Zentrum) und Beruf
    int homeX = 0, homeZ = 0, villageId = 0;
    int profession = 0;
    bool hasHome = false;
    const Village* village = nullptr;  // Heimatdorf (zeigt in den Zwischenspeicher des Dorfgenerators)
    int bedIndex = -1;                 // Bett im Dorf
    bool sleeping = false;
    int route = 0;                     // 0..3 ins Haus hinein, 10..12 hinaus
    float anchorX = 0.0f, anchorZ = 0.0f, anchorR = 22.0f;  // Bleibebereich beim Umherlaufen
    float sleepYaw = 0.0f;
    std::vector<glm::ivec3> path;  // berechneter Weg (Zellen, auf denen er steht)
    size_t pathIdx = 0;
    glm::ivec3 pathGoal{0};
    bool pathValid = false;
    float pathTimer = 0.0f;       // Zeit seit der letzten Bewegung (Festhaengen)
    glm::vec3 pathLastPos{0.0f};

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
    float timeOfDay = 0.3f;
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

    // Klangereignisse seit dem letzten Abholen (das Spiel spielt sie ab und leert die Liste)
    std::vector<SoundEvent>& soundEvents() { return soundEvents_; }

    void buildMesh(const World& world, const glm::vec3& camPos, float time, EntityBatches& out) const;

    const std::vector<Mob>& mobs() const { return mobs_; }
    std::vector<Mob>& mobs() { return mobs_; }
    const std::vector<ItemEntity>& items() const { return items_; }
    int countMobs(bool hostile) const;  // ohne Dorfbewohner
    // Dorfbewohner der Doerfer in der Naehe erscheinen lassen (einmal je Dorf und Sitzung)
    void spawnVillagers(World& world, const Player& player);

private:
    void updateItem(ItemEntity& it, float dt, World& world);
    void updateMob(Mob& m, float dt, World& world, const Player& player, const EntityEnv& env);
    void trySpawn(World& world, const Player& player, const EntityEnv& env);
    bool followPath(Mob& m, World& world, const glm::ivec3& goal, float arrive, glm::vec2& want, float& speed, bool& look, float& yaw, float dt);
    bool villagerBrain(Mob& m, float dt, World& world, const EntityEnv& env, glm::vec2& want, float& speed, bool& look, float& yaw);
    void openDoorsAhead(Mob& m, World& world, const glm::vec2& want);
    void updateOpenedDoors(float dt, World& world, const Player& player);
    struct OpenedDoor {
        glm::ivec3 pos;
        float timer;
    };
    std::vector<OpenedDoor> openedDoors_;
    void dropLoot(const Mob& m);

    std::vector<ItemEntity> items_;
    std::vector<Mob> mobs_;
    std::vector<PlayerHit> playerHits_;
    std::vector<SoundEvent> soundEvents_;
    void emitMobSound(const Mob& m, int kind);  // 0 Ruf, 1 Treffer, 2 Tod, 3 Angriff
    Rng rng_;
    float spawnTimer_ = 2.0f;
    float villageTimer_ = 0.5f;
    std::vector<uint32_t> villageDone_;  // Dorf-IDs, deren Bewohner schon erschienen sind
};

}  // namespace oc
