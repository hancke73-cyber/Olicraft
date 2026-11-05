#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Audio.h"
#include "Block.h"
#include "Camera.h"
#include "Crafting.h"
#include "EditorUi.h"
#include "TextureEditor.h"
#include "Entities.h"
#include "Inventory.h"
#include "Menu.h"
#include "Particles.h"
#include "Player.h"
#include "Renderer.h"
#include "Settings.h"
#include "TextureStore.h"
#include "World.h"
#include "Worlds.h"

struct GLFWwindow;

namespace oc {

struct Options {
    bool hasSeed = false;
    int32_t seed = 0;
    bool newWorld = false;
    bool noSave = false;
    std::string savePath = "saves/world.dat";  // Datei fuer --no-menu, --screenshot usw.
    std::string savesDir = "saves";            // Ordner der Welten (Hauptmenue)
    std::string settingsPath = "settings.cfg";
    std::string texturesDir = "textures";
    bool distanceCli = false;  // --distance gegeben: ueberschreibt die gespeicherte Einstellung
    bool vsyncCli = false;
    int renderDistance = 10;  // Chunks
    int width = 1280, height = 720;
    bool fullscreen = false;
    bool vsync = true;
    int forceMode = -1;  // -1: Spielstand/Standard, 0: Survival, 1: Kreativ
    bool mobs = true;
    bool sound = true;                // --no-sound
    std::string soundsDir = "sounds";  // --sounds PFAD

    // Test-/Entwicklungsoptionen
    std::string screenshotPath;  // wenn gesetzt: nach einigen Frames Screenshot speichern und beenden
    int screenshotFrames = 3;
    bool hasPos = false;
    glm::vec3 pos{0.0f};
    bool hasLook = false;
    float yaw = -90.0f, pitch = 0.0f;
    bool hasTime = false;
    bool noMenu = false;       // --no-menu: ohne Hauptmenue direkt ins Spiel
    bool gotoVillage = false;  // --village: zum naechsten Dorf springen (Test)
    int villageSide = 0;       // --village-side N: Blickrichtung 0..3
    int moonPhase = -1;  // --moon N: feste Mondphase 0..7 (Test)
    float timeOfDay = 0.3f;
    bool fly = false;
    int benchFrames = 0;  // >0: automatisch fliegen, Zeiten messen, beenden
    std::string demo;     // "mobs", "water", "inv" (Szenen fuer Screenshots)
    int health = -1;
    int viewMode = 0;  // 0 Ego, 1 von hinten, 2 von vorn
    bool selfTest = false;  // --selftest: Spielmechanik im echten Fenster pruefen
};

class Game {
public:
    explicit Game(const Options& opt);
    ~Game();
    int run();

private:
    bool init();
    void shutdown();

    void handleKey(int key, int action, int mods);
    void handleChar(unsigned int codepoint);
    void handleScroll(double dy);
    void handleMouseButton(int button, int action, int mods);
    void handleCursor(double x, double y);
    void handleFocus(bool focused);
    void handleResize(int w, int h);

