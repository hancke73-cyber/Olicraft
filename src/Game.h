#pragma once
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Entities.h"
#include "Inventory.h"
#include "Menu.h"
#include "Player.h"
#include "Renderer.h"
#include "Settings.h"
#include "World.h"

struct GLFWwindow;

namespace oc {

struct Options {
    bool hasSeed = false;
    int32_t seed = 0;
    bool newWorld = false;
    bool noSave = false;
    std::string savePath = "saves/world.dat";
    std::string settingsPath = "settings.cfg";
    bool distanceCli = false;  // --distance gegeben: ueberschreibt die gespeicherte Einstellung
    bool vsyncCli = false;
    int renderDistance = 10;  // Chunks
    int width = 1280, height = 720;
    bool fullscreen = false;
    bool vsync = true;
    int forceMode = -1;  // -1: Spielstand/Standard, 0: Survival, 1: Kreativ
    bool mobs = true;

    // Test-/Entwicklungsoptionen
    std::string screenshotPath;  // wenn gesetzt: nach einigen Frames Screenshot speichern und beenden
    int screenshotFrames = 3;
    bool hasPos = false;
    glm::vec3 pos{0.0f};
    bool hasLook = false;
    float yaw = -90.0f, pitch = 0.0f;
    bool hasTime = false;
    float timeOfDay = 0.3f;
    bool fly = false;
    int benchFrames = 0;  // >0: automatisch fliegen, Zeiten messen, beenden
    std::string demo;     // "mobs", "water", "inv" (Szenen fuer Screenshots)
    int health = -1;
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
    void handleScroll(double dy);
    void handleMouseButton(int button, int action, int mods);
    void handleCursor(double x, double y);
    void handleFocus(bool focused);
    void handleResize(int w, int h);

    void setPaused(bool paused);
    void activateWidget(int index, const MenuLayout& menu, glm::vec2 mouse);
    void updateSlider(int widgetId, float mouseX);
    void persistSettings();
    MenuState menuState() const;
    void toggleFullscreen();
    void openInventory(bool table = false);
    void openFurnace(const glm::ivec3& pos);
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
    void damagePlayer(int amount, const glm::vec3& knockback);
    void die();
    void respawn();
    void applyDemo();
    int selfTest();
    void showMessage(const std::string& m);

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
    MenuScreen screen_ = MenuScreen::None;
    int dragWidget_ = W_NONE;
    EntityManager entities_;
    Inventory inventory_;
    ItemStack cursor_;
    ItemStack craft_[9];
    int craftSize_ = 0;          // 0, 2 (Inventar) oder 3 (Werkbank)
    bool paletteWindow_ = false;  // Kreativ-Auswahl im Inventarfenster
    bool furnaceWindow_ = false;  // Ofenfenster (craftSize_ == 3 belegt den Platz)
    glm::ivec3 furnacePos_{0};

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
    float burnTimer_ = 0.0f;     // Restzeit des Brennens
    float burnDamageTimer_ = 0.0f;
    glm::vec3 spawnPoint_{0.0f};

    int slot_ = 0;
    float timeOfDay_ = 0.30f;
    float gameTime_ = 0.0f;
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
    bool wantScreenshot_ = false;
    bool wantSave_ = false;
    float autosaveTimer_ = 0.0f;
    float jumpTapTimer_ = 0.0f;

    ItemId shownItem_ = ITEM_NONE;
    float itemNameTimer_ = 0.0f;
    std::string statusMsg_;
    float statusTimer_ = 0.0f;

    std::vector<Vertex> entityVerts_, entityHurtVerts_;

    double fpsTimer_ = 0.0;
    int fpsFrames_ = 0;
    double fps_ = 0.0;
    int frameCounter_ = 0;
    double benchSum_ = 0.0, benchMax_ = 0.0;
};

}  // namespace oc
