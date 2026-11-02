#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Chest.h"
#include "Furnace.h"
#include "Inventory.h"

namespace oc {

struct PlayerState {
    glm::vec3 pos{0.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool flying = false;
    int slot = 0;
    float timeOfDay = 0.30f;  // 0..1, 0.25 = Mittag
    int health = 20;
    bool creative = false;
    bool hasInventory = false;  // false bei alten Spielstaenden (Version 1)
    Inventory inventory;
    bool hasSpawn = false;  // Version 5: Wiedereinstiegspunkt (Bett)
    glm::vec3 spawn{0.0f};
};

struct SavedChunk {
    int32_t cx = 0, cz = 0;
    std::vector<uint8_t> rle;
};

struct FurnaceSave {
    int32_t x = 0, y = 0, z = 0;
    Furnace furnace;
};

struct ChestSave {
    int32_t x = 0, y = 0, z = 0;
    Chest chest;
};

struct SaveFile {
    int32_t seed = 0;
    PlayerState player;
    std::vector<SavedChunk> chunks;
    std::vector<FurnaceSave> furnaces;
    std::vector<ChestSave> chests;
};

std::vector<uint8_t> rleEncode(const std::vector<BlockId>& blocks);
bool rleDecode(const std::vector<uint8_t>& rle, std::vector<BlockId>& out, size_t expectedSize);

// Schreibt atomar (temporaere Datei + Umbenennen).
bool writeSaveFile(const std::string& path, const SaveFile& save, std::string* error);
bool readSaveFile(const std::string& path, SaveFile& save, std::string* error);

// Nur der Kopf einer Speicherdatei (fuer die Weltliste), ohne die Chunks zu lesen.
struct SaveInfo {
    int32_t seed = 0;
    bool creative = false;
    int health = 20;
};
bool readSaveInfo(const std::string& path, SaveInfo& info);

}  // namespace oc
