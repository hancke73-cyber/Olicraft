#include "Game.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

namespace oc {

namespace {

constexpr float DAY_SECONDS = 600.0f;
constexpr float REACH = 6.0f;
constexpr float MOUSE_SENS = 0.09f;

void APIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* msg,
                              const void*) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    std::fprintf(stderr, "[GL %u] %s\n", id, msg);
    (void)source;
    (void)type;
}

Game* gameFrom(GLFWwindow* w) { return static_cast<Game*>(glfwGetWindowUserPointer(w)); }

}  // namespace

Game::Game(const Options& opt) : opt_(opt) {
    for (int i = 0; i < HOTBAR_SIZE; ++i) hotbar_[i] = DEFAULT_HOTBAR[i];
}

Game::~Game() { shutdown(); }

bool Game::init() {
    if (!glfwInit()) {
        std::fprintf(stderr, "GLFW konnte nicht initialisiert werden.\n");
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    const bool glDebug = std::getenv("OLICRAFT_GL_DEBUG") != nullptr;
    if (glDebug) glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    GLFWmonitor* monitor = nullptr;
    int w = opt_.width, h = opt_.height;
    if (opt_.fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        w = mode->width;
        h = mode->height;
    }
    window_ = glfwCreateWindow(w, h, "OliCraft", monitor, nullptr);
    if (!window_) {
        std::fprintf(stderr,
                     "Fenster mit OpenGL 3.3 konnte nicht erstellt werden. Bitte Grafiktreiber aktualisieren.\n");
        return false;
    }
    winW_ = opt_.width;
    winH_ = opt_.height;
    glfwGetWindowPos(window_, &winX_, &winY_);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(opt_.vsync ? 1 : 0);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "OpenGL-Funktionen konnten nicht geladen werden.\n");
        return false;
    }
    std::printf("OpenGL %s | %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    if (glDebug && GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugCallback, nullptr);
    }
    glEnable(GL_MULTISAMPLE);

    glfwGetFramebufferSize(window_, &fbW_, &fbH_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, [](GLFWwindow* win, int key, int, int action, int mods) {
        gameFrom(win)->handleKey(key, action, mods);
    });
    glfwSetScrollCallback(window_, [](GLFWwindow* win, double, double dy) { gameFrom(win)->handleScroll(dy); });
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* win, int b, int a, int) { gameFrom(win)->handleMouseButton(b, a); });
    glfwSetCursorPosCallback(window_, [](GLFWwindow* win, double x, double y) { gameFrom(win)->handleCursor(x, y); });
    glfwSetWindowFocusCallback(window_, [](GLFWwindow* win, int f) { gameFrom(win)->handleFocus(f != 0); });
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* win, int fw, int fh) { gameFrom(win)->handleResize(fw, fh); });

    if (!renderer_.init()) return false;

    // Welt laden oder neu erzeugen
    SaveFile sf;
    bool loaded = false;
    if (!opt_.noSave && !opt_.newWorld && std::filesystem::exists(opt_.savePath)) {
        std::string err;
        if (readSaveFile(opt_.savePath, sf, &err)) {
            loaded = true;
        } else {
            std::fprintf(stderr, "Spielstand konnte nicht geladen werden (%s). Starte eine neue Welt.\n", err.c_str());
        }
    }

    int32_t seed;
    if (loaded) seed = sf.seed;
    else if (opt_.hasSeed) seed = opt_.seed;
    else seed = static_cast<int32_t>(std::random_device{}() & 0x7FFFFFFFu);
    std::printf("Welt-Seed: %d%s\n", seed, loaded ? " (Spielstand geladen)" : "");

    world_ = std::make_unique<World>(seed);
    if (loaded) {
        world_->importChunks(sf.chunks);
        player_.pos = sf.player.pos;
        player_.yaw = sf.player.yaw;
        player_.pitch = sf.player.pitch;
        player_.flying = sf.player.flying;
        slot_ = std::max(0, std::min(HOTBAR_SIZE - 1, sf.player.slot));
        timeOfDay_ = sf.player.timeOfDay;
    } else {
        player_.pos = world_->findSpawn();
        player_.yaw = -90.0f;
        player_.pitch = -10.0f;
    }
    if (opt_.hasPos) player_.pos = opt_.pos;
    if (opt_.hasLook) {
        player_.yaw = opt_.yaw;
        player_.pitch = opt_.pitch;
    }
    if (opt_.hasTime) timeOfDay_ = opt_.timeOfDay;
    if (opt_.fly) player_.flying = true;

    const bool shotMode = !opt_.screenshotPath.empty();
    world_->loadAround(player_.pos, shotMode ? opt_.renderDistance : 3);

    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    return true;
}

