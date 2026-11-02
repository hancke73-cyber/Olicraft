#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace oc {

// Eine Welt ist eine Speicherdatei <Ordner>/<Name>.dat. Der Name ist der Dateiname ohne Endung.
struct WorldEntry {
    std::string name;
    std::string path;
    int32_t seed = 0;
    bool creative = false;
    int64_t modified = 0;  // Zeitstempel der Datei, nur zum Sortieren
};

constexpr size_t MAX_WORLD_NAME = 24;

// Macht aus einer Eingabe einen gueltigen Dateinamen: nur ASCII-Buchstaben, Ziffern, Leerzeichen, - und _,
// ohne Leerzeichen am Rand, hoechstens MAX_WORLD_NAME Zeichen, keine reservierten Windows-Namen. Leer, wenn nichts bleibt.
std::string sanitizeWorldName(const std::string& in);

// Zeichen, die beim Tippen eines Weltnamens erlaubt sind
bool worldNameChar(int c);

std::string worldPathFor(const std::string& dir, const std::string& name);

// Haengt " 2", " 3" ... an, bis es die Datei noch nicht gibt (Gross/Kleinschreibung zaehlt als gleich).
std::string uniqueWorldName(const std::string& dir, const std::string& base);

// Alle gueltigen *.dat im Ordner, neueste zuerst (bei gleicher Zeit nach Name). Defekte Dateien fehlen in der Liste.
std::vector<WorldEntry> listWorlds(const std::string& dir);

}  // namespace oc
