#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace oc {

enum class MenuScreen { None, Pause, Settings };

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
    W_VSYNC,
    W_FULLSCREEN,
    W_MODE,
};

struct Widget {
    enum Type { Button, Slider } type = Button;
    int id = W_NONE;
    std::string text;      // fertig formatierte Beschriftung (inkl. Wert)
    glm::vec2 pos{0.0f}, size{0.0f};
    float fraction = 0.0f;  // Slider: 0..1
};

struct MenuLayout {
    std::string title;
    glm::vec2 titlePos{0.0f};
    float scale = 1.0f;
    std::vector<Widget> widgets;
};

// Werte, die das Menue anzeigt
struct MenuState {
    int renderDistance = 10;
    float mouseSensitivity = 0.09f;
    float fov = 70.0f;
    bool vsync = true;
    bool fullscreen = false;
    bool creative = false;
};

MenuLayout buildMenu(MenuScreen screen, const MenuState& st, int w, int h);
int hitWidget(const MenuLayout& m, glm::vec2 p);  // Index in widgets oder -1

// Wert eines Sliders aus der Mausposition (auf Schritte gerastet)
int distanceFromFraction(float f);
float sensitivityFromFraction(float f);
float fovFromFraction(float f);
float sliderFraction(const Widget& w, float mouseX);  // 0..1

}  // namespace oc
