#pragma once
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "Block.h"
#include "Player.h"
#include "Renderer.h"
#include "World.h"

struct GLFWwindow;

namespace oc {

struct Options {
    bool hasSeed = false;
    int32_t seed = 0;
    bool newWorld = false;
    bool noSave = false;
    std::string savePath = "saves/world.dat";
    int renderDistance = 10;  // Chunks
    int width = 1280, height = 720;
    bool fullscreen = false;
    bool vsync = true;

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
    void handleMouseButton(int button, int action);
    void handleCursor(double x, double y);
    void handleFocus(bool focused);
    void handleResize(int w, int h);

    void setPaused(bool paused);
    void toggleFullscreen();
    void update(float dt);
    void updateInteraction(float dt);
    void render();
    bool saveWorld();
    void takeScreenshot();
    void updateTitle(double now);

    Options opt_;
    GLFWwindow* window_ = nullptr;
    std::unique_ptr<World> world_;
    Renderer renderer_;
    Player player_;

    BlockId hotbar_[HOTBAR_SIZE];
    int slot_ = 0;
    float timeOfDay_ = 0.30f;
    bool paused_ = false;
    bool firstMouse_ = true;
    double lastMx_ = 0, lastMy_ = 0;
    int fbW_ = 1280, fbH_ = 720;

    // Fenstermodus-Wiederherstellung
    int winX_ = 100, winY_ = 100, winW_ = 1280, winH_ = 720;

    RayHit target_;
    float breakTimer_ = 0.0f, placeTimer_ = 0.0f;
    bool prevLeft_ = false, prevRight_ = false, prevMiddle_ = false;
    bool showHud_ = true;
    bool wantScreenshot_ = false;
    bool wantSave_ = false;
    float autosaveTimer_ = 0.0f;
    float jumpTapTimer_ = 0.0f;
    bool prevJumpKey_ = false;

    double fpsTimer_ = 0.0;
    int fpsFrames_ = 0;
    double fps_ = 0.0;
    int frameCounter_ = 0;
    double benchSum_ = 0.0, benchMax_ = 0.0;
    std::string statusMsg_;
    double statusUntil_ = 0.0;
};

}  // namespace oc
