// Darstellung der Dampffahrzeuge aus vielen kleinen Quadern (Texturen aus dem Blockatlas).
// Lokale Achsen: x vorwaerts, y oben, z rechts. Mitte der Gondeln bzw. Unterkante des Bootsrumpfs.
#include "VehicleModels.h"

#include <algorithm>
#include <cmath>

#include "EntityBox.h"
#include "World.h"

namespace oc {

namespace {
constexpr float LENGTHS[3] = {3.2f, 9.0f, 12.0f};
constexpr float DIAMETERS[3] = {3.2f, 3.4f, 4.4f};
constexpr float ROPE_GAP = 0.8f;  // Abstand zwischen Gondeldach und Ballon
constexpr float PI = 3.14159265f;

Tile woodTile() { return static_cast<Tile>(blockInfo(PLANKS).top); }
Tile glassTile() { return static_cast<Tile>(blockInfo(GLASS).top); }
Tile lampTile() { return static_cast<Tile>(blockInfo(LAMP).top); }
Tile woolTile(BlockId wool) { return static_cast<Tile>(blockInfo(wool).top); }

// Quader mit Drehung und Welttransformation des Fahrzeugs
struct Builder {
    std::vector<Vertex>& out;
    float sky, light, c, s, wobble;
    glm::vec3 o;

