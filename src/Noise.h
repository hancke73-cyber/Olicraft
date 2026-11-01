#pragma once
#include <cstdint>

namespace oc {

// Perlin-Rauschen (deterministisch, plattformunabhaengig)
class Noise {
public:
    explicit Noise(uint32_t seed);

    float noise2(float x, float y) const;           // ca. [-1, 1]
    float noise3(float x, float y, float z) const;  // ca. [-1, 1]

    // Fraktale Summe, normiert auf ca. [-1, 1]
    float fbm2(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const;
    float fbm3(float x, float y, float z, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const;

private:
    uint8_t perm_[512];
};

// Schneller Integer-Hash fuer Zufallsentscheidungen
uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed);

}  // namespace oc
