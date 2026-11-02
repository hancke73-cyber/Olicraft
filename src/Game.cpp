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

#include "Noise.h"

namespace oc {

namespace {

constexpr float DAY_SECONDS = 600.0f;
constexpr float REACH = 6.0f;
constexpr float MELEE_REACH = 3.6f;
constexpr float MOUSE_SENS = 0.09f;
constexpr float MAX_AIR = 15.0f;

void APIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* msg,
                              const void*) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    std::fprintf(stderr, "[GL %u] %s\n", id, msg);
    (void)source;
    (void)type;
}

Game* gameFrom(GLFWwindow* w) { return static_cast<Game*>(glfwGetWindowUserPointer(w)); }

}  // namespace

Game::Game(const Options& opt) : opt_(opt) {}

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
    glfwGetWindowSize(window_, &winW_cur_, &winH_cur_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, [](GLFWwindow* win, int key, int, int action, int mods) {
        gameFrom(win)->handleKey(key, action, mods);
    });
    glfwSetScrollCallback(window_, [](GLFWwindow* win, double, double dy) { gameFrom(win)->handleScroll(dy); });
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* win, int b, int a, int m) {
        gameFrom(win)->handleMouseButton(b, a, m);
    });
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
    entities_ = EntityManager(static_cast<uint32_t>(seed) ^ 0xA5A5u);
    if (loaded) {
        world_->importChunks(sf.chunks);
        player_.pos = sf.player.pos;
        player_.yaw = sf.player.yaw;
        player_.pitch = sf.player.pitch;
        player_.flying = sf.player.flying;
        slot_ = std::max(0, std::min(Inventory::HOTBAR - 1, sf.player.slot));
        timeOfDay_ = sf.player.timeOfDay;
        health_ = sf.player.health;
        if (sf.player.hasInventory) {
            creative_ = sf.player.creative;
            inventory_ = sf.player.inventory;
        } else {
            // Spielstand aus Version 1 (kreativ, feste Hotbar)
            creative_ = true;
            for (int i = 0; i < Inventory::HOTBAR; ++i) inventory_.slots[i] = {DEFAULT_HOTBAR[i], 64};
        }
        spawnPoint_ = player_.pos;
    } else {
        player_.pos = world_->findSpawn();
        player_.yaw = -90.0f;
        player_.pitch = -10.0f;
        spawnPoint_ = player_.pos;
    }
    if (opt_.forceMode >= 0) creative_ = opt_.forceMode == 1;
    if (creative_ && inventory_.isEmpty())
        for (int i = 0; i < Inventory::HOTBAR; ++i) inventory_.slots[i] = {DEFAULT_HOTBAR[i], 64};
    if (opt_.hasPos) player_.pos = opt_.pos;
    if (opt_.hasLook) {
        player_.yaw = opt_.yaw;
        player_.pitch = opt_.pitch;
    }
    if (opt_.hasTime) timeOfDay_ = opt_.timeOfDay;
    if (opt_.fly && creative_) player_.flying = true;
    if (!creative_) player_.flying = false;
    if (opt_.health > 0) health_ = std::min(20, opt_.health);

    const bool shotMode = !opt_.screenshotPath.empty();
    world_->loadAround(player_.pos, shotMode ? opt_.renderDistance : 3);
    if (!opt_.demo.empty()) applyDemo();

    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    if (opt_.demo == "inv") openInventory();
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

void Game::showMessage(const std::string& m) {
    statusMsg_ = m;
    statusTimer_ = 3.0f;
}

glm::vec2 Game::mousePx() const {
    const double sx = winW_cur_ > 0 ? static_cast<double>(fbW_) / winW_cur_ : 1.0;
    const double sy = winH_cur_ > 0 ? static_cast<double>(fbH_) / winH_cur_ : 1.0;
    return glm::vec2(static_cast<float>(curX_ * sx), static_cast<float>(curY_ * sy));
}

