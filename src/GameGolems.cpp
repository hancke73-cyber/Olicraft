// Dampfgolems im Spiel: aus Bloecken bauen, im Spielstand erhalten
#include "Game.h"

namespace oc {

// Nach dem Setzen einer Lampe: Lampe auf Dampfkessel auf Eisenblock, Eisenbloecke links und rechts vom Kessel ergeben einen Golem.
// Gebaut wird in zwei Richtungen (Arme entlang x oder z). true, wenn ein Golem entstanden ist.
bool Game::tryBuildGolem(const glm::ivec3& head) {
    if (!steampunkEnabled() || world_->getBlock(head.x, head.y, head.z) != LAMP) return false;
    const glm::ivec3 torso = head - glm::ivec3(0, 1, 0), legs = head - glm::ivec3(0, 2, 0);
    if (!isBoiler(world_->getBlock(torso.x, torso.y, torso.z)) || world_->getBlock(legs.x, legs.y, legs.z) != IRON_BLOCK) return false;
    for (const glm::ivec3& axis : {glm::ivec3(1, 0, 0), glm::ivec3(0, 0, 1)}) {
        const glm::ivec3 a = torso + axis, b = torso - axis;
        if (world_->getBlock(a.x, a.y, a.z) != IRON_BLOCK || world_->getBlock(b.x, b.y, b.z) != IRON_BLOCK) continue;
        for (const glm::ivec3& c : {head, torso, legs, a, b}) {
            world_->removeMachine(c.x, c.y, c.z, nullptr);
            world_->setBlock(c.x, c.y, c.z, AIR);
        }
        Mob& g = entities_.spawnMob(MobType::Golem, glm::vec3(legs.x + 0.5f, static_cast<float>(legs.y), legs.z + 0.5f));
        g.owned = true;
        g.yaw = glm::radians(player_.yaw) + 3.14159265f;  // blickt den Spieler an
        playSound(SoundId::BlockStone, glm::vec3(legs) + glm::vec3(0.5f, 1.0f, 0.5f), 1.0f, 0.7f);
        showMessage("Ein Dampfgolem erwacht");
        return true;
    }
    return false;
}

// Nach dem Setzen von Lampe, Kessel oder Eisenblock: ist das Bauwerk mit diesem Teil als letztem vollstaendig?
bool Game::tryBuildGolemNear(const glm::ivec3& p) {
    static const glm::ivec3 heads[] = {{0, 0, 0}, {0, 1, 0}, {0, 2, 0}, {1, 1, 0}, {-1, 1, 0}, {0, 1, 1}, {0, 1, -1}};
    for (const glm::ivec3& h : heads)
        if (tryBuildGolem(p + h)) return true;
    return false;
}

}  // namespace oc
