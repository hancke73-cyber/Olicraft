#pragma once
#include <array>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "Block.h"
#include "Items.h"

namespace oc {

// Schluessel wie bei Oefen und Truhen (gleiche Formel wie in World.cpp)
inline uint64_t machineKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x) & 0xFFFFFFu) << 32) |
           (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0xFFFFFFu) << 8) | static_cast<uint64_t>(y & 0xFF);
}
// Nachbarrichtungen, gleiche Reihenfolge wie die Bohrerrichtungen: +X, -X, +Z, -Z, oben, unten
inline constexpr int DIR6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}};

// Zustand einer Dampfmaschine (Kessel, Destille, Bohrer, Motor), wird mit dem Spielstand gespeichert.
enum class MachineKind : uint8_t { Boiler = 0, Still = 1, Drill = 2, Motor = 3, Pump = 4 };

struct Machine {
    MachineKind kind = MachineKind::Boiler;
    // Kessel: [0] Brennstoff, [1] Wassereimer (wird in den Tank geleert); Destille: [0] Eingabe, [1] und [2] Ausgabe
    ItemStack slots[3];
    float water = 0.0f;      // Kessel: Wasser im Tank in ml
    float purity = 0.0f;     // Kessel: Anteil destilliertes Wasser 0..1 (mehr Dampf je Wasser)
    float burnTime = 0.0f;   // Kessel: Rest der Brenndauer
    float burnTotal = 0.0f;  // Kessel: Gesamtdauer des Brennstoffs (Flammenanzeige)
    float progress = 0.0f;   // Destille und Bohrer: Fortschritt 0..1
    // abgeleitete Werte, nicht gespeichert
    float load = 0.0f;       // Kessel: Auslastung, Motor: Leistung, Destille und Bohrer: Geschwindigkeit (0..1)
    float steam = 0.0f;      // Dampfversorgung des Netzes 0..1
    float spin = 0.0f;       // Motor: Drehwinkel fuer die Anzeige
    float pumped = 0.0f;     // Pumpe: Vorrat der Leitung in ml (laeuft zwischen den Schritten mit)
    float drained = 0.0f;    // Pumpe: ml seit der letzten Entnahme einer endlichen Quelle
    bool working = false;
    bool wantsFlow = false;  // Pumpe: hat Wasser in Reichweite und jemand braucht es (bestimmt den Bedarf an Antrieb)

    bool burning() const { return burnTime > 0.0f; }
};

constexpr float BOILER_WATER_MAX = 2000.0f;
constexpr float BOILER_BUCKET_ML = 1000.0f;
constexpr float BOILER_STEAM = 8.0f;        // Dampf je Sekunde bei brennendem Kessel
constexpr float BOILER_WATER_PER_STEAM = 0.5f;  // ml Wasser je Dampfeinheit (destilliertes halbiert)
constexpr float MOTOR_POWER = 4.0f;         // mechanische Leistung eines Motors
constexpr float MOTOR_IDLE_STEAM = 0.3f;
constexpr float MOTOR_STEAM_AT_FULL = 5.0f; // zusaetzlicher Dampfbedarf bei voller Last
constexpr float STILL_STEAM = 3.0f;
constexpr float DRILL_POWER = 2.0f;
constexpr float CONVEYOR_POWER = 0.2f;
constexpr float LIFT_POWER = 0.3f;
constexpr int DRILL_RANGE = 16;
constexpr float PUMP_POWER = 1.0f;       // Antriebsbedarf einer arbeitenden Pumpe
constexpr float PUMP_FLOW = 60.0f;       // ml Wasser je Sekunde bei voller Versorgung
constexpr float OUTLET_COST = 250.0f;    // ml je Quellblock am Wasserauslass
constexpr float WATER_SOURCE_ML = 1000.0f;  // so viel Wasser enthaelt eine endliche Quelle
constexpr float BELT_SPEED = 1.6f;   // Bloecke je Sekunde bei voller Versorgung
constexpr float LIFT_SPEED = 3.0f;   // Steiggeschwindigkeit im Dampflift

MachineKind machineKindOf(BlockId id);  // nur fuer isMachineBlock(id)
// Brenndauer im Kessel in Sekunden bei voller Last (0: kein Brennstoff)
float boilerFuelSeconds(ItemId item);

// Destille: Eingabe -> bis zu zwei Ausgaben
struct StillRecipe {
    ItemId input;
    ItemStack out1, out2;
    float seconds;
};
const StillRecipe* stillRecipe(ItemId input);

// Beute, die eine Maschine abgibt (Bohrer ohne Truhe): Game laesst sie als Gegenstaende fallen
struct MachineDrop {
    glm::ivec3 pos;
    ItemStack item;
};

class World;
// Name und zwei Zustandszeilen fuer Fadenkreuz-Anzeige und Fenster
std::array<std::string, 3> machineInfo(World& w, const glm::ivec3& p);

struct MachineSave {
    int32_t x = 0, y = 0, z = 0;
    Machine m;
};

}  // namespace oc