void Game::setPaused(bool p) {
    paused_ = p;
    firstMouse_ = true;
    if (!p) breakTimer_ = placeTimer_ = attackTimer_ = 0.3f;  // der Klick zum Fortsetzen soll nichts abbauen
    const bool freeCursor = p || inventoryOpen_;
    glfwSetInputMode(window_, GLFW_CURSOR, freeCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    if (!freeCursor && glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
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

void Game::openInventory() {
    if (dead_) return;
    inventoryOpen_ = true;
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwGetWindowSize(window_, &winW_cur_, &winH_cur_);
    curX_ = winW_cur_ * 0.5;
    curY_ = winH_cur_ * 0.5;
    glfwSetCursorPos(window_, curX_, curY_);
}

void Game::closeInventory() {
    if (!inventoryOpen_) return;
    if (!cursor_.empty()) {
        int left = creative_ ? 0 : inventory_.add(cursor_.id, cursor_.count);
        if (left > 0) {
            cursor_.count = left;
            dropFromCursor(true);
        }
        cursor_.clear();
    }
    inventoryOpen_ = false;
    firstMouse_ = true;
    breakTimer_ = placeTimer_ = attackTimer_ = 0.3f;
    if (!paused_) glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Game::dropFromCursor(bool all) {
    if (cursor_.empty()) return;
    ItemStack drop = cursor_;
    if (!all) drop.count = 1;
    cursor_.count -= drop.count;
    if (cursor_.count <= 0) cursor_.clear();
    const glm::vec3 f = player_.front();
    entities_.spawnItem(drop, player_.eyePos() + f * 0.4f - glm::vec3(0.0f, 0.2f, 0.0f), f * 4.5f + glm::vec3(0.0f, 2.0f, 0.0f),
                        1.5f);
}

void Game::dropSelected(bool all) {
    ItemStack& s = inventory_.slots[slot_];
    if (s.empty() || creative_) return;
    ItemStack drop = s;
    if (!all) drop.count = 1;
    inventory_.take(slot_, drop.count);
    const glm::vec3 f = player_.front();
    entities_.spawnItem(drop, player_.eyePos() + f * 0.4f - glm::vec3(0.0f, 0.2f, 0.0f), f * 4.5f + glm::vec3(0.0f, 2.0f, 0.0f),
                        1.5f);
}

void Game::inventoryClick(int button, int mods) {
    const bool right = button == GLFW_MOUSE_BUTTON_RIGHT;
    const InvLayout layout = InvLayout::make(fbW_, fbH_, creative_);
    const glm::vec2 m = mousePx();

    const int pal = layout.hitPalette(m);
    if (pal >= 0) {
        const ItemId id = creativeItem(pal);
        if (!cursor_.empty() && cursor_.id == id) cursor_.clear();  // zurueckgeben
        else cursor_ = {id, right ? 1 : itemInfo(id).maxStack};
        return;
    }
    const int slot = layout.hitSlot(m);
    if (slot >= 0) {
        if ((mods & GLFW_MOD_SHIFT) && !right) inventory_.quickMove(slot);
        else Inventory::click(inventory_.slots[slot], cursor_, right);
        return;
    }
    if (!layout.inside(m)) dropFromCursor(!right);
}

void Game::setCreative(bool on) {
    if (creative_ == on) return;
    creative_ = on;
    if (!on) player_.flying = false;
    if (on && inventory_.isEmpty())
        for (int i = 0; i < Inventory::HOTBAR; ++i) inventory_.slots[i] = {DEFAULT_HOTBAR[i], 64};
    showMessage(on ? "Modus: Kreativ" : "Modus: Survival");
}

void Game::handleKey(int key, int action, int mods) {
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_ESCAPE:
            if (inventoryOpen_) closeInventory();
            else setPaused(!paused_);
            return;
        case GLFW_KEY_Q:
            if (paused_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
            else if (!inventoryOpen_ && !dead_) dropSelected((mods & GLFW_MOD_CONTROL) != 0);
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
    if (dead_) {
        if (key == GLFW_KEY_SPACE) respawn();
        return;
    }
    if (key == GLFW_KEY_E) {
        if (inventoryOpen_) closeInventory();
        else openInventory();
        return;
    }
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        slot_ = key - GLFW_KEY_1;
        return;
    }
    if (inventoryOpen_) return;
    if (key == GLFW_KEY_G) {
        setCreative(!creative_);
    } else if (key == GLFW_KEY_F) {
        if (creative_) player_.flying = !player_.flying;
    } else if (key == GLFW_KEY_T) {
        timeOfDay_ = Renderer::daylight(timeOfDay_) > 0.5f ? 0.75f : 0.25f;
    } else if (key == GLFW_KEY_SPACE) {
        if (jumpTapTimer_ > 0.0f) {
            if (creative_) player_.flying = !player_.flying;
            jumpTapTimer_ = 0.0f;
        } else {
            jumpTapTimer_ = 0.3f;
        }
    }
}

void Game::handleScroll(double dy) {
    if (paused_ || inventoryOpen_ || dead_) return;
    int d = dy > 0 ? -1 : (dy < 0 ? 1 : 0);
    slot_ = ((slot_ + d) % Inventory::HOTBAR + Inventory::HOTBAR) % Inventory::HOTBAR;
}

void Game::handleMouseButton(int button, int action, int mods) {
    if (action != GLFW_PRESS) return;
    if (paused_) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) setPaused(false);
        return;
    }
    if (inventoryOpen_ && (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT))
        inventoryClick(button, mods);
}

void Game::handleCursor(double x, double y) {
    curX_ = x;
    curY_ = y;
    if (paused_ || inventoryOpen_) return;
    if (firstMouse_) {
        lastMx_ = x;
        lastMy_ = y;
        firstMouse_ = false;
        return;
    }
    if (!dead_) player_.look(static_cast<float>(x - lastMx_), static_cast<float>(y - lastMy_), MOUSE_SENS);
    lastMx_ = x;
    lastMy_ = y;
}

void Game::handleFocus(bool focused) {
    if (!focused && !paused_ && !inventoryOpen_ && opt_.benchFrames == 0 && opt_.screenshotPath.empty())
        setPaused(true);
}

void Game::handleResize(int w, int h) {
    fbW_ = std::max(1, w);
    fbH_ = std::max(1, h);
    glfwGetWindowSize(window_, &winW_cur_, &winH_cur_);
}

// ---------------------------------------------------------------------------------------------------------------
// Spielmechanik
// ---------------------------------------------------------------------------------------------------------------

void Game::damagePlayer(int amount, const glm::vec3& knockback) {
    if (creative_ || dead_ || amount <= 0) return;
    if (invulnerable_ > 0.0f && glm::length(knockback) > 0.0f) return;  // kurzer Schutz nach einem Treffer
    health_ -= amount;
    sinceDamage_ = 0.0f;
    damageFlash_ = 1.0f;
    if (glm::length(knockback) > 0.0f) {
        invulnerable_ = 0.5f;
        player_.vel += knockback;
        player_.onGround = false;
    }
    if (health_ <= 0) die();
}

void Game::die() {
    health_ = 0;
    dead_ = true;
    burnTimer_ = 0.0f;
    cursor_.clear();
    closeInventory();
    inventoryOpen_ = false;
    player_.flying = false;
}

void Game::respawn() {
    dead_ = false;
    health_ = 20;
    air_ = MAX_AIR;
    burnTimer_ = 0.0f;
    player_.pos = spawnPoint_;
    player_.vel = glm::vec3(0.0f);
    player_.landedFall = 0.0f;
    sinceDamage_ = 99.0f;
    damageFlash_ = 0.0f;
    world_->loadAround(player_.pos, 1);
}

void Game::updateSurvival(float dt) {
    invulnerable_ = std::max(0.0f, invulnerable_ - dt);
    sinceDamage_ += dt;
    damageFlash_ = std::max(0.0f, damageFlash_ - dt * 2.5f);

    // Fallschaden
    if (player_.landedFall > 0.0f) {
        const float fall = player_.landedFall;
        player_.landedFall = 0.0f;
        // wie Minecraft: ab mehr als 3 Bloecken Fallhoehe, je Block darueber ein Schaden (aufgerundet)
        const int dmg = static_cast<int>(std::ceil(fall - 3.0f - 0.02f));
        if (dmg > 0 && !creative_) damagePlayer(dmg, glm::vec3(0.0f));
    }

    // Luft / Ertrinken
    if (player_.headInWater && !creative_) {
        air_ -= dt;
        if (air_ <= 0.0f) {
            air_ = 0.0f;
            drownTimer_ += dt;
            if (drownTimer_ >= 1.0f) {
                drownTimer_ = 0.0f;
                damagePlayer(2, glm::vec3(0.0f));
            }
        }
    } else {
        air_ = std::min(MAX_AIR, air_ + dt * 5.0f);
        drownTimer_ = 0.0f;
    }

    // Lava: Schaden und Brennen (Wasser loescht)
    if (creative_) {
        burnTimer_ = 0.0f;
        lavaTimer_ = 0.0f;
    } else {
        if (player_.inLava) {
            lavaTimer_ += dt;
            burnTimer_ = 8.0f;
            if (lavaTimer_ >= 0.5f) {
                lavaTimer_ = 0.0f;
                damagePlayer(2, glm::vec3(0.0f));
            }
        } else {
            lavaTimer_ = 0.0f;
        }
        if (burnTimer_ > 0.0f) {
            if (player_.inWater && !player_.inLava) {
                burnTimer_ = 0.0f;
            } else {
                burnTimer_ -= dt;
                burnDamageTimer_ += dt;
                if (burnDamageTimer_ >= 1.0f) {
                    burnDamageTimer_ = 0.0f;
                    damagePlayer(1, glm::vec3(0.0f));
                }
            }
        } else {
            burnDamageTimer_ = 0.0f;
        }
    }

    // Treffer durch Mobs
    for (const PlayerHit& h : entities_.playerHits()) damagePlayer(h.damage, h.knockback);
    entities_.playerHits().clear();

    // Regeneration
    if (!creative_ && health_ < 20 && sinceDamage_ > 5.0f) {
        regenTimer_ += dt;
        if (regenTimer_ >= 1.5f) {
            regenTimer_ = 0.0f;
            ++health_;
        }
    }
}

void Game::update(float dt) {
    const bool shotMode = !opt_.screenshotPath.empty();
    if (!paused_) {
        gameTime_ += dt;
        timeOfDay_ += dt / DAY_SECONDS;
        timeOfDay_ -= std::floor(timeOfDay_);
        jumpTapTimer_ = std::max(0.0f, jumpTapTimer_ - dt);

        MoveInput in;
        if (!inventoryOpen_ && !dead_) {
            auto down = [&](int k) { return glfwGetKey(window_, k) == GLFW_PRESS; };
            in.forward = (down(GLFW_KEY_W) ? 1.0f : 0.0f) - (down(GLFW_KEY_S) ? 1.0f : 0.0f);
            in.strafe = (down(GLFW_KEY_D) ? 1.0f : 0.0f) - (down(GLFW_KEY_A) ? 1.0f : 0.0f);
            in.jump = down(GLFW_KEY_SPACE);
            in.sneak = down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT);
            in.sprint = down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL);
        }
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
        if (!creative_) player_.flying = false;
        player_.update(dt, in, *world_);

        updateSurvival(dt);
        if (!dead_ && !inventoryOpen_) updateInteraction(dt);
        else {
            target_ = RayHit{};
            breaking_ = false;
            breakProgress_ = 0.0f;
        }

        EntityEnv env;
        env.daylight = Renderer::daylight(timeOfDay_);
        env.creative = creative_;
        env.playerDead = dead_;
        env.allowSpawns = opt_.mobs && !shotMode;
        entities_.update(dt, *world_, player_, inventory_, env);

        autosaveTimer_ += dt;
        if (autosaveTimer_ > 90.0f) {
            autosaveTimer_ = 0.0f;
            wantSave_ = true;
        }
        statusTimer_ = std::max(0.0f, statusTimer_ - dt);
        itemNameTimer_ = std::max(0.0f, itemNameTimer_ - dt);
        world_->tick(dt);
    } else {
        target_ = RayHit{};
    }

    // Anzeige des gewaehlten Gegenstands
    {
        const ItemId cur = inventory_.slots[slot_].empty() ? ITEM_NONE : inventory_.slots[slot_].id;
        if (cur != shownItem_) {
            shownItem_ = cur;
            itemNameTimer_ = cur == ITEM_NONE ? 0.0f : 2.0f;
        }
    }

    world_->update(player_.pos, opt_.renderDistance, 5.0);

    if (wantSave_) {
        wantSave_ = false;
        if (saveWorld()) showMessage("Gespeichert");
    }
}