    glm::vec3 world(const glm::vec3& l) const { return glm::vec3(o.x + l.x * c - l.z * s, o.y + l.y + wobble, o.z + l.x * s + l.z * c); }
    void box(glm::vec3 center, glm::vec3 size, Tile tile, bool glow = false) {
        addBox(out, center, size, tile, glow ? 2.0f : sky, light, [&](glm::vec3 l) { return world(l); });
    }
    // Drehung um die x-Achse (a) durch die Mitte des Quaders
    void boxX(glm::vec3 center, glm::vec3 size, Tile tile, float a) {
        const float ca = std::cos(a), sa = std::sin(a);
        addBox(out, center, size, tile, sky, light, [&](glm::vec3 l) {
            const glm::vec3 d = l - center;
            return world(center + glm::vec3(d.x, d.y * ca - d.z * sa, d.y * sa + d.z * ca));
        });
    }
    // Drehung um die y-Achse
    void boxY(glm::vec3 center, glm::vec3 size, Tile tile, float a) {
        const float ca = std::cos(a), sa = std::sin(a);
        addBox(out, center, size, tile, sky, light, [&](glm::vec3 l) {
            const glm::vec3 d = l - center;
            return world(center + glm::vec3(d.x * ca - d.z * sa, d.y, d.x * sa + d.z * ca));
        });
    }
    // Achteckiger Querschnitt (Radius r) laengs der x-Achse: Kreuz plus gedrehtes Quadrat
    void octaX(float x, float len, float y, float r, Tile tile) {
        box({x, y, 0.0f}, {len, 1.84f * r, 0.76f * r}, tile);
        box({x, y, 0.0f}, {len, 0.76f * r, 1.84f * r}, tile);
        boxX({x, y, 0.0f}, {len, 1.3f * r, 1.3f * r}, tile, PI * 0.25f);
    }
    // Achteckiger Querschnitt senkrecht (Schicht eines runden Ballons)
    void octaY(float y, float h, float r, Tile tile) {
        box({0.0f, y, 0.0f}, {1.84f * r, h, 0.76f * r}, tile);
        box({0.0f, y, 0.0f}, {0.76f * r, h, 1.84f * r}, tile);
        boxY({0.0f, y, 0.0f}, {1.3f * r, h, 1.3f * r}, tile, PI * 0.25f);
    }
    // Propeller mit Nabe und vier Fluegeln um die x-Achse bei (x, y)
    void propeller(float x, float y, float radius, float angle) {
        for (int k = 0; k < 4; ++k) boxX({x, y + 0.0f, 0.0f}, {0.05f, radius, 0.2f}, TILE_SHAFT, angle + static_cast<float>(k) * PI * 0.5f);
        box({x, y, 0.0f}, {0.2f, 0.2f, 0.2f}, TILE_BRASS);
        box({x + 0.12f, y, 0.0f}, {0.1f, 0.12f, 0.12f}, TILE_SHAFT);
    }
    // Kessel mit Schornstein und Messingbaendern (liegend entlang x)
    void boiler(float x, float y, float len, float r, Tile skin, bool lit) {
        octaX(x, len, y, r, skin);
        for (float d : {-0.3f, 0.3f}) octaX(x + d * len, 0.1f, y, r * 1.08f, TILE_BRASS);
        box({x - len * 0.2f, y + r + 0.25f, 0.0f}, {0.24f, 0.5f, 0.24f}, TILE_PIPE);
        box({x - len * 0.2f, y + r + 0.54f, 0.0f}, {0.34f, 0.1f, 0.34f}, TILE_BRASS);
        if (lit) box({x + len * 0.5f + 0.02f, y, 0.0f}, {0.04f, r * 0.8f, r * 0.8f}, lampTile(), true);  // Feuerklappe
    }
};

}  // namespace

float balloonLength(int size) { return LENGTHS[std::max(0, std::min(2, size))]; }
float balloonDiameter(int size) { return DIAMETERS[std::max(0, std::min(2, size))]; }
float balloonCenterY(int size) { return 0.6f + ROPE_GAP + balloonDiameter(size) * 0.5f; }

namespace {

void boatModel(Builder& b, const Vehicle& v) {
    const Tile wood = woodTile(), rib = TILE_RIB;
    // Rumpf: Kiel, drei Lagen die nach oben ausladen, Bug und Heck
    b.box({0.0f, 0.06f, 0.0f}, {2.9f, 0.12f, 0.5f}, rib);
    b.box({0.0f, 0.2f, 0.0f}, {3.0f, 0.16f, 0.9f}, wood);
    for (int side : {-1, 1}) {
        const float z = static_cast<float>(side);
        b.box({0.0f, 0.36f, z * 0.52f}, {3.0f, 0.2f, 0.12f}, wood);
        b.box({0.0f, 0.56f, z * 0.62f}, {3.2f, 0.22f, 0.12f}, wood);
        b.box({0.0f, 0.78f, z * 0.68f}, {3.3f, 0.2f, 0.1f}, rib);  // Dollbord
        for (float x = -1.4f; x <= 1.41f; x += 0.7f) b.box({x, 0.5f, z * 0.7f}, {0.08f, 0.62f, 0.08f}, rib);  // Spanten aussen
        for (float x = -1.2f; x <= 1.21f; x += 0.8f) b.box({x, 0.98f, z * 0.68f}, {0.07f, 0.4f, 0.07f}, wood);  // Reling
        b.box({0.0f, 1.18f, z * 0.68f}, {2.7f, 0.06f, 0.06f}, TILE_BRASS);
    }
    for (int i = 0; i < 3; ++i) {  // Bug laeuft spitz zu
        const float f = static_cast<float>(i);
        b.box({1.6f + f * 0.2f, 0.3f + f * 0.2f, 0.0f}, {0.5f, 0.28f, 0.9f - f * 0.3f}, wood);
    }
    b.box({2.2f, 0.95f, 0.0f}, {0.1f, 0.5f, 0.1f}, rib);                      // Bugspriet
    b.box({2.2f, 1.28f, 0.0f}, {0.2f, 0.2f, 0.2f}, lampTile(), true);        // Laterne
    b.box({-1.62f, 0.55f, 0.0f}, {0.12f, 0.8f, 1.3f}, wood);                  // Heckspiegel
    b.box({-1.7f, 0.95f, 0.0f}, {0.3f, 0.12f, 1.0f}, rib);
    b.box({0.0f, 0.3f, 0.0f}, {2.6f, 0.04f, 1.0f}, rib);                      // Deckplanken
    for (float x = -1.0f; x <= 1.01f; x += 0.5f) b.box({x, 0.33f, 0.0f}, {0.04f, 0.04f, 1.0f}, wood);
    b.box({-0.9f, 0.5f, 0.0f}, {0.5f, 0.1f, 1.0f}, wood);                     // Sitzbank
    b.boiler(0.5f, 0.95f, 1.2f, 0.42f, v.engine.burning() ? TILE_BOILER_LIT : TILE_BOILER, v.engine.burning());
    for (float d : {-0.35f, 0.35f}) b.box({0.5f + d, 0.55f, 0.0f}, {0.12f, 0.2f, 0.7f}, TILE_BRASS);  // Kesselsattel
    b.propeller(-1.85f, 0.3f, 0.7f, v.propAngle);
    b.box({-1.75f, 0.12f, 0.0f}, {0.5f, 0.06f, 0.3f}, TILE_SHAFT);  // Ruderblatt
}

// Gondeln hintereinander mit Fenstern, Spanten, Bullaugen und Reling; gibt das hintere Ende zurueck
float gondolas(Builder& b, const Vehicle& v) {
    const Tile wood = woodTile(), rib = TILE_RIB;
    const float n = static_cast<float>(v.modules);
    for (int k = 0; k < v.modules; ++k) {
        const float x = (static_cast<float>(k) - (n - 1.0f) * 0.5f) * MODULE_LENGTH, len = MODULE_LENGTH - 0.08f;
        b.box({x, -0.5f, 0.0f}, {len * 0.8f, 0.2f, 0.8f}, rib);                 // Kielboden
        b.box({x, -0.28f, 0.0f}, {len, 0.26f, 1.1f}, wood);
        for (int side : {-1, 1}) {
            const float z = static_cast<float>(side);
            b.box({x, 0.0f, z * 0.6f}, {len, 0.3f, 0.1f}, wood);               // Bordwand
            b.box({x, 0.3f, z * 0.62f}, {len, 0.08f, 0.1f}, TILE_BRASS);        // Messingleiste
            for (int w = 0; w < 3; ++w)                                          // Fenster zwischen Pfosten
                b.box({x + (static_cast<float>(w) - 1.0f) * 0.62f, 0.12f, z * 0.64f}, {0.42f, 0.34f, 0.04f}, glassTile());
            for (int p = 0; p < 4; ++p) b.box({x + (static_cast<float>(p) - 1.5f) * 0.62f, 0.12f, z * 0.64f}, {0.07f, 0.48f, 0.07f}, rib);
            b.box({x, -0.4f, z * 0.58f}, {len * 0.9f, 0.1f, 0.06f}, rib);
            for (int p = 0; p < 2; ++p) b.box({x + (p ? 0.5f : -0.5f), -0.12f, z * 0.67f}, {0.14f, 0.14f, 0.05f}, lampTile(), true);  // Bullaugen
        }
        b.box({x, 0.42f, 0.0f}, {len, 0.08f, 1.3f}, TILE_BRASS);                 // Dach
        b.box({x, 0.5f, 0.0f}, {len * 0.9f, 0.08f, 0.9f}, wood);
        b.box({x - len * 0.5f, -0.1f, 0.0f}, {0.07f, 0.8f, 1.0f}, rib);         // Spant
        b.box({x + len * 0.5f, -0.1f, 0.0f}, {0.07f, 0.8f, 1.0f}, rib);
    }
    const float front = n * MODULE_LENGTH * 0.5f;
    b.box({front + 0.1f, 0.0f, 0.0f}, {0.2f, 0.5f, 0.8f}, wood);                // Bugkappe der Gondel
    b.box({front + 0.22f, 0.1f, 0.0f}, {0.04f, 0.3f, 0.5f}, glassTile());       // Frontfenster
    b.box({front + 0.1f, 0.5f, 0.0f}, {0.2f, 0.2f, 0.2f}, lampTile(), true);    // Positionslampe
    return -front;
}

void airshipModel(Builder& b, const Vehicle& v) {
    const Tile rib = TILE_RIB;
    const float rear = gondolas(b, v);
    // Maschine am Heck: Kessel, Propeller, Seitenruder
    b.boiler(rear - 0.55f, -0.1f, 0.9f, 0.4f, v.engine.burning() ? TILE_BOILER_LIT : TILE_BOILER, v.engine.burning());
    b.box({rear - 0.1f, -0.1f, 0.0f}, {0.1f, 0.5f, 0.9f}, rib);
    if (v.propeller) b.propeller(rear - 1.15f, -0.1f, 1.0f, v.propAngle);
    b.box({rear - 0.4f, 0.35f, 0.0f}, {0.5f, 0.6f, 0.06f}, TILE_BRASS);        // Ruder
    if (v.balloon < 0) return;

    const int size = v.balloon;
    const float len = balloonLength(size), dia = balloonDiameter(size), cy = balloonCenterY(size);
    const Tile white = woolTile(WOOL_WHITE);
    const Tile color = woolTile(size == 0 ? WOOL_RED : (size == 1 ? WOOL_BLUE : WOOL_GREEN));
    const float sag = 0.04f;
    if (size == 0) {  // Heissluftballon: runde Huelle in Schichten, Gurte, Bruenner
        const float r0 = dia * 0.5f;
        const int layers = 8;
        for (int i = 0; i < layers; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(layers);  // 0 unten .. 1 oben
            const float prof = std::sqrt(std::max(0.0f, 1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f)));
            const float r = r0 * (0.28f + 0.72f * prof);
            const float h = dia / static_cast<float>(layers);
            b.octaY(cy - r0 + (static_cast<float>(i) + 0.5f) * h, h + 0.01f, r, i % 2 == 0 ? white : color);
            if (i % 3 == 1) b.octaY(cy - r0 + (static_cast<float>(i) + 0.5f) * h, 0.1f, r * 1.07f, rib);  // Reifen
        }
        b.box({0.0f, cy + r0 + 0.1f, 0.0f}, {0.3f, 0.12f, 0.3f}, TILE_BRASS);  // Scheitelring
        b.box({0.0f, cy - r0 - 0.15f, 0.0f}, {0.9f, 0.12f, 0.9f}, TILE_BRASS);  // Brennerrahmen
        b.box({0.0f, cy - r0 - 0.3f, 0.0f}, {0.3f, 0.2f, 0.3f}, lampTile(), true);  // Flamme
        for (int sx : {-1, 1})
            for (int sz : {-1, 1}) {
                b.box({static_cast<float>(sx) * 0.55f, 0.65f + (cy - r0 - 0.7f) * 0.5f, static_cast<float>(sz) * 0.55f}, {0.06f, cy - r0 - 0.65f, 0.06f}, TILE_ROPE);
            }
        return;
    }
    // Zeppelin: Huelle aus Achtecken mit Rippen, Messingbaendern, Heckflossen, Bugkappe
    const int slices = static_cast<int>(len * 2.0f);
    const float sl = len / static_cast<float>(slices);
    for (int k = 0; k < slices; ++k) {
        const float t = (static_cast<float>(k) + 0.5f) / static_cast<float>(slices);
        const float prof = std::pow(std::max(0.0f, 1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f)), 0.55f);
        const float r = dia * 0.5f * (0.3f + 0.7f * prof);
        const float x = (t - 0.5f) * len;
        b.octaX(x, sl + 0.01f, cy, r, (k / 4) % 2 == 0 ? white : color);
        if (k % 6 == 0) b.octaX(x - sl * 0.5f, 0.12f, cy, r * 1.08f, rib);                   // Spantring
        if (k == 2 || k == slices - 3) b.octaX(x + sl * 0.4f, 0.1f, cy, r * 1.1f, TILE_BRASS);  // Messingband
    }
    // Laengsgurte oben und unten
    b.box({0.0f, cy + dia * 0.5f + 0.05f, 0.0f}, {len * 0.8f, 0.08f, 0.14f}, rib);
    b.box({0.0f, cy - dia * 0.5f - 0.04f, 0.0f}, {len * 0.8f, 0.08f, 0.14f}, rib);
    b.box({len * 0.5f + 0.1f, cy, 0.0f}, {0.24f, dia * 0.3f, dia * 0.3f}, TILE_BRASS);     // Bugkappe
    b.box({len * 0.5f + 0.28f, cy, 0.0f}, {0.12f, dia * 0.12f, dia * 0.12f}, lampTile(), true);
    const float fx = -len * 0.5f + 1.2f;
    const float tt = (fx / len) + 0.5f;
    const float fr = dia * 0.5f * (0.3f + 0.7f * std::pow(std::max(0.0f, 1.0f - (2.0f * tt - 1.0f) * (2.0f * tt - 1.0f)), 0.55f)) + 0.7f;
    for (int k = 0; k < 4; ++k) {  // vier Heckflossen
        const float a = static_cast<float>(k) * PI * 0.5f;
        b.boxX({fx, cy, 0.0f}, {1.4f, fr * 2.0f, 0.1f}, size == 1 ? woolTile(WOOL_RED) : woolTile(WOOL_BLUE), a);
        b.boxX({fx - 0.1f, cy, 0.0f}, {1.5f, fr * 2.0f + 0.06f, 0.04f}, rib, a);
    }
    // Aufhaengung: Seile und Gondelgurte
    const int pairs = std::max(3, static_cast<int>(len / 2.5f));
    for (int k = 0; k < pairs; ++k) {
        const float x = (static_cast<float>(k) / static_cast<float>(pairs - 1) - 0.5f) * len * 0.62f;
        for (int sz : {-1, 1}) {
            b.box({x, 0.5f + (cy - dia * 0.5f + 0.2f - 0.5f) * 0.5f, static_cast<float>(sz) * 0.55f}, {0.06f, cy - dia * 0.5f - 0.3f, 0.06f}, TILE_ROPE);
            b.box({x, cy - dia * 0.5f + 0.1f + sag, static_cast<float>(sz) * 0.55f}, {0.16f, 0.1f, 0.16f}, TILE_BRASS);
        }
    }
}

}  // namespace

