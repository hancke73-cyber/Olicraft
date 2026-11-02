#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace oc {

// RGBA8-Bild, Zeile 0 = oben (wie in Bilddateien)
struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;

    Image() = default;
    Image(int w_, int h_) : w(w_), h(h_), rgba(static_cast<size_t>(w_) * h_ * 4, 0) {}
    bool valid() const { return w > 0 && h > 0 && rgba.size() == static_cast<size_t>(w) * h * 4; }
    uint8_t* at(int x, int y) { return &rgba[(static_cast<size_t>(y) * w + x) * 4]; }
    const uint8_t* at(int x, int y) const { return &rgba[(static_cast<size_t>(y) * w + x) * 4]; }
};

// Laedt PNG, BMP oder TGA (andere Formate werden nicht unterstuetzt). Rueckgabe false mit Fehlertext.
bool loadImage(const std::string& path, Image& out, std::string* error);
// Schreibt ein PNG (legt fehlende Ordner an).
bool savePng(const std::string& path, const Image& img, std::string* error);

}  // namespace oc