void Game::breakBlock(const glm::ivec3& p) {
    const BlockId b = world_->getBlock(p.x, p.y, p.z);
    if (!blockInfo(b).breakable) return;
    world_->setBlock(p.x, p.y, p.z, AIR);
    if (!creative_) {
        ItemId drop = blockDropItem(b);
        // Laub gibt nur manchmal etwas her
        if (b == LEAVES && (hash3(p.x, p.y, p.z, 99u) & 7u) >= 3u) drop = ITEM_NONE;
        if (drop != ITEM_NONE)
            entities_.spawnItem({drop, 1}, glm::vec3(p.x + 0.5f, p.y + 0.25f, p.z + 0.5f),
                                glm::vec3(static_cast<float>((hash3(p.x, p.y, p.z, 5u) & 15u)) / 8.0f - 1.0f, 3.0f,
                                          static_cast<float>((hash3(p.x, p.y, p.z, 6u) & 15u)) / 8.0f - 1.0f),
                                0.3f);
    }
}

void Game::placeBlockOrUse() {
    ItemStack& sel = inventory_.slots[slot_];
    if (sel.empty()) return;

    if (sel.id == ITEM_MEAT) {
        if (!creative_ && health_ < 20 && eatTimer_ <= 0.0f) {
            health_ = std::min(20, health_ + 4);
            inventory_.take(slot_, 1);
            eatTimer_ = 0.8f;
        }
        return;
    }
    if (!itemIsBlock(sel.id) || !target_.hit) return;

    glm::ivec3 p = target_.block + target_.normal;
    if (target_.normal.x == 0 && target_.normal.y == 0 && target_.normal.z == 0) return;
    if (p.y < 0 || p.y >= CHUNK_H) return;
    const BlockId cur = world_->getBlock(p.x, p.y, p.z);
    if (cur != AIR && !blockInfo(cur).liquid) return;
    const BlockId id = itemBlock(sel.id);
    if (blockInfo(id).solid && player_.intersectsBlock(p.x, p.y, p.z)) return;
    if (world_->setBlock(p.x, p.y, p.z, id)) {
        placeTimer_ = 0.2f;
        if (!creative_) inventory_.take(slot_, 1);
    }
}

