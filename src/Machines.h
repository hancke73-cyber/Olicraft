#pragma once
#include <cstdint>

#include <glm/glm.hpp>

#include "Block.h"
#include "Items.h"

namespace oc {

// Zustand einer Dampfmaschine (Kessel, Destille, Bohrer, Motor), wird mit dem Spielstand gespeichert.
enum class MachineKind : uint8_t { Boiler = 0, Still = 1, Drill = 2, Motor = 3 };

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
    bool working = false;

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

struct MachineSave {
    int32_t x = 0, y = 0, z = 0;
    Machine m;
};

}  // namespace oc
