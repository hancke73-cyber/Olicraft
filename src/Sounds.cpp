#include "Sounds.h"

namespace oc {

static const SoundDef DEFS[SOUND_COUNT] = {
    // file, Beschreibung, Prompt, Sekunden, Lautstaerke
    {"block_stone", "Stein abbauen/setzen", "sharp chip of a pickaxe hitting stone, short dry rock crack, game sound effect", 0.5f, 0.9f},
    {"block_dirt", "Erde abbauen/setzen", "dull soft thud of digging into dirt, short earthy crunch, game sound effect", 0.5f, 0.9f},
    {"block_grass", "Gras und Laub", "short soft rustle of grass and leaves being cut, game sound effect", 0.5f, 0.8f},
    {"block_sand", "Sand abbauen/setzen", "short dry scrape and soft hiss of sand being shovelled, game sound effect", 0.5f, 0.8f},
    {"block_wood", "Holz abbauen/setzen", "hollow knock of an axe hitting wood, short wooden thunk, game sound effect", 0.5f, 0.9f},
    {"block_glass", "Glas abbauen", "short bright shatter of glass breaking into pieces, game sound effect", 0.7f, 0.8f},
    {"block_snow", "Schnee abbauen/setzen", "very soft muffled crunch of snow, short, game sound effect", 0.5f, 0.7f},

    {"step_stone", "Schritt auf Stein", "single footstep on hard stone, short dry tap, game sound effect", 0.35f, 0.6f},
    {"step_dirt", "Schritt auf Erde", "single footstep on soft dirt, muffled short thud, game sound effect", 0.35f, 0.6f},
    {"step_grass", "Schritt auf Gras", "single footstep on grass, soft short swish, game sound effect", 0.35f, 0.55f},
    {"step_sand", "Schritt auf Sand", "single footstep on dry sand, soft short scrunch, game sound effect", 0.35f, 0.55f},
    {"step_wood", "Schritt auf Holz", "single footstep on wooden planks, short hollow tap, game sound effect", 0.35f, 0.6f},
    {"step_snow", "Schritt auf Schnee", "single footstep in snow, soft short crunch, game sound effect", 0.4f, 0.55f},

    {"water_splash", "Platschen beim Eintauchen", "short splash of a person jumping into water, game sound effect", 0.9f, 0.9f},
    {"water_swim", "Schwimmen", "short gentle water paddling swish, game sound effect", 0.6f, 0.5f},
    {"lava_pop", "Lava blubbert", "single thick bubble popping in hot lava, short bloop with hiss, game sound effect", 0.7f, 0.6f},

    {"player_hurt", "Spieler verletzt", "short pained grunt of a man taking damage, oof, game sound effect", 0.5f, 0.9f},
    {"player_death", "Spieler stirbt", "short dying groan of a man falling down, game sound effect", 1.2f, 0.9f},
    {"eat", "Essen", "short crunchy bite and chew of eating food, game sound effect", 0.9f, 0.8f},
    {"item_pickup", "Gegenstand aufheben", "short bright pop of picking up an item, tiny blip, game sound effect", 0.25f, 0.5f},
    {"click", "Menue-Klick", "short soft click of a button, user interface, game sound effect", 0.15f, 0.5f},
    {"craft", "Herstellen", "short satisfying tool crafting clink with a wooden tap, game sound effect", 0.5f, 0.7f},
    {"fuse", "Ofen entzuendet", "short whoosh of a furnace igniting with a crackle, game sound effect", 1.0f, 0.6f},

    {"pig_idle", "Schwein grunzt", "a pig oinking and grunting, short, cartoon farm animal, game sound effect", 1.0f, 0.8f},
    {"pig_hurt", "Schwein verletzt", "a pig squealing in pain, short, game sound effect", 0.8f, 0.9f},
    {"pig_death", "Schwein stirbt", "a pig squealing and dying, short, game sound effect", 1.0f, 0.9f},
    {"cow_idle", "Kuh muht", "a cow mooing, one low moo, farm animal, game sound effect", 1.6f, 0.8f},
    {"cow_hurt", "Kuh verletzt", "a cow bellowing in pain, short, game sound effect", 0.9f, 0.9f},
    {"cow_death", "Kuh stirbt", "a cow mooing sadly and falling, short, game sound effect", 1.2f, 0.9f},
    {"sheep_idle", "Schaf blökt", "a sheep bleating baa, short, farm animal, game sound effect", 1.0f, 0.8f},
    {"sheep_hurt", "Schaf verletzt", "a sheep bleating in pain, short sharp baa, game sound effect", 0.7f, 0.9f},
    {"sheep_death", "Schaf stirbt", "a sheep bleating weakly and dying, short, game sound effect", 1.0f, 0.9f},

    {"zombie_idle", "Zombie stöhnt", "a zombie groaning, low raspy undead moan, horror game sound effect", 1.6f, 0.8f},
    {"zombie_hurt", "Zombie verletzt", "a zombie grunting in pain, short raspy hiss and groan, game sound effect", 0.7f, 0.9f},
    {"zombie_death", "Zombie stirbt", "a zombie dying, long raspy groan fading out, horror game sound effect", 1.4f, 0.9f},
    {"zombie_attack", "Zombie greift an", "a zombie snarling and lunging, short aggressive growl, horror game sound effect", 0.8f, 0.9f},
};

const SoundDef& soundDef(SoundId id) { return DEFS[static_cast<int>(id)]; }

Material blockMaterial(BlockId b) {
    switch (b) {
        case GRASS:
        case LEAVES: return Material::Grass;
        case DIRT: return Material::Dirt;
        case SAND: return Material::Sand;
        case LOG:
        case PLANKS:
        case CRAFTING_TABLE:
        case TORCH:
        case TORCH_PX:
        case TORCH_NX:
        case TORCH_PZ:
        case TORCH_NZ: return Material::Wood;
        case GLASS: return Material::Glass;
        case SNOW: return Material::Snow;
        default: return Material::Stone;
    }
}

SoundId blockSound(Material m) {
    switch (m) {
        case Material::Dirt: return SoundId::BlockDirt;
        case Material::Grass: return SoundId::BlockGrass;
        case Material::Sand: return SoundId::BlockSand;
        case Material::Wood: return SoundId::BlockWood;
        case Material::Glass: return SoundId::BlockGlass;
        case Material::Snow: return SoundId::BlockSnow;
        default: return SoundId::BlockStone;
    }
}

SoundId stepSound(Material m) {
    switch (m) {
        case Material::Dirt: return SoundId::StepDirt;
        case Material::Grass: return SoundId::StepGrass;
        case Material::Sand: return SoundId::StepSand;
        case Material::Wood: return SoundId::StepWood;
        case Material::Snow: return SoundId::StepSnow;
        default: return SoundId::StepStone;  // Stein und Glas
    }
}

}  // namespace oc