void Game::updateInteraction(float dt) {
    const glm::vec3 eye = player_.eyePos();
    const glm::vec3 dir = player_.front();
    target_ = world_->raycast(eye, dir, REACH);

    float mobT = 0.0f;
    Mob* mob = entities_.raycastMob(eye, dir, MELEE_REACH, mobT);
    const float blockDist =
        target_.hit ? glm::length(glm::vec3(target_.block) + glm::vec3(0.5f) - eye) - 0.5f : 1e9f;
    const bool mobFirst = mob && mobT <= blockDist;

    const bool left = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool right = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool middle = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

    breakTimer_ -= dt;
    placeTimer_ -= dt;
    attackTimer_ -= dt;
    eatTimer_ -= dt;
    if (!right) placeTimer_ = std::min(placeTimer_, 0.0f);

    // Linke Maustaste: angreifen oder abbauen
    breakProgress_ = 0.0f;
    if (left && mobFirst) {
        breaking_ = false;
        if (attackTimer_ <= 0.0f) {
            entities_.hurtMob(*mob, creative_ ? 20 : 2, player_.pos);
            attackTimer_ = 0.4f;
        }
    } else if (left && target_.hit) {
        const BlockId b = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
        const BlockInfo& bi = blockInfo(b);
        if (bi.breakable) {
            if (creative_) {
                if (breakTimer_ <= 0.0f) {
                    breakBlock(target_.block);
                    breakTimer_ = 0.22f;
                }
            } else {
                if (!breaking_ || breakCell_ != target_.block) {
                    breaking_ = true;
                    breakCell_ = target_.block;
                    breakTime_ = 0.0f;
                }
                breakTime_ += dt;
                if (breakTime_ >= bi.hardness) {
                    breakBlock(target_.block);
                    breaking_ = false;
                    breakTimer_ = 0.15f;
                } else {
                    breakProgress_ = breakTime_ / bi.hardness;
                }
            }
        }
    } else {
        breaking_ = false;
        breakTimer_ = std::min(breakTimer_, 0.0f);
    }

    // Rechte Maustaste: setzen / essen
    if (right && placeTimer_ <= 0.0f) placeBlockOrUse();

    // Mittlere Maustaste: Block uebernehmen
    if (middle && !prevMiddle_ && target_.hit) {
        const BlockId b = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
        if (b != AIR && itemValid(static_cast<ItemId>(b))) {
            const ItemId id = static_cast<ItemId>(b);
            // vorhandenen Slot waehlen, sonst (kreativ) in den aktiven Slot legen
            int found = -1;
            for (int i = 0; i < Inventory::HOTBAR; ++i)
                if (inventory_.slots[i].id == id) found = i;
            if (found >= 0) slot_ = found;
            else if (creative_) inventory_.slots[slot_] = {id, 64};
        }
    }
    prevMiddle_ = middle;
}

