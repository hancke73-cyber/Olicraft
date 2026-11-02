#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Image.h"
#include "Skins.h"
#include "TextureStore.h"

namespace oc {

// Ein bearbeitbarer Eintrag der Liste: ein Block (aufgeklapptes Kreuznetz aus bis zu 3 Kacheln), ein Gegenstand
// (einzelne Kachel) oder ein Figuren-Skin (ganzes Netzblatt).
struct EditorEntry {
    enum Kind { BlockNet, FlatTile, Skin } kind = FlatTile;
    std::string name;
    const char* group = "";
    int tiles[3] = {0, 0, 0};  // BlockNet: oben, unten, Seite. FlatTile: [0]
    SkinId skin = SKIN_PLAYER;
};
const std::vector<EditorEntry>& editorEntries();

enum class Tool { Pencil, Eraser, Fill, Picker };

// Fenster ueber das Netz fuer die Anzeige (Pixel im Ansichtsbild)
struct NetRegion {
    int x, y, w, h;
    char label;  // V vorn, H hinten, R rechts, L links, O oben, U unten, S Seite
};

// Vorschau: gedrehtes Modell als projizierte Vierecke (Koordinaten etwa im Bereich -1..1, y nach unten)
struct PreviewQuad {
    glm::vec2 p[4];
    glm::vec2 uv[4];
    float shade;
    bool skin;  // true: Skin-Textur, false: Blockatlas
};

class TextureEditor {
public:
    explicit TextureEditor(TextureStore& store);

    const std::vector<EditorEntry>& entries() const { return entries_; }
    int current() const { return current_; }
    const EditorEntry& entry() const { return entries_[static_cast<size_t>(current_)]; }
    void select(int index);

    // Ansichtsbild des aktuellen Eintrags (Zeile 0 oben); wird nach jeder Aenderung neu aufgebaut
    const Image& view() const { return view_; }
    std::vector<NetRegion> regions() const;
    bool editable(int vx, int vy) const;  // gehoert der Pixel zu einer Textur?

    // Werkzeuge
    Tool tool = Tool::Pencil;
    uint8_t color[4] = {200, 60, 40, 255};
    int brush = 1;  // 1..3
    int fillTolerance = 24;  // Fuellen: maximale Abweichung je Farbkanal

    void beginStroke();
    void paintAt(int vx, int vy);
    void paintLine(int x0, int y0, int x1, int y1);
    void endStroke();
    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool modified() const { return modified_; }

    // Dateien: speichern (vom Spiel beim Start geladen), Netz exportieren/importieren, Standard wiederherstellen
    bool save(std::string* message);
    bool exportNet(std::string* message);
    bool importNet(std::string* message);
    void resetToDefault(std::string* message);
    std::string exportPath() const;

    // Vorschau des gedrehten Modells
    std::vector<PreviewQuad> preview(float yaw, float pitch) const;

private:
    struct Snapshot {
        int n = 0;
        int tiles[3] = {0, 0, 0};
        Image images[3];
        Image skin;
    };
    bool mapPixel(int vx, int vy, int& tile, int& tx, int& ty) const;  // Kachel-Eintraege
    void writePixel(int vx, int vy, const uint8_t rgba[4]);
    void flood(int vx, int vy);
    void rebuildView();
    Snapshot snapshot() const;
    void restore(const Snapshot& s);
    int uniqueTiles(int out[3]) const;

    TextureStore& store_;
    std::vector<EditorEntry> entries_;
    int current_ = 0;
    Image view_;
    std::vector<Snapshot> undo_, redo_;
    bool stroking_ = false;
    bool modified_ = false;
};

}  // namespace oc