void Game::shutdown() {
    if (!window_) {
        glfwTerminate();
        return;
    }
    world_.reset();  // GL-Objekte der Chunks freigeben, solange der Kontext existiert
    renderer_.shutdown();
    glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
}

void Game::setPaused(bool p) {
    paused_ = p;
    firstMouse_ = true;
    if (!p) breakTimer_ = placeTimer_ = 0.3f;  // der Klick zum Fortsetzen soll nichts abbauen
    glfwSetInputMode(window_, GLFW_CURSOR, p ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    if (!p && glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void Game::toggleFullscreen() {
    GLFWmonitor* mon = glfwGetWindowMonitor(window_);
    if (mon) {
        glfwSetWindowMonitor(window_, nullptr, winX_, winY_, winW_, winH_, 0);
    } else {
        glfwGetWindowPos(window_, &winX_, &winY_);
        glfwGetWindowSize(window_, &winW_, &winH_);
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        glfwSetWindowMonitor(window_, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    firstMouse_ = true;
}

void Game::handleKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_ESCAPE:
            setPaused(!paused_);
            return;
        case GLFW_KEY_Q:
            if (paused_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
            return;
        case GLFW_KEY_F1:
            showHud_ = !showHud_;
            return;
        case GLFW_KEY_F2:
            wantScreenshot_ = true;
            return;
        case GLFW_KEY_F5:
            wantSave_ = true;
            return;
        case GLFW_KEY_F11:
            toggleFullscreen();
            return;
        default:
            break;
    }
    if (paused_) return;
    if (key == GLFW_KEY_F) {
        player_.flying = !player_.flying;
    } else if (key == GLFW_KEY_T) {
        timeOfDay_ = Renderer::daylight(timeOfDay_) > 0.5f ? 0.75f : 0.25f;
    } else if (key == GLFW_KEY_SPACE) {
        if (jumpTapTimer_ > 0.0f) {
            player_.flying = !player_.flying;
            jumpTapTimer_ = 0.0f;
        } else {
            jumpTapTimer_ = 0.3f;
        }
    } else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        slot_ = key - GLFW_KEY_1;
    }
}

void Game::handleScroll(double dy) {
    if (paused_) return;
    int d = dy > 0 ? -1 : (dy < 0 ? 1 : 0);
    slot_ = ((slot_ + d) % HOTBAR_SIZE + HOTBAR_SIZE) % HOTBAR_SIZE;
}

void Game::handleMouseButton(int button, int action) {
    if (paused_ && action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) setPaused(false);
}

void Game::handleCursor(double x, double y) {
    if (paused_) return;
    if (firstMouse_) {
        lastMx_ = x;
        lastMy_ = y;
        firstMouse_ = false;
        return;
    }
    player_.look(static_cast<float>(x - lastMx_), static_cast<float>(y - lastMy_), MOUSE_SENS);
    lastMx_ = x;
    lastMy_ = y;
}

void Game::handleFocus(bool focused) {
    if (!focused && !paused_ && opt_.benchFrames == 0 && opt_.screenshotPath.empty()) setPaused(true);
}

void Game::handleResize(int w, int h) {
    fbW_ = std::max(1, w);
    fbH_ = std::max(1, h);
}

void Game::update(float dt) {
    if (!paused_) {
        timeOfDay_ += dt / DAY_SECONDS;
        timeOfDay_ -= std::floor(timeOfDay_);
        jumpTapTimer_ = std::max(0.0f, jumpTapTimer_ - dt);

        MoveInput in;
        auto down = [&](int k) { return glfwGetKey(window_, k) == GLFW_PRESS; };
        in.forward = (down(GLFW_KEY_W) ? 1.0f : 0.0f) - (down(GLFW_KEY_S) ? 1.0f : 0.0f);
        in.strafe = (down(GLFW_KEY_D) ? 1.0f : 0.0f) - (down(GLFW_KEY_A) ? 1.0f : 0.0f);
        in.jump = down(GLFW_KEY_SPACE);
        in.sneak = down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT);
        in.sprint = down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL);
        if (opt_.benchFrames > 0) {  // automatischer Flug fuer Messungen
            player_.flying = true;
            in.forward = 1.0f;
            in.sprint = true;
            player_.yaw += 20.0f * dt;
            // Umbauten erzeugen, damit das sofortige Neumeshen mitgetestet wird (auch an Chunkgrenzen)
            if (frameCounter_ % 5 == 0) {
                int bx = static_cast<int>(std::floor(player_.pos.x)) + (frameCounter_ * 7) % 33 - 16;
                int bz = static_cast<int>(std::floor(player_.pos.z)) + (frameCounter_ * 13) % 33 - 16;
                int by = static_cast<int>(player_.pos.y) - 3 - (frameCounter_ % 9);
                BlockId cur = world_->getBlock(bx, by, bz);
                world_->setBlock(bx, by, bz, cur == AIR ? STONE : AIR);
            }
        }
        player_.update(dt, in, *world_);

        updateInteraction(dt);

        autosaveTimer_ += dt;
        if (autosaveTimer_ > 90.0f) {
            autosaveTimer_ = 0.0f;
            wantSave_ = true;
        }
    } else {
        target_ = RayHit{};
    }

    world_->update(player_.pos, opt_.renderDistance, 5.0);

    if (wantSave_) {
        wantSave_ = false;
        if (saveWorld()) {
            statusMsg_ = "Gespeichert";
            statusUntil_ = glfwGetTime() + 3.0;
        }
    }
}

void Game::updateInteraction(float dt) {
    const glm::vec3 eye = player_.eyePos();
    target_ = world_->raycast(eye, player_.front(), REACH);

    const bool left = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool right = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool middle = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

    breakTimer_ -= dt;
    placeTimer_ -= dt;
    if (!left) breakTimer_ = 0.0f;
    if (!right) placeTimer_ = 0.0f;

    if (left && breakTimer_ <= 0.0f && target_.hit) {
        BlockId b = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
        if (blockInfo(b).breakable) {
            world_->setBlock(target_.block.x, target_.block.y, target_.block.z, AIR);
            breakTimer_ = 0.22f;
        }
    }

    if (right && placeTimer_ <= 0.0f && target_.hit) {
        glm::ivec3 p = target_.block + target_.normal;
        BlockId cur = world_->getBlock(p.x, p.y, p.z);
        BlockId id = hotbar_[slot_];
        if ((cur == AIR || blockInfo(cur).liquid) && p.y >= 0 && p.y < CHUNK_H) {
            bool blocked = blockInfo(id).solid && player_.intersectsBlock(p.x, p.y, p.z);
            if (!blocked && (target_.normal.x != 0 || target_.normal.y != 0 || target_.normal.z != 0)) {
                world_->setBlock(p.x, p.y, p.z, id);
                placeTimer_ = 0.2f;
            }
        }
    }

    if (middle && !prevMiddle_ && target_.hit) {
        BlockId b = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
        if (b != AIR) hotbar_[slot_] = b;
    }
    prevLeft_ = left;
    prevRight_ = right;
    prevMiddle_ = middle;
}

void Game::render() {
    FrameInfo f;
    f.width = fbW_;
    f.height = fbH_;
    f.camPos = player_.eyePos();
    f.view = glm::lookAt(f.camPos, f.camPos + player_.front(), glm::vec3(0.0f, 1.0f, 0.0f));
    const float farPlane = static_cast<float>(opt_.renderDistance + 3) * CHUNK_W * 1.5f;
    f.proj = glm::perspective(glm::radians(70.0f), static_cast<float>(fbW_) / static_cast<float>(fbH_), 0.05f, farPlane);
    f.timeOfDay = timeOfDay_;
    f.renderDistance = opt_.renderDistance;
    f.underwater = blockInfo(world_->getBlock(static_cast<int>(std::floor(f.camPos.x)),
                                              static_cast<int>(std::floor(f.camPos.y)),
                                              static_cast<int>(std::floor(f.camPos.z))))
                       .liquid;
    f.hasTarget = target_.hit;
    f.target = target_.block;
    f.hotbarSlot = slot_;
    f.hotbar = hotbar_;
    f.showHud = showHud_;
    renderer_.render(*world_, f);
}

bool Game::saveWorld() {
    if (opt_.noSave) return true;
    SaveFile sf;
    sf.seed = world_->seed();
    sf.player.pos = player_.pos;
    sf.player.yaw = player_.yaw;
    sf.player.pitch = player_.pitch;
    sf.player.flying = player_.flying;
    sf.player.slot = slot_;
    sf.player.timeOfDay = timeOfDay_;
    world_->exportModified(sf.chunks);
    std::string err;
    if (!writeSaveFile(opt_.savePath, sf, &err)) {
        std::fprintf(stderr, "Speichern fehlgeschlagen: %s\n", err.c_str());
        return false;
    }
    std::printf("Gespeichert: %s (%zu veraenderte Chunks)\n", opt_.savePath.c_str(), sf.chunks.size());
    return true;
}

void Game::takeScreenshot() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories("screenshots", ec);
    std::time_t t = std::time(nullptr);
    char name[96];
    std::strftime(name, sizeof(name), "screenshots/olicraft_%Y%m%d_%H%M%S.bmp", std::localtime(&t));
    if (renderer_.saveScreenshot(name, fbW_, fbH_)) {
        std::printf("Screenshot: %s\n", name);
        statusMsg_ = std::string("Screenshot ") + name;
        statusUntil_ = glfwGetTime() + 3.0;
    }
}