// ---------------------------------------------------------------------------------------------------------------
// Szenen fuer Screenshots (Entwicklung)
// ---------------------------------------------------------------------------------------------------------------

void Game::applyDemo() {
    const glm::vec3 f(std::cos(glm::radians(player_.yaw)), 0.0f, std::sin(glm::radians(player_.yaw)));
    auto groundAt = [&](float x, float z) {
        int y = CHUNK_H - 2;
        while (y > 1 && !isSolid(world_->getBlock(static_cast<int>(std::floor(x)), y, static_cast<int>(std::floor(z))))) --y;
        return static_cast<float>(y + 1);
    };
    if (opt_.demo == "mobs") {
        const MobType kinds[4] = {MobType::Pig, MobType::Cow, MobType::Sheep, MobType::Zombie};
        for (int i = 0; i < 4; ++i) {
            glm::vec3 p = player_.pos + f * (4.0f + 0.4f * i) + glm::vec3(-f.z, 0.0f, f.x) * (-4.5f + 3.0f * i);
            p.y = groundAt(p.x, p.z);
            Mob& m = entities_.spawnMob(kinds[i], p);
            m.yaw = std::atan2(-f.z, -f.x) + 0.5f;
            m.stateTimer = 1000.0f;  // stehen lassen
        }
        entities_.mobs()[1].hurtTimer = 100.0f;  // eine Kuh rot aufleuchten lassen
        glm::vec3 ip = player_.pos + f * 2.5f;
        ip.y = groundAt(ip.x, ip.z);
        entities_.spawnItem({GRASS, 1}, ip + glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f), 99.0f);
        entities_.spawnItem({ITEM_MEAT, 1}, ip - glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f), 99.0f);
    } else if (opt_.demo == "water") {
        // Quelle auf einer Anhoehe, das Wasser laeuft ueber das Gelaende
        glm::vec3 p = player_.pos + f * 6.0f;
        int x = static_cast<int>(std::floor(p.x)), z = static_cast<int>(std::floor(p.z));
        int y = static_cast<int>(groundAt(p.x, p.z));
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz) world_->setBlock(x + dx, y, z + dz, STONE);
        world_->setBlock(x, y + 1, z, WATER);
        for (int i = 0; i < 40; ++i) world_->tick(0.25f);
    } else if (opt_.demo == "inv") {
        inventory_.clear();
        const ItemId items[] = {GRASS, DIRT, STONE, COBBLE, SAND, PLANKS, LOG, LEAVES, GLASS, SNOW, ITEM_MEAT, WATER};
        int i = 0;
        for (ItemId id : items) inventory_.add(id, 3 + i++ * 7);
        inventory_.slots[2].count = 64;
        if (creative_) {
            for (int k = 0; k < Inventory::HOTBAR; ++k) inventory_.slots[k] = {DEFAULT_HOTBAR[k], 64};
        }
        cursor_ = {STONE, 12};
    }
}


// ---------------------------------------------------------------------------------------------------------------
// Selbsttest der Spielmechanik (--selftest), laeuft im echten Fenster ohne Eingaben
// ---------------------------------------------------------------------------------------------------------------