    void setPaused(bool paused);
    void activateWidget(int index, const MenuLayout& menu, glm::vec2 mouse);
    void updateSlider(int widgetId, float mouseX);
    void persistSettings();
    // Textur-Editor
    void openEditor();
    void editorMouse(int button, int action, int mods);
    void editorCursor();
    bool editorKey(int key, int mods);
    void editorScroll(double dy);
    void activateEditorWidget(int id, glm::vec2 mouse);
    int hitEditorWidget(const EditorLayout& l, glm::vec2 m) const;
    MenuState menuState() const;
    void toggleFullscreen();
    void openInventory(bool table = false);
    void openFurnace(const glm::ivec3& pos);
    void openChest(const glm::ivec3& pos);
    void openTrade(int profession);
    bool buyTrade(int index);
    InvLayout invLayout() const;
    void useBed(const glm::ivec3& pos);
    void showMainMenu();
    void resetWorld(int32_t seed, bool creative, bool findSpawn = true);  // findSpawn=false: Welt bleibt leer (vor applySave)
    void refreshWorlds();
    bool loadWorld(const WorldEntry& e);
    void applySave(const SaveFile& sf);
    void createWorld();
    void selectWorld(int index);
    const std::vector<ItemId>& paletteItems() const;  // Eintraege des gewaehlten Reiters (oder die Suchtreffer)
    void setCreativeTab(int tab);
    void scrollPalette(int rows);
    void updateSearch();
    bool menuAtStart() const;
    // Ruestungsteil anlegen (tauscht mit dem getragenen Teil); true, wenn es eine Ruestung war
    bool equipArmor(ItemStack& from);
    // Rezeptbuch
    struct BookItem {
        const Recipe* recipe = nullptr;
        BookEntryHud hud;
    };
    std::vector<BookItem> buildBook() const;
    bool bookVisible() const { return inventoryOpen_ && craftSize_ > 0 && !furnaceWindow_ && !chestWindow_ && !tradeWindow_; }
    void autofillRecipe(const Recipe& r);
    void clampBookScroll(int total);
    void useHeldTool(int amount);
    void closeInventory();
    void inventoryClick(int button, int mods);
    void dropFromCursor(bool all);
    void dropSelected(bool all);
    void setCreative(bool on);

    void update(float dt);
    void updateInteraction(float dt);
    void updateSurvival(float dt);
    void breakBlock(const glm::ivec3& p);
    void placeBlockOrUse();
    void useBucket();
    void damagePlayer(int amount, const glm::vec3& knockback);
    void die();
    void respawn();
    void applyDemo();
    int selfTest();
    void showMessage(const std::string& m);
    // Ton
    void playSound(SoundId id, const glm::vec3& pos, float volume = 1.0f, float pitch = 1.0f);
    void uiSound(SoundId id, float volume = 1.0f);
    void updateAudio(float dt);
    void applyVolumes();

    void render();
    bool saveWorld();
    void takeScreenshot();
    void updateTitle(double now);
    glm::vec2 mousePx() const;

    Options opt_;
    GLFWwindow* window_ = nullptr;
    std::unique_ptr<World> world_;
    Renderer renderer_;
    Player player_;
    Settings settings_;
    TextureStore textures_;
    MenuScreen screen_ = MenuScreen::None;
    int dragWidget_ = W_NONE;
    std::unique_ptr<TextureEditor> editor_;
    std::unique_ptr<TextureEditor> skinPreviewEd_;  // nur fuer die Modellvorschau im Einstellungsmenue
    EditorUiState editorUi_;
    glm::ivec2 lastPaint_{-1, -1};
    bool painting_ = false;
    EntityManager entities_;
    Inventory inventory_;
    ItemStack cursor_;
    ItemStack craft_[9];
    int craftSize_ = 0;          // 0, 2 (Inventar) oder 3 (Werkbank)
    bool paletteWindow_ = false;  // Kreativ-Auswahl im Inventarfenster
    int creativeTab_ = 0;         // gewaehlter Reiter (CREATIVE_SEARCH_TAB: Suche)
    int creativeScroll_ = 0;      // erste sichtbare Reihe des Auswahlfelds
    std::string creativeQuery_;   // Suchtext
    std::vector<ItemId> searchResults_;
    bool furnaceWindow_ = false;  // Ofenfenster (craftSize_ == 3 belegt den Platz)
    glm::ivec3 furnacePos_{0};
    bool chestWindow_ = false;    // Truhenfenster
    bool tradeWindow_ = false;    // Handelsfenster
    int tradeProfession_ = 0;
    glm::ivec3 chestPos_{0};
    int bookScroll_ = 0;          // erste sichtbare Zeile des Rezeptbuchs
    bool bookOnlyCraftable_ = false;
    MenuScreen settingsFrom_ = MenuScreen::Pause;  // Hauptmenue oder Pause: wohin "Zurueck" fuehrt
    std::string newSeedText_;
    std::string newName_;
    bool newNameFocus_ = true;
    bool newNameFresh_ = false;  // Vorgabe im Namensfeld: der erste Tastendruck ersetzt sie
    bool newCreative_ = false;
    std::vector<WorldEntry> worlds_;  // Welten im Ordner (Hauptmenue)
    int worldSel_ = -1, worldScroll_ = 0;
    double lastWorldClick_ = -10.0;
    std::string currentWorld_;        // Name der geladenen Welt (leer: Hintergrundwelt des Hauptmenues)