void Game::updateTitle(double now) {
    ++fpsFrames_;
    if (now - fpsTimer_ < 0.5) return;
    fps_ = fpsFrames_ / (now - fpsTimer_);
    fpsFrames_ = 0;
    fpsTimer_ = now;

    char buf[256];
    const char* blockName = blockInfo(hotbar_[slot_]).name;
    int hour = static_cast<int>(std::fmod(timeOfDay_ * 24.0f + 18.0f, 24.0f));  // 0.25 -> 12 Uhr
    int minute = static_cast<int>(std::fmod(timeOfDay_ * 24.0f * 60.0f, 60.0f));
    std::snprintf(buf, sizeof(buf), "OliCraft | %.0f FPS | XYZ %.1f %.1f %.1f | %02d:%02d | Block: %s%s%s%s", fps_,
                  player_.pos.x, player_.pos.y, player_.pos.z, hour, minute, blockName,
                  player_.flying ? " | Flug" : "", paused_ ? " | PAUSE (Klick: weiter, Q: beenden)" : "",
                  now < statusUntil_ ? (" | " + statusMsg_).c_str() : "");
    glfwSetWindowTitle(window_, buf);
}

int Game::run() {
    if (!init()) return 1;

    const bool shotMode = !opt_.screenshotPath.empty();
    double last = glfwGetTime();
    fpsTimer_ = last;
    int exitCode = 0;

    while (!glfwWindowShouldClose(window_)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(std::min(now - last, 0.1));
        last = now;

        glfwPollEvents();
        if (paused_ && (opt_.benchFrames > 0 || shotMode)) setPaused(false);
        update(dt);
        render();
        ++frameCounter_;

        if (wantScreenshot_) {
            wantScreenshot_ = false;
            takeScreenshot();
        }
        if (shotMode && frameCounter_ >= opt_.screenshotFrames) {
            if (!renderer_.saveScreenshot(opt_.screenshotPath, fbW_, fbH_)) {
                std::fprintf(stderr, "Screenshot konnte nicht gespeichert werden.\n");
                exitCode = 1;
            } else {
                std::printf("Screenshot gespeichert: %s (%zu Chunks, %d sichtbar, %llu Quads)\n",
                            opt_.screenshotPath.c_str(), world_->loadedChunkCount(), renderer_.drawnChunks(),
                            static_cast<unsigned long long>(renderer_.drawnQuads()));
            }
            break;
        }

        glfwSwapBuffers(window_);
        updateTitle(now);

        if (opt_.benchFrames > 0) {
            double ft = (glfwGetTime() - now) * 1000.0;
            benchSum_ += ft;
            benchMax_ = std::max(benchMax_, ft);
            if (frameCounter_ >= opt_.benchFrames) {
                std::printf("Bench: %d Frames, Mittel %.2f ms, Max %.2f ms, %zu Chunks geladen, %d sichtbar, Pos %.0f %.0f %.0f\n",
                            frameCounter_, benchSum_ / frameCounter_, benchMax_, world_->loadedChunkCount(),
                            renderer_.drawnChunks(), player_.pos.x, player_.pos.y, player_.pos.z);
                break;
            }
        }
    }

    if (!shotMode) saveWorld();
    return exitCode;
}

}  // namespace oc
