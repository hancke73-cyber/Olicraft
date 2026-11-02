#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "GuiTexture.h"
#include "Inventory.h"
#include "Items.h"
#include "Menu.h"

namespace oc {

struct HudVertex {
    float x, y, u, v, r, g, b, a;
};

enum HudTex { HUD_TEX_GUI = 0, HUD_TEX_ATLAS = 1 };

// Sammelt 2D-Geometrie (Pixelkoordinaten, y nach unten) in Zeichenreihenfolge.
class HudBatch {
public:
    struct Cmd {
        int tex;
        size_t first, count;
    };
    std::vector<HudVertex> verts;
    std::vector<Cmd> cmds;

    void clear() {
        verts.clear();
        cmds.clear();
    }
    void rect(float x0, float y0, float x1, float y1, glm::vec4 c);
    void quad(const glm::vec2 p[4], const glm::vec2 uv[4], glm::vec4 c, int tex);
    void sprite(const GuiRect& r, float x, float y, float w, float h, glm::vec4 c = glm::vec4(1.0f));
    void text(float x, float y, float scale, const std::string& s, glm::vec4 c = glm::vec4(1.0f), bool shadow = true);
    void itemIcon(ItemId id, float cx, float cy, float size);
};

float guiScale(int windowHeight);

// Anordnung des Inventar-Fensters (Survival: Rucksack, Kreativ: zusaetzlich Auswahl aller Bloecke)
struct InvLayout {
    float s = 1.0f;
    bool creative = false;
    glm::vec2 panelPos{0.0f}, panelSize{0.0f};
    glm::vec2 paletteTop{0.0f};  // linke obere Ecke des Auswahlfelds (Kreativ)
    glm::vec2 storageTop{0.0f};  // linke obere Ecke der Rucksack-Reihen
    glm::vec2 hotbarTop{0.0f};   // linke obere Ecke der Hotbar im Fenster
    static constexpr int PALETTE_COLS = 9;

    static InvLayout make(int w, int h, bool creative);
    float pitch() const { return 18.0f * s; }
    glm::vec2 slotPos(int index) const;     // Inventar-Slot 0..35
    glm::vec2 palettePos(int index) const;  // Auswahlfeld (Kreativ)
    int hitSlot(glm::vec2 m) const;         // -1: kein Slot
    int hitPalette(glm::vec2 m) const;      // -1: kein Eintrag
    bool inside(glm::vec2 m) const;
};

struct HudState {
    bool show = true;
    bool creative = false;
    int health = 20;
    bool showAir = false;
    float air = 1.0f;  // 0..1
    const Inventory* inventory = nullptr;
    int selectedSlot = 0;
    bool inventoryOpen = false;
    ItemStack cursor;
    glm::vec2 mouse{0.0f};
    float breakProgress = 0.0f;
    float damageFlash = 0.0f;
    bool dead = false;
    bool underwater = false;
    bool underLava = false;
    bool burning = false;
    MenuLayout menu;  // Pause-/Einstellungsmenue (leer = geschlossen)
    int menuHover = -1;
    std::string message;
    float messageAlpha = 0.0f;
    std::string itemName;
    float itemNameAlpha = 0.0f;
};

void buildHud(HudBatch& b, const HudState& st, int w, int h);

}  // namespace oc