    bool creative_ = false;
    int health_ = 20;
    bool dead_ = false;
    float invulnerable_ = 0.0f;
    float sinceDamage_ = 99.0f;
    float regenTimer_ = 0.0f;
    float damageFlash_ = 0.0f;
    float air_ = 15.0f;
    float drownTimer_ = 0.0f;
    float lavaTimer_ = 0.0f;
    float cactusTimer_ = 0.0f;
    float burnTimer_ = 0.0f;     // Restzeit des Brennens
    float burnDamageTimer_ = 0.0f;
    glm::vec3 spawnPoint_{0.0f};

    int slot_ = 0;
    float timeOfDay_ = 0.30f;
    float gameTime_ = 0.0f;
    int dayCount_ = 0;  // vergangene Tage dieser Sitzung (bestimmt die Mondphase)
    bool paused_ = false;
    bool inventoryOpen_ = false;
    bool firstMouse_ = true;
    double lastMx_ = 0, lastMy_ = 0;
    double curX_ = 0, curY_ = 0;
    int fbW_ = 1280, fbH_ = 720;
    int winW_cur_ = 1280, winH_cur_ = 720;

    // Fenstermodus-Wiederherstellung
    int winX_ = 100, winY_ = 100, winW_ = 1280, winH_ = 720;

    RayHit target_;
    float breakTimer_ = 0.0f, placeTimer_ = 0.0f, attackTimer_ = 0.0f, eatTimer_ = 0.0f;
    glm::ivec3 breakCell_{0};
    float breakTime_ = 0.0f;
    bool breaking_ = false;
    float breakProgress_ = 0.0f;
    bool prevMiddle_ = false;
    bool showHud_ = true;
    CameraMode camMode_ = CameraMode::First;
    float walkPhase_ = 0.0f, walkAmount_ = 0.0f;
    float deathAnim_ = 0.0f;
    bool wantScreenshot_ = false;
    bool wantSave_ = false;
    float autosaveTimer_ = 0.0f;
    float jumpTapTimer_ = 0.0f;

    ItemId shownItem_ = ITEM_NONE;
    float itemNameTimer_ = 0.0f;
    std::string statusMsg_;
    float statusTimer_ = 0.0f;

    EntityBatches entityBatches_;

    ParticleSystem particles_;
    std::vector<Vertex> particleVerts_;
    struct Emitter {
        glm::ivec3 pos;
        uint8_t kind;  // 0 Fackel, 1 Lava, 2 brennender Ofen
    };
    std::unordered_map<int64_t, std::vector<Emitter>> emitters_;
    size_t emitterScanIdx_ = 0;
    void updateParticles(float dt);

    AudioEngine audio_;
    float stepDist_ = 0.0f;       // seit dem letzten Schritt gelaufene Strecke
    bool wasInWater_ = false, wasInLava_ = false;
    bool furnaceWasLit_ = false;
public:
    const AudioEngine& audio() const { return audio_; }
private:

    double fpsTimer_ = 0.0;
    int fpsFrames_ = 0;
    double fps_ = 0.0;
    int frameCounter_ = 0;
    double benchSum_ = 0.0, benchMax_ = 0.0;
};

}  // namespace oc
