#pragma once
#include <cstdint>

#include "Block.h"

namespace oc {

// Alle Klangereignisse des Spiels. Zu jedem gehoert ein Dateiname ohne Endung im Ordner sounds/
// (z. B. sounds/step_grass.flac oder mehrere Varianten step_grass_1.flac, step_grass_2.flac ...).
enum class SoundId : uint8_t {
    // Bloecke abbauen/setzen (je Material)
    BlockStone, BlockDirt, BlockGrass, BlockSand, BlockWood, BlockGlass, BlockSnow,
    // Schritte
    StepStone, StepDirt, StepGrass, StepSand, StepWood, StepSnow,
    // Wasser, Lava
    WaterSplash, WaterSwim, LavaPop,
    // Spieler und Oberflaeche
    PlayerHurt, PlayerDeath, Eat, ItemPickup, Click, Craft, Fuse,
    // Tiere und Zombies
    PigIdle, PigHurt, PigDeath,
    CowIdle, CowHurt, CowDeath,
    SheepIdle, SheepHurt, SheepDeath,
    ZombieIdle, ZombieHurt, ZombieDeath, ZombieAttack,
    Count
};

constexpr int SOUND_COUNT = static_cast<int>(SoundId::Count);

struct SoundDef {
    const char* file;        // Dateiname ohne Endung
    const char* desc;        // kurze Beschreibung (deutsch)
    const char* prompt;      // Vorschlag fuer die Erzeugung mit einem Text-zu-Audio-Modell
    float seconds;           // empfohlene Dauer
    float baseVolume;        // Grundlautstaerke 0..1
};
const SoundDef& soundDef(SoundId id);

// Material eines Blocks fuer Geraeusche
enum class Material : uint8_t { Stone, Dirt, Grass, Sand, Wood, Glass, Snow };
Material blockMaterial(BlockId b);
SoundId blockSound(Material m);  // Abbauen/Setzen
SoundId stepSound(Material m);   // Laufen (Glas zaehlt als Stein)

// Ereignis, das Spielsysteme (Mobs, Gegenstaende) melden; das Spiel spielt es an der Position ab
struct SoundEvent {
    SoundId id;
    float x, y, z;
    float volume;
    float pitch;
};

}  // namespace oc
