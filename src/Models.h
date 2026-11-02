#pragma once
#include <vector>

#include <glm/glm.hpp>

#include "Chunk.h"
#include "Skins.h"

namespace oc {

// Figurenmodelle aus Quadern. Koordinaten in 1/16 Block: x = vorwaerts, y = oben, z = rechts.
// Jeder Quader liest sein Netz aus der Skin-Textur (Layout wie bei Minecraft, siehe skinNetRect).
enum ModelAnim : uint8_t { A_NONE, A_LEG_A, A_LEG_B, A_ARM_A, A_ARM_B, A_HEAD, A_ARM_SWING_A, A_ARM_SWING_B };

struct ModelPart {
    glm::vec3 center, size, pivot;
    ModelAnim anim;
    int u, v;      // Ursprung des Netzes in der Skin-Textur (Pixel)
    bool mirror;   // gespiegelt (linke Gliedmassen teilen sich das Netz mit den rechten)
};

const std::vector<ModelPart>& modelParts(SkinId skin);

// Rechteck der Flaeche f (0 +X vorn, 1 -X hinten, 2 +Y oben, 3 -Y unten, 4 +Z rechts, 5 -Z links) im Netz.
struct NetRect {
    int x, y, w, h;
};
NetRect skinNetRect(const ModelPart& p, int face);
// Groesse des Netzes (Breite, Hoehe) eines Quaders
glm::ivec2 skinNetSize(const ModelPart& p);

// Texturkoordinaten (0..1, v nach unten) einer Ecke (i, j) der Flaeche f im Skin
glm::vec2 skinCornerUV(const ModelPart& p, int face, int i, int j, int texW, int texH);

struct Pose {
    glm::vec3 pos{0.0f};      // Fuss-Mitte (relativ zur Kamera)
    float yaw = 0.0f;         // Blickrichtung (Bogenmass), vorwaerts = (cos, 0, sin)
    float walkPhase = 0.0f;
    float walkAmount = 0.0f;  // 0..1
    float time = 0.0f;
    float headPitch = 0.0f;   // Bogenmass, positiv = nach oben
    float roll = 0.0f;        // Umfallen (0..pi/2)
    float sky = 1.0f;
    float light = 0.0f;       // Blocklicht 0..1
    float tint = 1.0f;
    bool armsSwing = false;   // Spieler: Arme schwingen mit, Zombies halten sie vor
};

// Haengt die Geometrie des Modells an (Quads, Positionen in Kamera-Weltkoordinaten)
void appendModel(SkinId skin, const Pose& pose, std::vector<Vertex>& out);

}  // namespace oc
