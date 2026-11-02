#pragma once
#include <glm/glm.hpp>

namespace oc {

class World;

struct MoveInput {
    float forward = 0.0f;  // -1..1
    float strafe = 0.0f;   // -1..1 (rechts positiv)
    bool jump = false;
    bool sneak = false;    // beim Fliegen: absteigen
    bool sprint = false;
};

class Player {
public:
    static constexpr float WIDTH = 0.6f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float EYE = 1.62f;

    glm::vec3 pos{0.0f};  // Fuss-Mitte
    glm::vec3 vel{0.0f};
    float yaw = -90.0f;   // Grad
    float pitch = 0.0f;   // Grad
    bool flying = false;
    bool onGround = false;
    bool inWater = false;
    bool headInWater = false;
    float landedFall = 0.0f;  // Fallhoehe beim letzten Aufprall (vom Spiel auszulesen und zurueckzusetzen)

    glm::vec3 eyePos() const { return pos + glm::vec3(0.0f, EYE, 0.0f); }
    glm::vec3 front() const;
    void look(float dxPixels, float dyPixels, float sensitivity);
    void update(float dt, const MoveInput& in, const World& world);

    // Ueberschneidet der Spieler den Block (x,y,z)?
    bool intersectsBlock(int x, int y, int z) const;

private:
    float peakY_ = 0.0f;
    float boost_ = 0.0f;  // Restzeit des Schubs beim Verlassen des Wassers
    bool airborne_ = false;
    void moveAxis(int axis, float delta, const World& world);
    bool bodyInWater(const World& world) const;
};

}  // namespace oc
