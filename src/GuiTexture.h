#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace oc {

// Textur fuer die Oberflaeche: Schrift (5x7-Bitmap) und kleine Symbole.
constexpr int GUI_TEX_W = 256;
constexpr int GUI_TEX_H = 64;

struct GuiRect {
    int x, y, w, h;  // Pixel in der GUI-Textur (y = 0 ist die oberste Zeile)
};

// RGBA8, Zeile 0 = oben (entspricht v = 0 in OpenGL, wenn die Daten so hochgeladen werden)
std::vector<uint8_t> generateGuiPixels();

// Zeichen 32..126; unbekannte Zeichen ergeben ein Fragezeichen
GuiRect glyphRect(unsigned char c);
constexpr int GLYPH_W = 5;
constexpr int GLYPH_H = 8;
constexpr int GLYPH_ADVANCE = 6;

GuiRect iconHeartEmpty();
GuiRect iconHeartFull();
GuiRect iconHeartHalf();
GuiRect iconBubble();

// Breite eines Texts in Pixeln bei Skalierung 1
int textWidthPx(const std::string& s);

}  // namespace oc