void appendVehicleMesh(const Vehicle& v, const World& world, const glm::vec3& camPos, float time, EntityBatches& out) {
    const int bx = static_cast<int>(std::floor(v.pos.x)), bz = static_cast<int>(std::floor(v.pos.z));
    if (!world.isChunkLoaded(floorDiv(bx, CHUNK_W), floorDiv(bz, CHUNK_W))) return;
    Builder b{out.items, 1.0f, std::max(static_cast<float>(world.lightAt(bx, static_cast<int>(std::floor(v.pos.y + 0.5f)), bz)) / MAX_LIGHT, 0.0f),
              std::cos(v.yaw), std::sin(v.yaw), v.type == VehicleType::Boat ? 0.03f * std::sin(time * 2.0f + static_cast<float>(v.id)) : 0.0f,
              v.pos - camPos};
    if (v.type == VehicleType::Boat) {
        boatModel(b, v);
    } else {
        airshipModel(b, v);
        if (v.anchored) {  // Seil mit Anker bis zum Boden
            int gy = static_cast<int>(std::floor(v.pos.y));
            while (gy > 1 && !world.isSolidAt(bx, gy, bz) && !isWater(world.getBlock(bx, gy, bz))) --gy;
            const float len = std::min(40.0f, std::max(0.0f, v.pos.y - static_cast<float>(gy + 1)));
            b.box({0.9f, -len * 0.5f - 0.6f, 0.0f}, {0.07f, len, 0.07f}, TILE_ROPE);
            b.box({0.9f, -len - 0.55f, 0.0f}, {0.4f, 0.14f, 0.4f}, TILE_BRASS);
            b.box({0.9f, -len - 0.7f, 0.0f}, {0.14f, 0.2f, 0.14f}, TILE_SHAFT);
        }
    }
}

}  // namespace oc
