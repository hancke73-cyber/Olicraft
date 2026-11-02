#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "GuiTexture.h"
#include "Chest.h"
#include "Crafting.h"
#include "Inventory.h"
#include "EditorUi.h"
#include "Items.h"
#include "Menu.h"

namespace oc {

struct HudVertex {
    float x, y, u, v, r, g, b, a;
};

enum HudTex { HUD_TEX_GUI = 0, HUD_TEX_ATLAS = 1, HUD_TEX_VIEW = 2, HUD_TEX_SKIN = 3 };

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
    bool creative = false;  // Auswahlfeld mit allen Gegenstaenden (Kreativ-Inventar)
    int craftSize = 0;      // 0 = kein Handwerksfeld, 2 = Handwerk im Inventar, 3 = Werkbank
    bool furnace = false;   // Ofenfenster (nutzt den Platz des 3x3-Rasters)
    bool chest = false;     // Truhenfenster (3 Reihen zu 9 Plaetzen anstelle des Handwerksrasters)
    glm::vec2 chestTop{0.0f};
    int tradeRows = 0;      // Handelsfenster: Anzahl Angebote (Reihen)
    glm::vec2 panelPos{0.0f}, panelSize{0.0f};
    glm::vec2 craftTop{0.0f};    // linke obere Ecke des Handwerksrasters
    glm::vec2 resultPos{0.0f};   // Ergebnisfeld
    glm::vec2 paletteTop{0.0f};  // linke obere Ecke des Auswahlfelds (Kreativ)
    glm::vec2 storageTop{0.0f};  // linke obere Ecke der Rucksack-Reihen
    glm::vec2 hotbarTop{0.0f};   // linke obere Ecke der Hotbar im Fenster
    static constexpr int PALETTE_COLS = 9;

    static InvLayout make(int w, int h, bool creative, int craftSize = 0, bool furnace = false, bool chest = false, int tradeRows = 0);
    float pitch() const { return 18.0f * s; }
    glm::vec2 slotPos(int index) const;     // Inventar-Slot 0..35
    glm::vec2 palettePos(int index) const;  // Auswahlfeld (Kreativ)
    int hitSlot(glm::vec2 m) const;         // -1: kein Slot
    int hitPalette(glm::vec2 m) const;      // -1: kein Eintrag
    int hitCraft(glm::vec2 m) const;        // Feld im Handwerksraster, -1: keins
    bool hitResult(glm::vec2 m) const;      // Ergebnisfeld
    glm::vec2 furnaceSlotPos(int which) const;  // 0 Eingabe, 1 Brennstoff, 2 Ausgabe
    int hitFurnace(glm::vec2 m) const;          // -1: keins
    glm::vec2 craftPos(int index) const;
    glm::vec2 chestPos(int index) const;  // Truhenplatz 0..26
    int hitChest(glm::vec2 m) const;      // -1: keiner
    glm::vec2 tradePos(int row) const { return chestTop + glm::vec2(0.0f, row * pitch()); }
    int hitTrade(glm::vec2 m) const;      // Angebot unter der Maus oder -1
    bool inside(glm::vec2 m) const;
};

// Rezeptbuch neben dem Handwerksfenster: Liste aller Rezepte, herstellbare zuerst. Klick fuellt das Raster.
struct BookEntryHud {
    ItemStack result;
    bool craftable = false;  // Zutaten vorhanden (und Raster gross genug)
    bool fits = true;        // passt ins aktuelle Raster (sonst wird eine Werkbank gebraucht)
    std::vector<Ingredient> ingredients;
};

struct BookHud {
    bool open = false;
    std::vector<BookEntryHud> entries;
    int scroll = 0;            // erste sichtbare Zeile
    bool onlyCraftable = false;
};

struct BookLayout {
    float s = 1.0f;
    glm::vec2 pos{0.0f}, size{0.0f};
    glm::vec2 gridTop{0.0f};
    glm::vec2 filterPos{0.0f}, filterSize{0.0f};
    int cols = 6, rows = 1;

    static BookLayout make(const InvLayout& inv, int windowW, int windowH);
    float pitch() const { return 18.0f * s; }
    glm::vec2 entryPos(int visibleIndex) const;       // visibleIndex = Position im sichtbaren Ausschnitt
    int hitEntry(glm::vec2 m, int total, int scroll) const;  // Index in die Eintragsliste oder -1
    bool hitFilter(glm::vec2 m) const;
    bool inside(glm::vec2 m) const;
    int maxScroll(int total) const;  // groesste sinnvolle erste Zeile
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
    bool paletteOpen = false;      // Kreativ-Auswahl statt Handwerk
    bool furnaceOpen = false;
    bool chestOpen = false;
    const Chest* chest = nullptr;
    BookHud book;
    bool tradeOpen = false;
    struct TradeRowHud {
        ItemStack cost, result;
        bool affordable = false;
    };
    std::vector<TradeRowHud> trades;
    ItemStack furnaceIn, furnaceFuel, furnaceOut;
    float furnaceBurn = 0.0f;      // Flamme: Rest der Brenndauer 0..1
    float furnaceCook = 0.0f;      // Pfeil: Fortschritt 0..1
    int craftSize = 0;             // 0, 2 oder 3
    ItemStack craftCells[9];
    ItemStack craftResult;
    std::string windowTitle;       // leer = Standard
    ItemStack cursor;
    glm::vec2 mouse{0.0f};
    float breakProgress = 0.0f;
    float damageFlash = 0.0f;
    bool dead = false;
    bool underwater = false;
    bool underLava = false;
    bool burning = false;
    EditorHud editor; // Textur-Editor
    MenuLayout menu;  // Pause-/Einstellungsmenue (leer = geschlossen)
    int menuHover = -1;
    std::vector<PreviewQuad> menuPreview;  // Spieler-Modell in den Einstellungen
    std::string message;
    float messageAlpha = 0.0f;
    std::string itemName;
    float itemNameAlpha = 0.0f;
};

void buildHud(HudBatch& b, const HudState& st, int w, int h);

}  // namespace oc
