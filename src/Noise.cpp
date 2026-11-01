#include "Noise.h"

#include <cmath>
#include <utility>

namespace oc {

static inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
static inline float lerpf(float a, float b, float t) { return a + t * (b - a); }

static inline float grad2(int h, float x, float y) {
    switch (h & 7) {
        case 0: return x + y;
        case 1: return -x + y;
        case 2: return x - y;
        case 3: return -x - y;
        case 4: return x * 1.4142f;
        case 5: return -x * 1.4142f;
        case 6: return y * 1.4142f;
        default: return -y * 1.4142f;
    }
}

static inline float grad3(int h, float x, float y, float z) {
    h &= 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

Noise::Noise(uint32_t seed) {
    uint32_t s = seed * 2654435761u + 0x9E3779B9u;
    if (s == 0) s = 0x1234567u;
    auto next = [&s]() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    };
    for (int i = 0; i < 256; ++i) perm_[i] = static_cast<uint8_t>(i);
    for (int i = 255; i > 0; --i) {
        int j = static_cast<int>(next() % static_cast<uint32_t>(i + 1));
        std::swap(perm_[i], perm_[j]);
    }
    for (int i = 0; i < 256; ++i) perm_[256 + i] = perm_[i];
}

float Noise::noise2(float x, float y) const {
    float fx = std::floor(x), fy = std::floor(y);
    int X = static_cast<int>(fx) & 255, Y = static_cast<int>(fy) & 255;
    x -= fx;
    y -= fy;
    float u = fade(x), v = fade(y);
    int aa = perm_[perm_[X] + Y], ab = perm_[perm_[X] + Y + 1];
    int ba = perm_[perm_[X + 1] + Y], bb = perm_[perm_[X + 1] + Y + 1];
    float r = lerpf(lerpf(grad2(aa, x, y), grad2(ba, x - 1, y), u),
                    lerpf(grad2(ab, x, y - 1), grad2(bb, x - 1, y - 1), u), v);
    return r * 0.8f;
}

float Noise::noise3(float x, float y, float z) const {
    float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    int X = static_cast<int>(fx) & 255, Y = static_cast<int>(fy) & 255, Z = static_cast<int>(fz) & 255;
    x -= fx;
    y -= fy;
    z -= fz;
    float u = fade(x), v = fade(y), w = fade(z);
    int A = perm_[X] + Y, AA = perm_[A] + Z, AB = perm_[A + 1] + Z;
    int B = perm_[X + 1] + Y, BA = perm_[B] + Z, BB = perm_[B + 1] + Z;
    return lerpf(
        lerpf(lerpf(grad3(perm_[AA], x, y, z), grad3(perm_[BA], x - 1, y, z), u),
              lerpf(grad3(perm_[AB], x, y - 1, z), grad3(perm_[BB], x - 1, y - 1, z), u), v),
        lerpf(lerpf(grad3(perm_[AA + 1], x, y, z - 1), grad3(perm_[BA + 1], x - 1, y, z - 1), u),
              lerpf(grad3(perm_[AB + 1], x, y - 1, z - 1), grad3(perm_[BB + 1], x - 1, y - 1, z - 1), u), v),
        w);
}

float Noise::fbm2(float x, float y, int octaves, float lacunarity, float gain) const {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise2(x, y);
        norm += amp;
        amp *= gain;
        x *= lacunarity;
        y *= lacunarity;
    }
    return sum / norm;
}

float Noise::fbm3(float x, float y, float z, int octaves, float lacunarity, float gain) const {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise3(x, y, z);
        norm += amp;
        amp *= gain;
        x *= lacunarity;
        y *= lacunarity;
        z *= lacunarity;
    }
    return sum / norm;
}

uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed) {
    uint32_t h = seed * 0x9E3779B1u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h = (h << 13) | (h >> 19);
    h *= 0xC2B2AE35u;
    h ^= static_cast<uint32_t>(y) * 0x27D4EB2Fu;
    h = (h << 11) | (h >> 21);
    h *= 0x165667B1u;
    h ^= static_cast<uint32_t>(z) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

}  // namespace oc
