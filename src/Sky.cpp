#include "Sky.h"

#include <algorithm>
#include <cmath>

#include "Noise.h"

namespace oc {

namespace {
void put(std::vector<uint8_t>& px, int w, int x, int y, int r, int g, int b, int a) {
    uint8_t* p = &px[(static_cast<size_t>(y) * w + x) * 4];
    p[0] = static_cast<uint8_t>(std::max(0, std::min(255, r)));
    p[1] = static_cast<uint8_t>(std::max(0, std::min(255, g)));
    p[2] = static_cast<uint8_t>(std::max(0, std::min(255, b)));
    p[3] = static_cast<uint8_t>(std::max(0, std::min(255, a)));
}
}  // namespace

std::vector<uint8_t> sunPixels() {
    std::vector<uint8_t> px(static_cast<size_t>(SUN_PX) * SUN_PX * 4, 0);
    const float c = (SUN_PX - 1) * 0.5f;
    for (int y = 0; y < SUN_PX; ++y)
        for (int x = 0; x < SUN_PX; ++x) {
            const float dx = x - c, dy = y - c;
            const float r = std::sqrt(dx * dx + dy * dy);
            const int noise = static_cast<int>(hash3(x, y, 0, 501u) & 7u) - 3;
            if (r <= 4.3f) {  // heller Kern, nach aussen gelber
                const float t = r / 4.3f;
                put(px, SUN_PX, x, y, 255, static_cast<int>(252 - 40 * t) + noise, static_cast<int>(215 - 120 * t) + noise, 255);
            } else if (r <= 5.5f) {  // oranger Rand
                put(px, SUN_PX, x, y, 255, 168 + noise, 52, 255);
            } else if (r <= 7.7f) {  // Strahlen: Achsen und Diagonalen, nach aussen schwaecher
                const float ax = std::fabs(dx), ay = std::fabs(dy);
                if (ax < 0.9f || ay < 0.9f || std::fabs(ax - ay) < 0.9f) {
                    const int a = r <= 6.6f ? 225 : 150;
                    put(px, SUN_PX, x, y, 255, 205 + noise, 85, a);
                }
            }
        }
    return px;
}

std::vector<uint8_t> moonPixels() {
    const int W = MOON_PX * MOON_PHASES, H = MOON_PX;
    std::vector<uint8_t> px(static_cast<size_t>(W) * H * 4, 0);
    const float c = (MOON_PX - 1) * 0.5f, R = 6.3f;
    struct Crater {
        float x, y, r;
    };
    static const Crater craters[] = {{4.6f, 6.8f, 2.2f}, {9.6f, 4.2f, 1.5f}, {9.0f, 9.8f, 2.4f}, {12.0f, 7.0f, 0.9f}, {5.4f, 11.6f, 1.2f}};
    for (int p = 0; p < MOON_PHASES; ++p) {
        const float phi = static_cast<float>(p) * 3.14159265f / 4.0f;
        const glm::vec3 L(std::sin(phi), 0.0f, std::cos(phi));
        for (int y = 0; y < MOON_PX; ++y)
            for (int x = 0; x < MOON_PX; ++x) {
                const float dx = x - c, dy = y - c;
                if (dx * dx + dy * dy > R * R) continue;
                const float nx = dx / R, ny = dy / R;
                const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                const float lit = nx * L.x + ny * L.y + nz * L.z;
                const int noise = static_cast<int>(hash3(x, y, 0, 503u) & 7u) - 3;
                bool crater = false;
                for (const Crater& cr : craters) {
                    const float cx = x - cr.x, cy = y - cr.y;
                    crater = crater || cx * cx + cy * cy <= cr.r * cr.r;
                }
                const int X = p * MOON_PX + x;
                if (lit > 0.05f) {
                    const float shade = 0.72f + 0.28f * std::min(1.0f, lit * 2.0f);
                    const int base = crater ? 168 : 226;
                    put(px, W, X, y, static_cast<int>((base + noise) * shade), static_cast<int>((base + 4 + noise) * shade),
                        static_cast<int>((base + 16 + noise) * shade), 255);
                } else {  // unbeleuchtete Seite: nur ein schwacher Umriss
                    put(px, W, X, y, 44, 52, 74, 70);
                }
            }
    }
    return px;
}

std::vector<uint8_t> glowPixels() {
    std::vector<uint8_t> px(static_cast<size_t>(GLOW_PX) * GLOW_PX * 4, 0);
    const float c = (GLOW_PX - 1) * 0.5f;
    for (int y = 0; y < GLOW_PX; ++y)
        for (int x = 0; x < GLOW_PX; ++x) {
            const float d = std::sqrt((x - c) * (x - c) + (y - c) * (y - c)) / c;
            const float a = std::max(0.0f, 1.0f - d);
            put(px, GLOW_PX, x, y, 255, 255, 255, static_cast<int>(a * a * a * 255.0f));
        }
    return px;
}

std::vector<StarDef> generateStars(int count) {
    std::vector<StarDef> stars;
    stars.reserve(static_cast<size_t>(count));
    const float PI = 3.14159265f;
    for (int i = 0; i < count; ++i) {
        const uint32_t h1 = hash3(i, 1, 0, 77u), h2 = hash3(i, 2, 0, 77u), h3 = hash3(i, 3, 0, 77u), h4 = hash3(i, 4, 0, 77u);
        const float u = (h1 & 0xFFFFu) / 65535.0f * 2.0f - 1.0f;
        const float a = (h2 & 0xFFFFu) / 65535.0f * 2.0f * PI;
        const float r = std::sqrt(std::max(0.0f, 1.0f - u * u));
        StarDef s;
        s.dir = glm::vec3(r * std::cos(a), u, r * std::sin(a));
        const float k = (h3 & 0xFFFFu) / 65535.0f;
        if (k < 0.80f) {
            s.size = 1.5f;
            s.brightness = 0.35f + 0.35f * ((h4 & 0xFFu) / 255.0f);
        } else if (k < 0.95f) {
            s.size = 2.2f;
            s.brightness = 0.65f + 0.25f * ((h4 & 0xFFu) / 255.0f);
        } else {
            s.size = 3.2f;
            s.brightness = 0.9f + 0.1f * ((h4 & 0xFFu) / 255.0f);
        }
        s.phase = ((h4 >> 8) & 0xFFFFu) / 65535.0f;
        stars.push_back(s);
    }
    return stars;
}

}  // namespace oc
