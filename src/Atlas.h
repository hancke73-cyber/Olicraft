#pragma once
#include <cstdint>
#include <vector>

#include "Block.h"

namespace oc {

constexpr int ATLAS_TILES = 16;   // Kacheln pro Zeile
constexpr int TILE_PX = 16;       // Pixel pro Kachel
constexpr int ATLAS_PX = ATLAS_TILES * TILE_PX;

// Erzeugt den Texturatlas prozedural (RGBA8, Zeile 0 = unten, passend zu v = 0).
std::vector<uint8_t> generateAtlasPixels();

struct UVRect {
    float u0, v0, u1, v1;
};
UVRect tileUV(int tile);

}  // namespace oc
