#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace oc {

enum class MenuScreen { None, Pause, Settings, Editor, Main, NewWorld, Graphics };

enum WidgetId {
    W_NONE = 0,
    W_RESUME,
    W_SETTINGS,
    W_SAVE,
    W_QUIT,
    W_BACK,
    W_DISTANCE,
    W_SENSITIVITY,
    W_FOV,
    W_VOL_MASTER,
    W_VOL_EFFECTS,
    W_VOL_MUSIC,
    W_SKIN,
    W_MAIN_PLAY,
    W_MAIN_NEW,
    W_NEW_MODE,
    W_NEW_SEED,
    W_NEW_CREATE,
    W_NEW_BACK,
    W_GRAPHICS,
    W_GFX_SHADOWS,
    W_GFX_CLOUDS,
    W_GFX_PARTICLES,
    W_GFX_FILTER,
    W_GFX_ANISO,
    W_TO_MAIN,
    W_VSYNC,
    W_FULLSCREEN,
    W_MODE,
    W_EDITOR,
    // Textur-Editor
    W_ED_PENCIL,
    W_ED_ERASER,
    W_ED_FILL,
    W_ED_PICKER,
    W_ED_BRUSH,
    W_ED_UNDO,
    W_ED_REDO,
    W_ED_GRID,
    W_ED_NET,
    W_ED_ROTATE,
    W_ED_ROT_LEFT,
    W_ED_ROT_RIGHT,
    W_ED_SAVE,
    W_ED_EXPORT,
    W_ED_IMPORT,
    W_ED_RESET,
    W_ED_BACK,
    W_ED_R,
    W_ED_G,
    W_ED_B,
    W_ED_A,
    W_ED_LIST_UP,
    W_ED_LIST_DOWN,
    W_ED_SWATCH_BASE = 1000,  // + Index der Farbfelder
    W_ED_LIST_BASE = 2000,    // + Index des Eintrags
};

struct Widget {
    enum Type { Button, Slider } type = Button;
    int id = W_NONE;
    std::string text;      // fertig formatierte Beschriftung (inkl. Wert)
    glm::vec2 pos{0.0f}, size{0.0f};
    float fraction = 0.0f;  // Slider: 0..1
    bool swatch = false;    // Farbfeld statt Beschriftung
    glm::vec4 color{0.0f};  // Farbe des Farbfelds
    bool selected = false;  // hervorgehoben (aktives Werkzeug, gewaehlter Eintrag)
    bool enabled = true;    // false: Ueberschrift oder ausgegraut
    bool left = false;      // Text linksbuendig (Listen)
};

struct MenuLayout {
    std::string title;
    glm::vec2 titlePos{0.0f};
    float scale = 1.0f;
    float titleScale = 2.0f;
    std::string subtitle;
    float dim = 0.6f;  // Abdunklung des Hintergrunds 0..1
    std::vector<Widget> widgets;
    glm::vec4 skinPreview{0.0f};  // x, y, Breite, Hoehe der Skin-Vorschau (Breite 0: keine)
};

// Werte, die das Menue anzeigt
struct MenuState {
    int renderDistance = 10;
    float mouseSensitivity = 0.09f;
    float fov = 70.0f;
    bool vsync = true;
    float masterVolume = 0.8f, effectVolume = 1.0f, musicVolume = 0.5f;
    int skinSlot = 0;
    bool hasSave = false;       // Hauptmenue: gibt es einen Spielstand
    std::string seedText;       // Neue Welt: eingetippte Ziffern
    bool newCreative = false;   // Neue Welt: Spielmodus
    int shadows = 1;
    bool clouds = true, particles = true;
    int texFilter = 0, anisotropy = 8;
    bool fullscreen = false;
    bool creative = false;
};

MenuLayout buildMenu(MenuScreen screen, const MenuState& st, int w, int h);
int hitWidget(const MenuLayout& m, glm::vec2 p);  // Index in widgets oder -1

// Wert eines Sliders aus der Mausposition (auf Schritte gerastet)
int distanceFromFraction(float f);
float sensitivityFromFraction(float f);
float fovFromFraction(float f);
float volumeFromFraction(float f);  // in 5%-Schritten
float sliderFraction(const Widget& w, float mouseX);  // 0..1

}  // namespace oc
