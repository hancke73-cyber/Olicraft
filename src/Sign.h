#pragma once
#include <array>
#include <string>

namespace oc {

// Text eines Schilds: vier Zeilen zu hoechstens 15 Zeichen
struct SignText {
    static constexpr int LINES = 4;
    static constexpr size_t MAX_CHARS = 15;
    std::array<std::string, LINES> lines;
    bool empty() const {
        for (const std::string& l : lines)
            if (!l.empty()) return false;
        return true;
    }
    bool operator==(const SignText& o) const { return lines == o.lines; }
};

// Wandelt ein eingetipptes Zeichen (Unicode) in Text fuer das Schild: ASCII bleibt, Umlaute werden zu ae, oe, ue, ss; sonst leer
inline std::string signCharText(unsigned int cp) {
    if (cp >= 32 && cp < 127) return std::string(1, static_cast<char>(cp));
    switch (cp) {
        case 0xE4: case 0xC4: return "ae";
        case 0xF6: case 0xD6: return "oe";
        case 0xFC: case 0xDC: return "ue";
        case 0xDF: return "ss";
        default: return "";
    }
}

}  // namespace oc
