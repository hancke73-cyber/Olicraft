#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Menu.h"
#include "TextureEditor.h"

namespace oc {

// Anzeigezustand des Textur-Editors (nicht Teil der Textur selbst)
struct EditorUiState {
    int listScroll = 0;  // erste sichtbare Zeile der Liste
    bool grid = true;
    bool net = true;
    bool rotate = true;
    float previewYaw = 0.6f;
    float previewPitch = 0.4f;
    std::string status;
};

struct EditorLayout {
    float s = 1.0f;                // Pixel je logischem Pixel
    glm::vec4 canvas{0.0f};        // x, y, Breite, Hoehe des angezeigten Bildes
    float zoom = 1.0f;             // Pixel je Bildpixel
    glm::vec4 preview{0.0f};       // Vorschaufeld
    glm::vec4 list{0.0f};          // Listenbereich (fuer das Mausrad)
    glm::vec2 titlePos{0.0f}, statusPos{0.0f};
    int listRows = 0;              // sichtbare Zeilen
    int listTotalRows = 0;         // Zeilen insgesamt (mit Gruppenueberschriften)
    std::vector<Widget> widgets;
};

// Zeichen- und Anzeigedaten fuer das HUD
struct EditorHud {
    bool open = false;
    EditorLayout layout;
    int hover = -1;                // Index in layout.widgets
    const Image* view = nullptr;
    std::vector<NetRegion> regions;
    bool grid = true, net = true;
    glm::ivec2 hoverPixel{-1, -1};
    bool hoverEditable = false;
    std::vector<PreviewQuad> quads;
    int previewSkin = -1;          // >= 0: Skin-Textur fuer die Vorschau
    std::string title, status;
    glm::vec4 color{1.0f};
};

float editorScale(int w, int h);
EditorLayout buildEditorLayout(int w, int h, const TextureEditor& ed, const EditorUiState& ui);
bool canvasPixel(const EditorLayout& l, const TextureEditor& ed, glm::vec2 m, int& x, int& y);
bool insideRect(const glm::vec4& r, glm::vec2 m);
int maxListScroll(const EditorLayout& l);
const std::vector<glm::vec3>& editorPalette();  // 32 Farben

}  // namespace oc