int Game::selfTest() {
    int failed = 0, checks = 0;
    auto check = [&](bool ok, const char* what) {
        ++checks;
        if (!ok) {
            ++failed;
            std::fprintf(stderr, "FEHLER: %s\n", what);
        }
    };
#define SCHECK(c) check((c), #c)

    // Testbuehne: Stein-Plattform bei y = 100
    const int gy = 100;
    world_->loadAround(glm::vec3(0.0f, 100.0f, 0.0f), 2);
    for (int x = -12; x <= 12; ++x)
        for (int z = -12; z <= 12; ++z) {
            for (int y = gy; y < gy + 40; ++y) world_->setBlock(x, y, z, AIR);
            world_->setBlock(x, gy - 1, z, STONE);
        }
    EntityEnv env;
    env.allowSpawns = false;
    auto settle = [&](float seconds) {
        for (float t = 0; t < seconds; t += 0.05f) {
            MoveInput none;
            player_.update(0.05f, none, *world_);
            updateSurvival(0.05f);
            entities_.update(0.05f, *world_, player_, inventory_, env);
        }
    };

    creative_ = false;
    dead_ = false;
    health_ = 20;
    inventory_.clear();
    entities_.clear();
    player_.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    player_.vel = glm::vec3(0.0f);
    player_.yaw = 0.0f;
    player_.pitch = -90.0f;  // nach unten schauen
    spawnPoint_ = player_.pos;
    settle(0.5f);

    // 1. Abbauen liefert den richtigen Gegenstand
    world_->setBlock(3, gy - 1, 0, GRASS);
    breakBlock(glm::ivec3(3, gy - 1, 0));
    SCHECK(world_->getBlock(3, gy - 1, 0) == AIR);
    SCHECK(entities_.items().size() == 1 && entities_.items()[0].stack.id == DIRT);
    world_->setBlock(3, gy - 1, 0, STONE);
    breakBlock(glm::ivec3(3, gy - 1, 0));
    SCHECK(entities_.items().size() == 2);
    world_->setBlock(4, gy - 1, 0, BEDROCK);
    breakBlock(glm::ivec3(4, gy - 1, 0));
    SCHECK(world_->getBlock(4, gy - 1, 0) == BEDROCK);  // unzerstoerbar
    player_.pos = glm::vec3(3.5f, static_cast<float>(gy), 0.5f);
    settle(1.0f);
    SCHECK(inventory_.count(DIRT) == 1 && inventory_.count(COBBLE) == 1);
    SCHECK(entities_.items().empty());
    world_->setBlock(4, gy - 1, 0, STONE);

    // 2. Setzen verbraucht Gegenstaende
    player_.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    player_.pitch = -90.0f;
    settle(0.3f);
    inventory_.clear();
    inventory_.slots[0] = {PLANKS, 2};
    slot_ = 0;
    target_ = world_->raycast(player_.eyePos(), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);
    SCHECK(target_.hit && target_.normal == glm::ivec3(0, 1, 0));
    placeBlockOrUse();  // setzt auf das Feld, auf dem der Spieler steht: verboten
    SCHECK(world_->getBlock(0, gy, 0) == AIR && inventory_.slots[0].count == 2);
    player_.pos = glm::vec3(5.5f, static_cast<float>(gy), 5.5f);
    target_ = world_->raycast(glm::vec3(0.5f, gy + 1.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);
    placeBlockOrUse();
    SCHECK(world_->getBlock(0, gy, 0) == PLANKS && inventory_.slots[0].count == 1);
    placeTimer_ = 0.0f;
    placeBlockOrUse();
    SCHECK(world_->getBlock(0, gy, 0) == PLANKS);  // Ziel ist jetzt belegt (Normale zeigt auf Plankenblock): oben drauf
    target_ = world_->raycast(glm::vec3(0.5f, gy + 3.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);
    placeTimer_ = 0.0f;
    placeBlockOrUse();
    SCHECK(world_->getBlock(0, gy + 1, 0) == PLANKS && inventory_.slots[0].empty());
    placeTimer_ = 0.0f;
    placeBlockOrUse();  // leere Hand: nichts
    SCHECK(world_->getBlock(0, gy + 2, 0) == AIR);

    // 3. Essen heilt
    inventory_.slots[1] = {ITEM_MEAT, 2};
    slot_ = 1;
    health_ = 10;
    eatTimer_ = 0.0f;
    placeBlockOrUse();
    SCHECK(health_ == 14 && inventory_.slots[1].count == 1);
    health_ = 20;
    eatTimer_ = 0.0f;
    placeBlockOrUse();
    SCHECK(inventory_.slots[1].count == 1);  // bei voller Gesundheit nicht verbrauchen

    // 4. Schaden und Schutzzeit
    health_ = 20;
    invulnerable_ = 0.0f;
    damagePlayer(4, glm::vec3(0.0f));
    SCHECK(health_ == 16);
    damagePlayer(3, glm::vec3(2.0f, 3.0f, 0.0f));
    SCHECK(health_ == 13 && invulnerable_ > 0.0f);
    damagePlayer(3, glm::vec3(2.0f, 3.0f, 0.0f));
    SCHECK(health_ == 13);  // geschuetzt
    invulnerable_ = 0.0f;

    // 5. Fallschaden: Sturz aus 6 Bloecken Hoehe = 3 Schaden, 3 Bloecke = keiner
    player_.pos = glm::vec3(-5.5f, gy + 3.0f, -5.5f);
    player_.vel = glm::vec3(0.0f);
    player_.landedFall = 0.0f;
    health_ = 20;
    settle(2.0f);
    SCHECK(health_ == 20);
    player_.pos = glm::vec3(-5.5f, gy + 6.0f, -5.5f);
    player_.vel = glm::vec3(0.0f);
    settle(2.0f);
    SCHECK(health_ == 17);

    // 6. Tod und Wiedergeburt
    damagePlayer(100, glm::vec3(0.0f));
    SCHECK(dead_ && health_ == 0);
    player_.pos = glm::vec3(8.0f, static_cast<float>(gy), 8.0f);
    respawn();
    SCHECK(!dead_ && health_ == 20 && std::fabs(player_.pos.x - 0.5f) < 0.01f);

    // 7. Regeneration nach einer Pause
    health_ = 10;
    sinceDamage_ = 6.0f;
    regenTimer_ = 0.0f;
    player_.pos = glm::vec3(0.5f, static_cast<float>(gy + 2), 0.5f);  // auf dem Plankenturm
    settle(3.2f);
    SCHECK(health_ >= 11 && health_ <= 13);

    // 8. Ertrinken: Becken mit Wasser
    for (int x = 6; x <= 10; ++x)
        for (int z = 6; z <= 10; ++z)
            for (int y = gy; y <= gy + 4; ++y) world_->setBlock(x, y, z, WATER);
    health_ = 20;
    sinceDamage_ = 0.0f;
    air_ = 15.0f;
    player_.pos = glm::vec3(8.5f, gy + 2.5f, 8.5f);
    player_.vel = glm::vec3(0.0f);
    settle(0.3f);
    SCHECK(player_.inWater);
    // tief tauchen: Kopf unter Wasser halten (Plattform hat 5 Bloecke Wasser)
    for (float t = 0; t < 5.0f; t += 0.05f) {
        player_.pos.y = std::min(player_.pos.y, gy + 2.0f);
        MoveInput none;
        player_.update(0.05f, none, *world_);
        updateSurvival(0.05f);
    }
    SCHECK(air_ < 15.0f && air_ > 5.0f);
    for (float t = 0; t < 14.0f; t += 0.05f) {
        player_.pos.y = std::min(player_.pos.y, gy + 2.0f);
        MoveInput none;
        player_.update(0.05f, none, *world_);
        updateSurvival(0.05f);
    }
    SCHECK(air_ == 0.0f && health_ < 20);
    player_.pos = glm::vec3(0.5f, static_cast<float>(gy), 0.5f);
    settle(2.0f);
    SCHECK(air_ > 5.0f);  // an der Luft fuellt sich der Vorrat wieder

    // 12. Lava: Schaden und Brennen, Wasser loescht
    health_ = 20;
    sinceDamage_ = 99.0f;
    invulnerable_ = 0.0f;
    dead_ = false;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            for (int y = gy; y <= gy + 2; ++y) world_->setBlock(-9 + dx, y, -9 + dz, LAVA);
    player_.pos = glm::vec3(-8.5f, gy + 0.2f, -8.5f);
    player_.vel = glm::vec3(0.0f);
    settle(1.3f);
    SCHECK(player_.inLava);
    SCHECK(health_ <= 16 && burnTimer_ > 0.0f);
    player_.pos = glm::vec3(8.5f, gy + 2.0f, 8.5f);  // ins Wasserbecken
    player_.vel = glm::vec3(0.0f);
    settle(0.5f);
    SCHECK(player_.inWater && burnTimer_ == 0.0f);
    creative_ = true;
    health_ = 20;
    player_.pos = glm::vec3(-8.5f, gy + 0.2f, -8.5f);
    settle(1.0f);
    SCHECK(health_ == 20 && burnTimer_ == 0.0f);  // Kreativ: unverwundbar
    creative_ = false;

    // 9. Inventar per Mausklick
    dead_ = false;
    health_ = 20;
    inventory_.clear();
    inventory_.slots[9] = {STONE, 10};
    openInventory();
    SCHECK(inventoryOpen_);
    {
        InvLayout L = InvLayout::make(fbW_, fbH_, false);
        auto clickAt = [&](glm::vec2 fbPos, int button, int mods) {
            const double sx = static_cast<double>(winW_cur_) / fbW_, sy = static_cast<double>(winH_cur_) / fbH_;
            curX_ = fbPos.x * sx;
            curY_ = fbPos.y * sy;
            inventoryClick(button, mods);
        };
        glm::vec2 c9 = L.slotPos(9) + glm::vec2(L.pitch() * 0.5f);
        glm::vec2 c0 = L.slotPos(0) + glm::vec2(L.pitch() * 0.5f);
        clickAt(c9, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.id == STONE && cursor_.count == 10 && inventory_.slots[9].empty());
        clickAt(c0, GLFW_MOUSE_BUTTON_RIGHT, 0);  // eines in die Hotbar
        SCHECK(inventory_.slots[0].count == 1 && cursor_.count == 9);
        closeInventory();  // Rest geht zurueck in den Rucksack
        SCHECK(!inventoryOpen_ && cursor_.empty() && inventory_.count(STONE) == 10);
        // Umschalt-Klick verschiebt den Stapel
        openInventory();
        clickAt(c0, GLFW_MOUSE_BUTTON_LEFT, GLFW_MOD_SHIFT);
        SCHECK(inventory_.slots[0].empty() && inventory_.count(STONE) == 10);
        // Klick ausserhalb wirft den gehaltenen Stapel weg
        glm::vec2 c10 = L.slotPos(10) + glm::vec2(L.pitch() * 0.5f);
        (void)c10;
        clickAt(L.slotPos(Inventory::HOTBAR) + glm::vec2(L.pitch() * 0.5f), GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(!cursor_.empty());
        clickAt(glm::vec2(3.0f, 3.0f), GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.empty() && !entities_.items().empty());
        closeInventory();
    }

    // 10. Kreativ: unendlich, keine Schaeden
    entities_.clear();
    setCreative(true);
    SCHECK(creative_ && !inventory_.isEmpty());
    slot_ = 0;
    int before = inventory_.slots[0].count;
    player_.pos = glm::vec3(5.5f, static_cast<float>(gy), -5.5f);
    target_ = world_->raycast(glm::vec3(5.5f, gy + 1.5f, -4.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);
    placeBlockOrUse();
    SCHECK(inventory_.slots[0].count == before);
    health_ = 20;
    damagePlayer(10, glm::vec3(0.0f));
    SCHECK(health_ == 20);
    setCreative(false);
    SCHECK(!creative_ && !player_.flying);

    // 11. Speichern: Inventar, Gesundheit, Modus
    const std::string path = "olicraft_selftest/world.dat";
    std::filesystem::remove_all("olicraft_selftest");
    opt_.savePath = path;
    opt_.noSave = false;
    inventory_.clear();
    inventory_.slots[4] = {LOG, 12};
    inventory_.slots[30] = {ITEM_MEAT, 3};
    health_ = 9;
    slot_ = 4;
    SCHECK(saveWorld());
    SaveFile sf;
    std::string err;
    SCHECK(readSaveFile(path, sf, &err));
    SCHECK(sf.player.hasInventory && sf.player.health == 9 && !sf.player.creative && sf.player.slot == 4);
    SCHECK(sf.player.inventory.slots[4] == ItemStack({LOG, 12}) && sf.player.inventory.slots[30] == ItemStack({ITEM_MEAT, 3}));
    SCHECK(!sf.chunks.empty());
    std::filesystem::remove_all("olicraft_selftest");

#undef SCHECK
    std::printf("Selbsttest: %d Pruefungen, %d fehlgeschlagen\n", checks, failed);
    return failed;
}

// ---------------------------------------------------------------------------------------------------------------

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
    f.underwater = world_->inWaterAt(f.camPos);
    f.underLava = world_->inLavaAt(f.camPos);
    f.hasTarget = target_.hit && !inventoryOpen_ && !dead_;
    f.target = target_.block;

    entityVerts_.clear();
    entityHurtVerts_.clear();
    entities_.buildMesh(*world_, f.camPos, gameTime_, entityVerts_, entityHurtVerts_);
    f.entityVerts = &entityVerts_;
    f.entityHurtVerts = &entityHurtVerts_;

    HudState& h = f.hud;
    h.show = showHud_;
    h.creative = creative_;
    h.health = health_;
    h.showAir = player_.headInWater || air_ < MAX_AIR - 0.01f;
    h.air = air_ / MAX_AIR;
    h.inventory = &inventory_;
    h.selectedSlot = slot_;
    h.inventoryOpen = inventoryOpen_;
    h.cursor = cursor_;
    h.mouse = mousePx();
    h.breakProgress = breakProgress_;
    h.damageFlash = damageFlash_;
    h.dead = dead_;
    h.burning = burnTimer_ > 0.0f;
    h.message = statusMsg_;
    h.messageAlpha = statusTimer_;
    h.itemName = shownItem_ == ITEM_NONE ? "" : itemInfo(shownItem_).name;
    h.itemNameAlpha = itemNameTimer_;
    renderer_.render(*world_, f);
}

bool Game::saveWorld() {
    if (opt_.noSave) return true;
    SaveFile sf;
    sf.seed = world_->seed();
    sf.player.pos = dead_ ? spawnPoint_ : player_.pos;
    sf.player.yaw = player_.yaw;
    sf.player.pitch = player_.pitch;
    sf.player.flying = player_.flying;
    sf.player.slot = slot_;
    sf.player.timeOfDay = timeOfDay_;
    sf.player.health = dead_ ? 20 : std::max(1, health_);
    sf.player.creative = creative_;
    sf.player.hasInventory = true;
    sf.player.inventory = inventory_;
    // Ein gehaltener Stapel geht nicht verloren
    if (!cursor_.empty()) sf.player.inventory.add(cursor_.id, cursor_.count);
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
        showMessage(std::string("Screenshot: ") + name);
    }
}

void Game::updateTitle(double now) {
    ++fpsFrames_;
    if (now - fpsTimer_ < 0.5) return;
    fps_ = fpsFrames_ / (now - fpsTimer_);
    fpsFrames_ = 0;
    fpsTimer_ = now;

    char buf[256];
    int hour = static_cast<int>(std::fmod(timeOfDay_ * 24.0f + 18.0f, 24.0f));  // 0.25 -> 12 Uhr
    int minute = static_cast<int>(std::fmod(timeOfDay_ * 24.0f * 60.0f, 60.0f));
    std::snprintf(buf, sizeof(buf), "OliCraft | %.0f FPS | XYZ %.1f %.1f %.1f | %02d:%02d | %s%s%s", fps_, player_.pos.x,
                  player_.pos.y, player_.pos.z, hour, minute, creative_ ? "Kreativ" : "Survival",
                  player_.flying ? " | Flug" : "", paused_ ? " | PAUSE (Klick: weiter, Q: beenden)" : "");
    glfwSetWindowTitle(window_, buf);
}

int Game::run() {
    if (!init()) return 1;
    if (opt_.selfTest) return selfTest() == 0 ? 0 : 1;

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
                std::printf("Screenshot gespeichert: %s (%zu Chunks, %d sichtbar, %llu Quads, %zu Mobs, %zu Items)\n",
                            opt_.screenshotPath.c_str(), world_->loadedChunkCount(), renderer_.drawnChunks(),
                            static_cast<unsigned long long>(renderer_.drawnQuads()), entities_.mobs().size(),
                            entities_.items().size());
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
                std::printf("Bench: %d Frames, Mittel %.2f ms, Max %.2f ms, %zu Chunks geladen, %d sichtbar, Pos %.0f %.0f %.0f, %zu Mobs\n",
                            frameCounter_, benchSum_ / frameCounter_, benchMax_, world_->loadedChunkCount(),
                            renderer_.drawnChunks(), player_.pos.x, player_.pos.y, player_.pos.z, entities_.mobs().size());
                break;
            }
        }
    }

    if (!shotMode) saveWorld();
    return exitCode;
}

}  // namespace oc
