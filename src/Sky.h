#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace oc {

// Pixelgrafiken fuer den Himmel (RGBA8, Zeile 0 = oben). Ohne OpenGL, damit sie testbar sind.
constexpr int SUN_PX = 16;
constexpr int MOON_PX = 16;
constexpr int MOON_PHASES = 8;  // 0 Vollmond, 2 zunehmend (rechte Haelfte), 4 Neumond, 6 abnehmend
constexpr int GLOW_PX = 64;

std::vector<uint8_t> sunPixels();   // 16x16: Scheibe mit Strahlen
std::vector<uint8_t> moonPixels();  // (8*16) x 16: alle Phasen nebeneinander, mit Kratern
std::vector<uint8_t> glowPixels();  // 64x64: weicher Lichtkranz (weiss, Alpha faellt nach aussen ab)

struct StarDef {
    glm::vec3 dir;     // Einheitsvektor
    float size;        // Punktgroesse in Pixeln (bei 720p)
    float brightness;  // 0..1
    float phase;       // 0..1, fuer Funkeln und Farbton
};
std::vector<StarDef> generateStars(int count);

}  // namespace oc
