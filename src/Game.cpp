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
#include <fstream>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

#include "Crafting.h"
#include "Models.h"
#include "Noise.h"

namespace oc {

namespace {

constexpr float DAY_SECONDS = 600.0f;
constexpr float REACH = 6.0f;
constexpr float MELEE_REACH = 3.6f;
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
    if (!opt_.noSave) loadSettings(opt_.settingsPath, settings_);
    if (opt_.distanceCli) settings_.renderDistance = opt_.renderDistance;
    if (opt_.vsyncCli) settings_.vsync = opt_.vsync;
    settings_.clamp();
    opt_.renderDistance = settings_.renderDistance;
    opt_.vsync = settings_.vsync;

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

    textures_.setDirectory(opt_.texturesDir);
    if (!opt_.noSave) {
        std::vector<std::string> msgs;
        const int n = textures_.loadAllFromDirectory(&msgs);
        if (n > 0) std::printf("%d eigene Texturen aus %s geladen\n", n, opt_.texturesDir.c_str());
        for (const std::string& m : msgs) std::fprintf(stderr, "Textur ignoriert: %s\n", m.c_str());
    }
    textures_.selectPlayerSkin(settings_.playerSkin);
    if (!renderer_.init(textures_)) return false;

    // Ton: Tests und Screenshots laufen ohne echtes Geraet; OLICRAFT_AUDIO=null erzwingt das ebenfalls
    if (opt_.sound) {
        const char* envAudio = std::getenv("OLICRAFT_AUDIO");
        const bool nullDevice = opt_.selfTest || !opt_.screenshotPath.empty() || opt_.benchFrames > 0 ||
                                (envAudio && std::string(envAudio) == "null");
        if (audio_.init(nullDevice)) {
            const int n = audio_.loadSounds(opt_.soundsDir);
            std::printf("Ton: %d Klangereignisse, %d Musikstuecke aus %s\n", n, audio_.musicTrackCount(),
                        opt_.soundsDir.c_str());
            applyVolumes();
        } else {
            std::fprintf(stderr, "Ton: kein Audiogeraet, das Spiel bleibt still.\n");
        }
    }

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
        world_->importFurnaces(sf.furnaces);
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
    camMode_ = opt_.viewMode == 1 ? CameraMode::ThirdBack : (opt_.viewMode == 2 ? CameraMode::ThirdFront : CameraMode::First);
    if (opt_.fly && creative_) player_.flying = true;
    if (!creative_) player_.flying = false;
    if (opt_.health > 0) health_ = std::min(20, opt_.health);

    const bool shotMode = !opt_.screenshotPath.empty();
    world_->loadAround(player_.pos, shotMode ? opt_.renderDistance : 3);
    if (!opt_.demo.empty()) applyDemo();

    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    if (opt_.demo == "inv") openInventory();
    if (opt_.demo == "furnace") {
        inventory_.clear();
        inventory_.add(IRON_ORE, 12);
        inventory_.add(ITEM_COAL, 7);
        inventory_.add(COAL_ORE, 3);
        inventory_.add(DIAMOND_ORE, 2);
        inventory_.add(ITEM_DIAMOND, 4);
        inventory_.add(ITEM_IRON_INGOT, 9);
        inventory_.add(ITEM_COOKED_MEAT, 3);
        inventory_.add(FURNACE, 1);
        const glm::ivec3 fp(static_cast<int>(std::floor(player_.pos.x)) + 2, static_cast<int>(std::floor(player_.pos.y)),
                            static_cast<int>(std::floor(player_.pos.z)));
        world_->setBlock(fp.x, fp.y, fp.z, FURNACE);
        openFurnace(fp);
        Furnace& fu = world_->furnaceAt(fp.x, fp.y, fp.z);
        fu.input = {IRON_ORE, 5};
        fu.fuel = {ITEM_COAL, 3};
        fu.burnTime = 50.0f;
        fu.burnTotal = 80.0f;
        fu.cookTime = 6.0f;
        fu.output = {ITEM_IRON_INGOT, 2};
    }
    if (opt_.demo == "craft") {
        inventory_.clear();
        const ItemId tools[] = {ITEM_WOODEN_PICKAXE, ITEM_WOODEN_AXE, ITEM_WOODEN_SHOVEL, ITEM_WOODEN_SWORD,
                                ITEM_STONE_PICKAXE, ITEM_STONE_AXE, ITEM_STONE_SHOVEL, ITEM_STONE_SWORD, ITEM_STICK};
        for (int i = 0; i < 9; ++i) inventory_.slots[i] = {tools[i], i == 8 ? 12 : 1, i == 4 ? 40 : (i == 0 ? 15 : 0)};
        inventory_.add(PLANKS, 20);
        inventory_.add(COBBLE, 9);
        inventory_.add(CRAFTING_TABLE, 1);
        inventory_.add(LOG, 5);
        openInventory(true);
        craft_[0] = craft_[1] = craft_[2] = {COBBLE, 1};
        craft_[4] = craft_[7] = {ITEM_STICK, 1};
    }
    if (opt_.demo.rfind("editor", 0) == 0) {  // editor, editorblock, editorpig ...
        setPaused(true);
        openEditor();
        const std::string want = opt_.demo == "editorblock" ? "Gras" : (opt_.demo == "editorpig" ? "Schwein" : (opt_.demo == "editoritem" ? "Eisenspitzhacke" : "Spieler"));
        for (size_t i = 0; i < editor_->entries().size(); ++i)
            if (editor_->entries()[i].name == want) editor_->select(static_cast<int>(i));
        curX_ = winW_cur_ * 0.5;
        curY_ = winH_cur_ * 0.5;
    }
    if (opt_.demo == "menu" || opt_.demo == "settings") {
        setPaused(true);
        if (opt_.demo == "settings") screen_ = MenuScreen::Settings;
        curX_ = winW_cur_ * 0.5;
        curY_ = winH_cur_ * 0.5 - 25.0 * guiScale(fbH_);
    }
    return true;
}

void Game::shutdown() {
    if (!window_) {
        glfwTerminate();
        return;
    }
    audio_.shutdown();
    world_.reset();  // GL-Objekte der Chunks freigeben, solange der Kontext existiert
    renderer_.shutdown();
    glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
}

void Game::applyVolumes() { audio_.setVolumes(settings_.masterVolume, settings_.effectVolume, settings_.musicVolume); }

void Game::playSound(SoundId id, const glm::vec3& pos, float volume, float pitch) {
    audio_.play(id, pos, volume, pitch, true);
}

void Game::uiSound(SoundId id, float volume) { audio_.playUi(id, volume); }

void Game::updateAudio(float dt) {
    if (!audio_.active()) return;
    applyVolumes();
    audio_.setListener(player_.eyePos(), player_.front(), glm::vec3(0.0f, 1.0f, 0.0f));
    for (const SoundEvent& e : entities_.soundEvents())
        playSound(e.id, glm::vec3(e.x, e.y, e.z), e.volume, e.pitch);
    entities_.soundEvents().clear();
    audio_.update(dt);
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
    screen_ = p ? MenuScreen::Pause : MenuScreen::None;
    dragWidget_ = W_NONE;
    firstMouse_ = true;
    if (!p) breakTimer_ = placeTimer_ = attackTimer_ = 0.3f;  // der Klick zum Fortsetzen soll nichts abbauen
    const bool freeCursor = p || inventoryOpen_;
    glfwSetInputMode(window_, GLFW_CURSOR, freeCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    if (!freeCursor && glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    if (p) {
        glfwGetWindowSize(window_, &winW_cur_, &winH_cur_);
        curX_ = winW_cur_ * 0.5;
        curY_ = winH_cur_ * 0.5;
        glfwSetCursorPos(window_, curX_, curY_);
    }
}

MenuState Game::menuState() const {
    MenuState s;
    s.renderDistance = settings_.renderDistance;
    s.mouseSensitivity = settings_.mouseSensitivity;
    s.fov = settings_.fov;
    s.vsync = settings_.vsync;
    s.skinSlot = settings_.playerSkin;
    s.masterVolume = settings_.masterVolume;
    s.effectVolume = settings_.effectVolume;
    s.musicVolume = settings_.musicVolume;
    s.fullscreen = window_ && glfwGetWindowMonitor(window_) != nullptr;
    s.creative = creative_;
    return s;
}

void Game::persistSettings() {
    if (!opt_.noSave) saveSettings(opt_.settingsPath, settings_);
}

void Game::updateSlider(int widgetId, float mouseX) {
    if (screen_ == MenuScreen::Editor && editor_) {
        const EditorLayout L = buildEditorLayout(fbW_, fbH_, *editor_, editorUi_);
        for (const Widget& w : L.widgets)
            if (w.id == widgetId) {
                const int idx = widgetId - W_ED_R;
                if (idx >= 0 && idx < 4) editor_->color[idx] = static_cast<uint8_t>(std::lround(sliderFraction(w, mouseX) * 255.0f));
            }
        return;
    }
    const MenuLayout menu = buildMenu(screen_, menuState(), fbW_, fbH_);
    for (const Widget& w : menu.widgets) {
        if (w.id != widgetId) continue;
        const float f = sliderFraction(w, mouseX);
        if (widgetId == W_DISTANCE) {
            settings_.renderDistance = distanceFromFraction(f);
            opt_.renderDistance = settings_.renderDistance;
        } else if (widgetId == W_SENSITIVITY) {
            settings_.mouseSensitivity = sensitivityFromFraction(f);
        } else if (widgetId == W_FOV) {
            settings_.fov = fovFromFraction(f);
        } else if (widgetId == W_VOL_MASTER) {
            settings_.masterVolume = volumeFromFraction(f);
        } else if (widgetId == W_VOL_EFFECTS) {
            settings_.effectVolume = volumeFromFraction(f);
        } else if (widgetId == W_VOL_MUSIC) {
            settings_.musicVolume = volumeFromFraction(f);
        }
        settings_.clamp();
    }
}

void Game::activateWidget(int index, const MenuLayout& menu, glm::vec2 mouse) {
    if (index < 0 || index >= static_cast<int>(menu.widgets.size())) return;
    const int id = menu.widgets[index].id;
    switch (id) {
        case W_RESUME: setPaused(false); break;
        case W_SETTINGS: screen_ = MenuScreen::Settings; break;
        case W_SAVE: wantSave_ = true; break;
        case W_QUIT: glfwSetWindowShouldClose(window_, GLFW_TRUE); break;
        case W_BACK:
            screen_ = MenuScreen::Pause;
            persistSettings();
            break;
        case W_DISTANCE:
        case W_SENSITIVITY:
        case W_FOV:
        case W_VOL_MASTER:
        case W_VOL_EFFECTS:
        case W_VOL_MUSIC:
            dragWidget_ = id;
            updateSlider(id, mouse.x);
            break;
        case W_SKIN:
            settings_.playerSkin = (settings_.playerSkin + 1) % PLAYER_SKIN_SLOTS;
            textures_.selectPlayerSkin(settings_.playerSkin);
            uiSound(SoundId::Click, 0.7f);
            persistSettings();
            break;
        case W_VSYNC:
            settings_.vsync = !settings_.vsync;
            glfwSwapInterval(settings_.vsync ? 1 : 0);
            persistSettings();
            break;
        case W_FULLSCREEN: toggleFullscreen(); break;
        case W_MODE: setCreative(!creative_); break;
        case W_EDITOR: openEditor(); break;
        default: break;
    }
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

void Game::openInventory(bool table) {
    if (dead_) return;
    inventoryOpen_ = true;
    furnaceWindow_ = false;
    craftSize_ = table ? 3 : (creative_ ? 0 : 2);
    paletteWindow_ = creative_ && !table;
    for (ItemStack& c : craft_) c.clear();
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwGetWindowSize(window_, &winW_cur_, &winH_cur_);
    curX_ = winW_cur_ * 0.5;
    curY_ = winH_cur_ * 0.5;
    glfwSetCursorPos(window_, curX_, curY_);
}

void Game::openFurnace(const glm::ivec3& pos) {
    openInventory(true);
    furnaceWindow_ = true;
    furnacePos_ = pos;
    world_->furnaceAt(pos.x, pos.y, pos.z);
}

void Game::closeInventory() {
    if (!inventoryOpen_) return;
    // Gehaltener Stapel und Handwerksraster gehen zurueck in den Rucksack, was nicht passt, wird geworfen
    if (!cursor_.empty()) {
        int left = creative_ ? 0 : inventory_.add(cursor_.id, cursor_.count, cursor_.damage);
        if (left > 0) {
            cursor_.count = left;
            dropFromCursor(true);
        }
        cursor_.clear();
    }
    for (ItemStack& c : craft_) {
        if (c.empty()) continue;
        int left = inventory_.add(c.id, c.count, c.damage);
        if (left > 0) {
            cursor_ = c;
            cursor_.count = left;
            dropFromCursor(true);
        }
        c.clear();
    }
    inventoryOpen_ = false;
    craftSize_ = 0;
    paletteWindow_ = false;
    furnaceWindow_ = false;
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
    const InvLayout layout = InvLayout::make(fbW_, fbH_, paletteWindow_, craftSize_, furnaceWindow_);
    const glm::vec2 m = mousePx();
    uiSound(SoundId::Click, 0.4f);

    if (furnaceWindow_) {
        Furnace* fu = world_->findFurnace(furnacePos_.x, furnacePos_.y, furnacePos_.z);
        const int fslot = layout.hitFurnace(m);
        if (fu && fslot >= 0) {
            ItemStack* st = fslot == 0 ? &fu->input : (fslot == 1 ? &fu->fuel : &fu->output);
            if ((mods & GLFW_MOD_SHIFT) && !right) {
                if (!st->empty()) {  // in den Rucksack verschieben
                    const int left = inventory_.add(st->id, st->count, st->damage);
                    if (left <= 0) st->clear();
                    else st->count = left;
                }
            } else if (fslot == 2) {  // Ausgabe: nur entnehmen
                if (cursor_.empty()) {
                    cursor_ = *st;
                    st->clear();
                } else if (cursor_.sameKind(*st) && cursor_.count + st->count <= itemInfo(cursor_.id).maxStack) {
                    cursor_.count += st->count;
                    st->clear();
                }
            } else {
                Inventory::click(*st, cursor_, right);
            }
            return;
        }
    }

    const int pal = layout.hitPalette(m);
    if (pal >= 0) {
        const ItemId id = creativeItem(pal);
        if (!cursor_.empty() && cursor_.id == id) cursor_.clear();  // zurueckgeben
        else cursor_ = {id, right ? 1 : itemInfo(id).maxStack};
        return;
    }
    const int cell = layout.hitCraft(m);
    if (cell >= 0) {
        Inventory::click(craft_[cell], cursor_, right);
        return;
    }
    if (layout.hitResult(m)) {
        ItemStack res = matchRecipe(craft_, craftSize_);
        if (res.empty()) return;
        uiSound(SoundId::Craft, 0.8f);
        if ((mods & GLFW_MOD_SHIFT) && !right) {
            // so oft herstellen, wie Zutaten und Platz reichen
            for (int i = 0; i < 64; ++i) {
                res = matchRecipe(craft_, craftSize_);
                if (res.empty()) break;
                Inventory trial = inventory_;
                if (trial.add(res.id, res.count, res.damage) > 0) break;
                inventory_ = trial;
                consumeIngredients(craft_, craftSize_);
            }
        } else if (cursor_.empty()) {
            cursor_ = res;
            consumeIngredients(craft_, craftSize_);
        } else if (cursor_.sameKind(res) && cursor_.count + res.count <= itemInfo(res.id).maxStack) {
            cursor_.count += res.count;
            consumeIngredients(craft_, craftSize_);
        }
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

void Game::useHeldTool(int amount) {
    if (creative_) return;
    ItemStack& s = inventory_.slots[slot_];
    if (s.empty()) return;
    const ItemInfo& info = itemInfo(s.id);
    if (info.maxDurability <= 0) return;
    s.damage += amount;
    if (s.damage >= info.maxDurability) {
        s.clear();
        showMessage("Werkzeug zerbrochen");
    }
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
    if (paused_ && screen_ == MenuScreen::Editor && key != GLFW_KEY_ESCAPE && editorKey(key, mods)) return;
    switch (key) {
        case GLFW_KEY_ESCAPE:
            if (inventoryOpen_) closeInventory();
            else if (paused_ && screen_ == MenuScreen::Editor) {
                screen_ = MenuScreen::Settings;
                dragWidget_ = W_NONE;
                painting_ = false;
            } else if (paused_ && screen_ == MenuScreen::Settings) {
                screen_ = MenuScreen::Pause;
                persistSettings();
            } else setPaused(!paused_);
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
    } else if (key == GLFW_KEY_V) {
        camMode_ = nextCameraMode(camMode_);
        showMessage(camMode_ == CameraMode::First ? "Ansicht: Ego" : (camMode_ == CameraMode::ThirdBack ? "Ansicht: von hinten" : "Ansicht: von vorn"));
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
    if (paused_ && screen_ == MenuScreen::Editor) {
        editorScroll(dy);
        return;
    }
    if (paused_ || inventoryOpen_ || dead_) return;
    int d = dy > 0 ? -1 : (dy < 0 ? 1 : 0);
    slot_ = ((slot_ + d) % Inventory::HOTBAR + Inventory::HOTBAR) % Inventory::HOTBAR;
}

void Game::handleMouseButton(int button, int action, int mods) {
    if (paused_ && screen_ == MenuScreen::Editor) {
        editorMouse(button, action, mods);
        return;
    }
    if (paused_) {
        if (button != GLFW_MOUSE_BUTTON_LEFT) return;
        if (action == GLFW_RELEASE) {
            if (dragWidget_ != W_NONE) {
                dragWidget_ = W_NONE;
                persistSettings();
            }
            return;
        }
        if (action == GLFW_PRESS) {
            const MenuLayout menu = buildMenu(screen_, menuState(), fbW_, fbH_);
            const int hit = hitWidget(menu, mousePx());
            if (hit >= 0) uiSound(SoundId::Click, 0.7f);
            activateWidget(hit, menu, mousePx());
        }
        return;
    }
    if (action != GLFW_PRESS) return;
    if (inventoryOpen_ && (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT))
        inventoryClick(button, mods);
}

void Game::handleCursor(double x, double y) {
    curX_ = x;
    curY_ = y;
    if (paused_) {
        if (dragWidget_ != W_NONE) updateSlider(dragWidget_, mousePx().x);
        if (screen_ == MenuScreen::Editor) editorCursor();
        return;
    }
    if (inventoryOpen_) return;
    if (firstMouse_) {
        lastMx_ = x;
        lastMy_ = y;
        firstMouse_ = false;
        return;
    }
    if (!dead_) player_.look(static_cast<float>(x - lastMx_), static_cast<float>(y - lastMy_), settings_.mouseSensitivity);
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
    else uiSound(SoundId::PlayerHurt);
}

void Game::die() {
    health_ = 0;
    dead_ = true;
    uiSound(SoundId::PlayerDeath);
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

        {
            const float hspeed = std::sqrt(player_.vel.x * player_.vel.x + player_.vel.z * player_.vel.z);
            const float moving = (player_.onGround || player_.flying || player_.inWater) ? std::min(1.0f, hspeed / 3.0f) : 0.0f;
            walkAmount_ += (moving - walkAmount_) * std::min(1.0f, 8.0f * dt);
            walkPhase_ += hspeed * dt * 5.5f;
            deathAnim_ = dead_ ? deathAnim_ + dt : 0.0f;
        }
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

        // Schritte, Wasser, Lava
        {
            const float hs = std::sqrt(player_.vel.x * player_.vel.x + player_.vel.z * player_.vel.z);
            if (player_.onGround && !player_.flying && !player_.inWater && !dead_) {
                stepDist_ += hs * dt;
                if (stepDist_ >= 1.9f) {
                    stepDist_ = 0.0f;
                    const int bx = static_cast<int>(std::floor(player_.pos.x)), bz = static_cast<int>(std::floor(player_.pos.z));
                    const int by = static_cast<int>(std::floor(player_.pos.y - 0.2f));
                    playSound(stepSound(blockMaterial(world_->getBlock(bx, by, bz))), player_.pos, 0.55f);
                }
            } else if (player_.inWater && !dead_ && hs > 0.5f) {
                stepDist_ += hs * dt;
                if (stepDist_ >= 2.2f) {
                    stepDist_ = 0.0f;
                    playSound(SoundId::WaterSwim, player_.pos, 0.6f);
                }
            } else if (!player_.inWater) {
                stepDist_ = std::min(stepDist_, 1.0f);
            }
            if (player_.inWater && !wasInWater_ && !player_.inLava) playSound(SoundId::WaterSplash, player_.pos);
            if (player_.inLava && !wasInLava_) playSound(SoundId::LavaPop, player_.pos);
            wasInWater_ = player_.inWater;
            wasInLava_ = player_.inLava;
            // Ofen angezuendet
            if (furnaceWindow_) {
                const Furnace* fu = world_->findFurnace(furnacePos_.x, furnacePos_.y, furnacePos_.z);
                const bool lit = fu && fu->lit();
                if (lit && !furnaceWasLit_) uiSound(SoundId::Fuse);
                furnaceWasLit_ = lit;
            } else {
                furnaceWasLit_ = false;
            }
        }

        autosaveTimer_ += dt;
        if (autosaveTimer_ > 90.0f) {
            autosaveTimer_ = 0.0f;
            wantSave_ = true;
        }
        statusTimer_ = std::max(0.0f, statusTimer_ - dt);
        itemNameTimer_ = std::max(0.0f, itemNameTimer_ - dt);
        world_->tick(dt);
        for (const glm::ivec3& p : world_->takeTorchDrops())
            if (!creative_) entities_.spawnItem({TORCH, 1}, glm::vec3(p.x + 0.5f, p.y + 0.2f, p.z + 0.5f), glm::vec3(0.0f, 2.0f, 0.0f), 0.3f);
    } else {
        target_ = RayHit{};
        if (screen_ == MenuScreen::Editor && editorUi_.rotate) editorUi_.previewYaw += dt * 0.9f;
    }

    // Anzeige des gewaehlten Gegenstands
    {
        const ItemId cur = inventory_.slots[slot_].empty() ? ITEM_NONE : inventory_.slots[slot_].id;
        if (cur != shownItem_) {
            shownItem_ = cur;
            itemNameTimer_ = cur == ITEM_NONE ? 0.0f : 2.0f;
        }
    }

    updateAudio(dt);
    world_->update(player_.pos, opt_.renderDistance, 5.0);

    if (wantSave_) {
        wantSave_ = false;
        if (saveWorld()) showMessage("Gespeichert");
    }
}

void Game::breakBlock(const glm::ivec3& p) {
    const BlockId b = world_->getBlock(p.x, p.y, p.z);
    if (!blockInfo(b).breakable) return;
    const bool harvest = creative_ ? false : canHarvest(b, inventory_.slots[slot_]);
    if (b == FURNACE || b == FURNACE_LIT) {  // Inhalt des Ofens faellt heraus
        Furnace content;
        if (world_->removeFurnace(p.x, p.y, p.z, &content) && !creative_) {
            for (const ItemStack* st : {&content.input, &content.fuel, &content.output})
                if (!st->empty())
                    entities_.spawnItem(*st, glm::vec3(p.x + 0.5f, p.y + 0.4f, p.z + 0.5f), glm::vec3(0.0f, 2.5f, 0.0f), 0.4f);
        }
    }
    world_->setBlock(p.x, p.y, p.z, AIR);
    playSound(blockSound(blockMaterial(b)), glm::vec3(p.x + 0.5f, p.y + 0.5f, p.z + 0.5f));
    if (!creative_) {
        ItemId drop = harvest ? blockDropItem(b) : ITEM_NONE;
        // Laub gibt nur manchmal etwas her
        if (b == LEAVES && (hash3(p.x, p.y, p.z, 99u) & 7u) >= 3u) drop = ITEM_NONE;
        if (drop != ITEM_NONE)
            entities_.spawnItem({drop, 1}, glm::vec3(p.x + 0.5f, p.y + 0.25f, p.z + 0.5f),
                                glm::vec3(static_cast<float>((hash3(p.x, p.y, p.z, 5u) & 15u)) / 8.0f - 1.0f, 3.0f,
                                          static_cast<float>((hash3(p.x, p.y, p.z, 6u) & 15u)) / 8.0f - 1.0f),
                                0.3f);
        useHeldTool(1);
    }
}

void Game::placeBlockOrUse() {
    ItemStack& sel = inventory_.slots[slot_];
    // Rechtsklick auf Werkbank oder Ofen oeffnet sie (mit Umschalt wird stattdessen gesetzt)
    if (target_.hit && glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) != GLFW_PRESS &&
        glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) != GLFW_PRESS) {
        const BlockId tb = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
        if (tb == CRAFTING_TABLE) {
            openInventory(true);
            return;
        }
        if (tb == FURNACE || tb == FURNACE_LIT) {
            openFurnace(target_.block);
            return;
        }
    }
    if (sel.empty()) return;

    if (sel.id == ITEM_MEAT || sel.id == ITEM_COOKED_MEAT) {
        if (!creative_ && health_ < 20 && eatTimer_ <= 0.0f) {
            health_ = std::min(20, health_ + (sel.id == ITEM_COOKED_MEAT ? 8 : 4));
            inventory_.take(slot_, 1);
            eatTimer_ = 0.8f;
            uiSound(SoundId::Eat);
        }
        return;
    }
    if (!itemIsBlock(sel.id) || !target_.hit) return;

    glm::ivec3 p = target_.block + target_.normal;
    if (target_.normal.x == 0 && target_.normal.y == 0 && target_.normal.z == 0) return;
    if (p.y < 0 || p.y >= CHUNK_H) return;
    const BlockId cur = world_->getBlock(p.x, p.y, p.z);
    if (cur != AIR && !blockInfo(cur).liquid) return;
    BlockId id = itemBlock(sel.id);
    if (id == TORCH) {
        // Fackeln brauchen einen festen Traegerblock: auf dem Boden oder an einer Wand, nie an der Decke
        const glm::ivec3& n = target_.normal;
        if (n.y < 0 || cur != AIR) return;
        if (!isOpaque(world_->getBlock(target_.block.x, target_.block.y, target_.block.z))) return;
        if (n.y > 0) id = TORCH;
        else if (n.x > 0) id = TORCH_NX;  // Traeger liegt auf der -x-Seite der Fackel
        else if (n.x < 0) id = TORCH_PX;
        else if (n.z > 0) id = TORCH_NZ;
        else id = TORCH_PZ;
    }
    if (blockInfo(id).solid && player_.intersectsBlock(p.x, p.y, p.z)) return;
    if (world_->setBlock(p.x, p.y, p.z, id)) {
        placeTimer_ = 0.2f;
        playSound(blockSound(blockMaterial(id)), glm::vec3(p.x + 0.5f, p.y + 0.5f, p.z + 0.5f), 0.8f, 0.85f);
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
            entities_.hurtMob(*mob, creative_ ? 20 : attackDamage(inventory_.slots[slot_]), player_.pos);
            const ItemInfo& hi = itemInfo(inventory_.slots[slot_].id);
            useHeldTool(hi.tool == TOOL_SWORD ? 1 : 2);
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
                const float need = miningSeconds(b, inventory_.slots[slot_]);
                if (breakTime_ >= need) {
                    breakBlock(target_.block);
                    breaking_ = false;
                    breakTimer_ = 0.15f;
                } else {
                    breakProgress_ = breakTime_ / need;
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
// Textur-Editor
// ---------------------------------------------------------------------------------------------------------------

void Game::openEditor() {
    if (!editor_) editor_ = std::make_unique<TextureEditor>(textures_);
    editor_->select(editor_->current());  // Ansicht neu aufbauen (der Spieler-Skin kann inzwischen gewechselt sein)
    screen_ = MenuScreen::Editor;
    dragWidget_ = W_NONE;
    painting_ = false;
    editorUi_.status = "Texturen: " + textures_.directory() + "/   Export: " + textures_.directory() + "/export/";
}

int Game::hitEditorWidget(const EditorLayout& l, glm::vec2 m) const {
    for (size_t i = 0; i < l.widgets.size(); ++i) {
        const Widget& w = l.widgets[i];
        if (!w.enabled || w.id == W_NONE) continue;
        if (m.x >= w.pos.x && m.x < w.pos.x + w.size.x && m.y >= w.pos.y && m.y < w.pos.y + w.size.y) return static_cast<int>(i);
    }
    return -1;
}

void Game::activateEditorWidget(int id, glm::vec2 mouse) {
    TextureEditor& ed = *editor_;
    std::string msg;
    switch (id) {
        case W_ED_PENCIL: ed.tool = Tool::Pencil; break;
        case W_ED_ERASER: ed.tool = Tool::Eraser; break;
        case W_ED_FILL: ed.tool = Tool::Fill; break;
        case W_ED_PICKER: ed.tool = Tool::Picker; break;
        case W_ED_BRUSH: ed.brush = ed.brush % 3 + 1; break;
        case W_ED_UNDO: if (!ed.undo()) editorUi_.status = "Nichts rueckgaengig zu machen"; break;
        case W_ED_REDO: if (!ed.redo()) editorUi_.status = "Nichts zu wiederholen"; break;
        case W_ED_GRID: editorUi_.grid = !editorUi_.grid; break;
        case W_ED_NET: editorUi_.net = !editorUi_.net; break;
        case W_ED_ROTATE: editorUi_.rotate = !editorUi_.rotate; break;
        case W_ED_ROT_LEFT: editorUi_.previewYaw -= 0.4f; break;
        case W_ED_ROT_RIGHT: editorUi_.previewYaw += 0.4f; break;
        case W_ED_SAVE: ed.save(&msg); editorUi_.status = msg; break;
        case W_ED_EXPORT: ed.exportNet(&msg); editorUi_.status = msg; break;
        case W_ED_IMPORT: ed.importNet(&msg); editorUi_.status = msg; break;
        case W_ED_RESET: ed.resetToDefault(&msg); editorUi_.status = msg; break;
        case W_ED_BACK:
            screen_ = MenuScreen::Settings;
            painting_ = false;
            break;
        case W_ED_R:
        case W_ED_G:
        case W_ED_B:
        case W_ED_A:
            dragWidget_ = id;
            updateSlider(id, mouse.x);
            break;
        case W_ED_LIST_UP: editorUi_.listScroll = std::max(0, editorUi_.listScroll - 3); break;
        case W_ED_LIST_DOWN: editorUi_.listScroll += 3; break;
        default:
            if (id >= W_ED_LIST_BASE) {
                ed.select(id - W_ED_LIST_BASE);
                editorUi_.status = std::string(ed.entry().name) + (ed.entry().kind == EditorEntry::Skin ? ": Netzblatt " : "");
                editorUi_.status = ed.entry().name;
            } else if (id >= W_ED_SWATCH_BASE) {
                const auto& pal = editorPalette();
                const size_t i = static_cast<size_t>(id - W_ED_SWATCH_BASE);
                if (i < pal.size())
                    for (int c = 0; c < 3; ++c) ed.color[c] = static_cast<uint8_t>(std::lround(pal[i][c] * 255.0f));
            }
            break;
    }
}

void Game::editorMouse(int button, int action, int) {
    if (!editor_) return;
    TextureEditor& ed = *editor_;
    const glm::vec2 m = mousePx();
    const EditorLayout L = buildEditorLayout(fbW_, fbH_, ed, editorUi_);
    if (action == GLFW_RELEASE) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (painting_) ed.endStroke();
            painting_ = false;
            dragWidget_ = W_NONE;
        }
        return;
    }
    if (action != GLFW_PRESS) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        const int hit = hitEditorWidget(L, m);
        if (hit >= 0) {
            activateEditorWidget(L.widgets[static_cast<size_t>(hit)].id, m);
            return;
        }
    }
    int px, py;
    if (!canvasPixel(L, ed, m, px, py)) return;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {  // Rechtsklick: Farbe aufnehmen
        const Tool keep = ed.tool;
        ed.tool = Tool::Picker;
        ed.paintAt(px, py);
        ed.tool = keep;
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (ed.tool == Tool::Pencil || ed.tool == Tool::Eraser) {
            ed.beginStroke();
            ed.paintAt(px, py);
            painting_ = true;
            lastPaint_ = {px, py};
        } else {
            ed.paintAt(px, py);
        }
    }
}

void Game::editorCursor() {
    if (!editor_ || !painting_) return;
    TextureEditor& ed = *editor_;
    const EditorLayout L = buildEditorLayout(fbW_, fbH_, ed, editorUi_);
    int px, py;
    if (!canvasPixel(L, ed, mousePx(), px, py)) {
        lastPaint_ = {-1, -1};
        return;
    }
    if (lastPaint_.x < 0) ed.paintAt(px, py);
    else ed.paintLine(lastPaint_.x, lastPaint_.y, px, py);
    lastPaint_ = {px, py};
}

bool Game::editorKey(int key, int mods) {
    if (!editor_) return false;
    TextureEditor& ed = *editor_;
    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0, shift = (mods & GLFW_MOD_SHIFT) != 0;
    std::string msg;
    if (ctrl) {
        if (key == GLFW_KEY_Z) {
            if (shift) ed.redo();
            else ed.undo();
            return true;
        }
        if (key == GLFW_KEY_Y) {
            ed.redo();
            return true;
        }
        if (key == GLFW_KEY_S) {
            ed.save(&msg);
            editorUi_.status = msg;
            return true;
        }
        return false;
    }
    switch (key) {
        case GLFW_KEY_B: ed.tool = Tool::Pencil; return true;
        case GLFW_KEY_E: ed.tool = Tool::Eraser; return true;
        case GLFW_KEY_F: ed.tool = Tool::Fill; return true;
        case GLFW_KEY_P: ed.tool = Tool::Picker; return true;
        case GLFW_KEY_G: editorUi_.grid = !editorUi_.grid; return true;
        case GLFW_KEY_N: editorUi_.net = !editorUi_.net; return true;
        case GLFW_KEY_R: editorUi_.rotate = !editorUi_.rotate; return true;
        case GLFW_KEY_1: ed.brush = 1; return true;
        case GLFW_KEY_2: ed.brush = 2; return true;
        case GLFW_KEY_3: ed.brush = 3; return true;
        default: return false;
    }
}

void Game::editorScroll(double dy) {
    if (!editor_) return;
    const EditorLayout L = buildEditorLayout(fbW_, fbH_, *editor_, editorUi_);
    if (!insideRect(L.list, mousePx())) return;
    editorUi_.listScroll = std::max(0, std::min(maxListScroll(L), editorUi_.listScroll + (dy > 0 ? -2 : 2)));
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
    } else if (opt_.demo == "torch") {
        const glm::vec3 side(-f.z, 0.0f, f.x);
        for (int i = -2; i <= 2; ++i) {
            glm::vec3 p = player_.pos + f * (5.0f + (i % 2 ? 1.5f : 0.0f)) + side * (3.0f * i);
            p.y = groundAt(p.x, p.z);
            world_->setBlock(static_cast<int>(std::floor(p.x)), static_cast<int>(p.y), static_cast<int>(std::floor(p.z)), TORCH);
        }
        // eine Wand mit Wandfackel
        glm::vec3 w = player_.pos + f * 9.0f;
        const int wx = static_cast<int>(std::floor(w.x)), wz = static_cast<int>(std::floor(w.z));
        const int wy = static_cast<int>(groundAt(w.x, w.z));
        for (int y = wy; y < wy + 4; ++y)
            for (int d = -3; d <= 3; ++d) world_->setBlock(wx + (std::fabs(f.x) > std::fabs(f.z) ? 0 : d), y, wz + (std::fabs(f.x) > std::fabs(f.z) ? d : 0), PLANKS);
        const bool alongX = std::fabs(f.x) > std::fabs(f.z);
        const int sx = alongX ? (f.x > 0 ? -1 : 1) : 0, sz = alongX ? 0 : (f.z > 0 ? -1 : 1);
        world_->setBlock(wx + sx, wy + 1, wz + sz, sx < 0 ? TORCH_PX : (sx > 0 ? TORCH_NX : (sz < 0 ? TORCH_PZ : TORCH_NZ)));
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
    SCHECK(world_->getBlock(3, gy - 1, 0) == AIR && entities_.items().size() == 1);  // Stein von Hand: keine Beute
    inventory_.slots[5] = {ITEM_WOODEN_PICKAXE, 1};
    slot_ = 5;
    world_->setBlock(3, gy - 1, 0, STONE);
    breakBlock(glm::ivec3(3, gy - 1, 0));
    SCHECK(entities_.items().size() == 2 && inventory_.slots[5].damage == 1);  // mit Spitzhacke: Bruchstein, Werkzeug nutzt ab
    slot_ = 0;
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

    // 14. Handwerk: Inventar-Raster, Werkbank, Werkzeuge
    {
        dead_ = false;
        creative_ = false;
        inventory_.clear();
        entities_.clear();
        cursor_.clear();
        openInventory();
        SCHECK(inventoryOpen_ && craftSize_ == 2 && !paletteWindow_);
        auto clickAtPos = [&](glm::vec2 fbPos, int button, int mods) {
            curX_ = fbPos.x * static_cast<double>(winW_cur_) / fbW_;
            curY_ = fbPos.y * static_cast<double>(winH_cur_) / fbH_;
            inventoryClick(button, mods);
        };
        auto layout = [&]() { return InvLayout::make(fbW_, fbH_, paletteWindow_, craftSize_); };
        auto center = [&](glm::vec2 p, const InvLayout& L) { return p + glm::vec2(L.pitch() * 0.5f); };

        inventory_.slots[9] = {LOG, 2};
        InvLayout L = layout();
        clickAtPos(center(L.slotPos(9), L), GLFW_MOUSE_BUTTON_LEFT, 0);               // Stapel aufnehmen
        clickAtPos(center(L.craftPos(0), L), GLFW_MOUSE_BUTTON_RIGHT, 0);             // ein Holz ins Raster
        SCHECK(craft_[0].id == LOG && craft_[0].count == 1 && cursor_.count == 1);
        clickAtPos(center(L.slotPos(10), L), GLFW_MOUSE_BUTTON_LEFT, 0);              // Rest ablegen
        SCHECK(matchRecipe(craft_, craftSize_).id == PLANKS);
        clickAtPos(center(L.resultPos, L), GLFW_MOUSE_BUTTON_LEFT, 0);                // Ergebnis nehmen
        SCHECK(cursor_.id == PLANKS && cursor_.count == 4 && craft_[0].empty());
        clickAtPos(center(L.slotPos(11), L), GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(inventory_.slots[11].id == PLANKS && inventory_.slots[11].count == 4);

        // Umschalt-Klick auf das Ergebnis stellt alles auf einmal her
        craft_[1] = {LOG, 1};
        clickAtPos(center(L.resultPos, L), GLFW_MOUSE_BUTTON_LEFT, GLFW_MOD_SHIFT);
        SCHECK(craft_[1].empty() && inventory_.count(PLANKS) == 8);

        // Schliessen gibt Zutaten zurueck
        craft_[3] = {DIRT, 2};
        closeInventory();
        SCHECK(!inventoryOpen_ && inventory_.count(DIRT) == 2 && craft_[3].empty());

        // Werkbank: Spitzhacke herstellen
        inventory_.clear();
        openInventory(true);
        SCHECK(craftSize_ == 3);
        L = layout();
        craft_[0] = craft_[1] = craft_[2] = {PLANKS, 1};
        craft_[4] = craft_[7] = {ITEM_STICK, 1};
        clickAtPos(center(L.resultPos, L), GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.id == ITEM_WOODEN_PICKAXE && cursor_.count == 1 && matchRecipe(craft_, 3).empty());
        clickAtPos(center(L.slotPos(0), L), GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(inventory_.slots[0].id == ITEM_WOODEN_PICKAXE && cursor_.empty());
        closeInventory();

        // Rechtsklick auf die Werkbank oeffnet sie, mit gehaltenem Block nicht setzen
        world_->setBlock(2, gy, 4, CRAFTING_TABLE);
        slot_ = 1;
        inventory_.slots[1] = {DIRT, 5};
        target_ = world_->raycast(glm::vec3(2.5f, gy + 1.5f, 2.5f), glm::vec3(0.0f, -0.5f, 1.0f), 5.0f);
        SCHECK(target_.hit && target_.block == glm::ivec3(2, gy, 4));
        placeTimer_ = 0.0f;
        placeBlockOrUse();
        SCHECK(inventoryOpen_ && craftSize_ == 3 && inventory_.slots[1].count == 5);
        closeInventory();

        // Werkzeug nutzt ab und zerbricht
        inventory_.clear();
        inventory_.slots[0] = {ITEM_WOODEN_AXE, 1, 58};
        slot_ = 0;
        world_->setBlock(1, gy - 1, 3, LOG);
        world_->setBlock(1, gy - 1, 4, LOG);
        breakBlock(glm::ivec3(1, gy - 1, 3));
        SCHECK(inventory_.slots[0].damage == 59);
        breakBlock(glm::ivec3(1, gy - 1, 4));
        SCHECK(inventory_.slots[0].empty());
        world_->setBlock(1, gy - 1, 3, STONE);
        world_->setBlock(1, gy - 1, 4, STONE);
    }

    // 15. Ofen: oeffnen, befuellen, schmelzen, Ausgabe entnehmen, Abbau gibt den Inhalt zurueck
    {
        dead_ = false;
        creative_ = false;
        inventory_.clear();
        entities_.clear();
        cursor_.clear();
        world_->setBlock(-3, gy, 4, FURNACE);
        slot_ = 3;
        inventory_.slots[3] = {DIRT, 5};
        target_ = world_->raycast(glm::vec3(-2.5f, gy + 1.5f, 2.5f), glm::vec3(-0.15f, -0.5f, 1.0f), 5.0f);
        SCHECK(target_.hit && target_.block == glm::ivec3(-3, gy, 4));
        placeTimer_ = 0.0f;
        placeBlockOrUse();
        SCHECK(inventoryOpen_ && furnaceWindow_ && furnacePos_ == glm::ivec3(-3, gy, 4));
        inventory_.slots[9] = {IRON_ORE, 3};
        inventory_.slots[10] = {ITEM_COAL, 2};
        InvLayout L = InvLayout::make(fbW_, fbH_, false, craftSize_, true);
        auto clickAtPos = [&](glm::vec2 fbPos, int button, int mods) {
            curX_ = fbPos.x * static_cast<double>(winW_cur_) / fbW_;
            curY_ = fbPos.y * static_cast<double>(winH_cur_) / fbH_;
            inventoryClick(button, mods);
        };
        auto center = [&](glm::vec2 p) { return p + glm::vec2(L.pitch() * 0.5f); };
        clickAtPos(center(L.slotPos(9)), GLFW_MOUSE_BUTTON_LEFT, 0);              // Erz aufnehmen
        clickAtPos(center(L.furnaceSlotPos(0)), GLFW_MOUSE_BUTTON_LEFT, 0);        // in die Eingabe
        clickAtPos(center(L.slotPos(10)), GLFW_MOUSE_BUTTON_LEFT, 0);              // Kohle aufnehmen
        clickAtPos(center(L.furnaceSlotPos(1)), GLFW_MOUSE_BUTTON_RIGHT, 0);       // eine Kohle als Brennstoff
        clickAtPos(center(L.slotPos(10)), GLFW_MOUSE_BUTTON_LEFT, 0);              // Rest zurueck
        Furnace* fu = world_->findFurnace(-3, gy, 4);
        SCHECK(fu && fu->input.id == IRON_ORE && fu->input.count == 3 && fu->fuel.id == ITEM_COAL && fu->fuel.count == 1);
        SCHECK(cursor_.empty() && inventory_.count(ITEM_COAL) == 1);
        for (int i = 0; i < 230; ++i) world_->tick(0.1f);  // 23 s: zwei Barren
        SCHECK(fu && fu->output.id == ITEM_IRON_INGOT && fu->output.count == 2);
        clickAtPos(center(L.furnaceSlotPos(2)), GLFW_MOUSE_BUTTON_LEFT, 0);        // Ausgabe nehmen
        SCHECK(cursor_.id == ITEM_IRON_INGOT && cursor_.count == 2 && fu->output.empty());
        clickAtPos(center(L.slotPos(12)), GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(inventory_.slots[12].id == ITEM_IRON_INGOT);
        // Umschalt-Klick auf die Eingabe holt den Rest zurueck
        clickAtPos(center(L.furnaceSlotPos(0)), GLFW_MOUSE_BUTTON_LEFT, GLFW_MOD_SHIFT);
        SCHECK(fu->input.empty() && inventory_.count(IRON_ORE) == 1);
        fu->output = {ITEM_IRON_INGOT, 1};
        closeInventory();
        SCHECK(!inventoryOpen_ && !furnaceWindow_);
        // Abbauen: Ofen verschwindet, Inhalt faellt heraus
        breakBlock(glm::ivec3(-3, gy, 4));
        SCHECK(world_->getBlock(-3, gy, 4) == AIR && world_->findFurnace(-3, gy, 4) == nullptr);
        bool foundIngot = false;
        for (const ItemEntity& it : entities_.items())
            if (it.stack.id == ITEM_IRON_INGOT) foundIngot = true;
        SCHECK(foundIngot);
        // Essen: gebratenes Fleisch heilt mehr
        inventory_.slots[2] = {ITEM_COOKED_MEAT, 2};
        slot_ = 2;
        health_ = 5;
        eatTimer_ = 0.0f;
        placeBlockOrUse();
        SCHECK(health_ == 13 && inventory_.slots[2].count == 1);
    }

    // 16. Ansicht umschalten (Ego, von hinten, von vorn)
    {
        dead_ = false;
        inventoryOpen_ = false;
        paused_ = false;
        camMode_ = CameraMode::First;
        handleKey(GLFW_KEY_V, GLFW_PRESS, 0);
        SCHECK(camMode_ == CameraMode::ThirdBack);
        handleKey(GLFW_KEY_V, GLFW_PRESS, 0);
        SCHECK(camMode_ == CameraMode::ThirdFront);
        render();  // mit Spielermodell ohne Fehler zeichnen
        handleKey(GLFW_KEY_V, GLFW_PRESS, 0);
        SCHECK(camMode_ == CameraMode::First);
    }

    // 17. Textur-Editor ueber das Menue bedienen
    {
        dead_ = false;
        inventoryOpen_ = false;
        paused_ = false;
        screen_ = MenuScreen::None;
        textures_.setDirectory("olicraft_selftest/textures");
        std::filesystem::remove_all("olicraft_selftest/textures");
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(paused_ && screen_ == MenuScreen::Pause);
        auto toPx = [&](glm::vec2 fb) { return glm::vec2(static_cast<float>(fb.x * winW_cur_ / fbW_), static_cast<float>(fb.y * winH_cur_ / fbH_)); };
        auto cursorTo = [&](glm::vec2 fb) {
            const glm::vec2 w = toPx(fb);
            curX_ = w.x;
            curY_ = w.y;
            handleCursor(curX_, curY_);
        };
        auto click = [&](glm::vec2 fb) {
            cursorTo(fb);
            handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
            handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
        };
        auto clickMenu = [&](int id) {
            const MenuLayout m = buildMenu(screen_, menuState(), fbW_, fbH_);
            for (const Widget& w : m.widgets)
                if (w.id == id) {
                    click(w.pos + w.size * 0.5f);
                    return true;
                }
            return false;
        };
        auto clickEditor = [&](int id) {
            for (int scroll = 0; scroll < 80; ++scroll) {
                const EditorLayout L = buildEditorLayout(fbW_, fbH_, *editor_, editorUi_);
                for (const Widget& w : L.widgets)
                    if (w.id == id && w.enabled) {
                        click(w.pos + w.size * 0.5f);
                        return true;
                    }
                if (id < W_ED_LIST_BASE) return false;
                editorUi_.listScroll = scroll;  // Listeneintrag erst sichtbar scrollen
            }
            return false;
        };
        SCHECK(clickMenu(W_SETTINGS) && screen_ == MenuScreen::Settings);
        SCHECK(clickMenu(W_EDITOR) && screen_ == MenuScreen::Editor && editor_);
        int stickIndex = -1;
        for (size_t i = 0; i < editor_->entries().size(); ++i)
            if (editor_->entries()[i].name == "Stock") stickIndex = static_cast<int>(i);
        SCHECK(stickIndex >= 0 && clickEditor(W_ED_LIST_BASE + stickIndex) && editor_->current() == stickIndex);
        SCHECK(clickEditor(W_ED_SWATCH_BASE + 9));  // Farbe waehlen
        const auto& pal = editorPalette();
        SCHECK(editor_->color[0] == std::lround(pal[9].x * 255.0f));
        SCHECK(clickEditor(W_ED_PENCIL));
        // Mit gedrueckter Maus ueber die Leinwand ziehen
        {
            const EditorLayout L = buildEditorLayout(fbW_, fbH_, *editor_, editorUi_);
            auto pixelCenter = [&](int x, int y) { return glm::vec2(L.canvas.x + (x + 0.5f) * L.zoom, L.canvas.y + (y + 0.5f) * L.zoom); };
            cursorTo(pixelCenter(3, 3));
            handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
            cursorTo(pixelCenter(8, 6));
            handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
            const Image tile = textures_.tile(TILE_STICK);
            SCHECK(tile.at(3, 3)[0] == editor_->color[0] && tile.at(8, 6)[0] == editor_->color[0] && tile.at(6, 5)[0] == editor_->color[0]);
            SCHECK(editor_->canUndo());
            // Strg+Z macht den ganzen Strich rueckgaengig
            handleKey(GLFW_KEY_Z, GLFW_PRESS, GLFW_MOD_CONTROL);
            SCHECK(textures_.tile(TILE_STICK).rgba == textures_.defaultTile(TILE_STICK).rgba);
            handleKey(GLFW_KEY_Y, GLFW_PRESS, GLFW_MOD_CONTROL);
            SCHECK(textures_.tile(TILE_STICK).at(3, 3)[0] == editor_->color[0]);
            // Rechtsklick nimmt die Farbe auf
            const uint8_t before0 = editor_->color[0];
            editor_->color[0] = 1;
            cursorTo(pixelCenter(3, 3));
            handleMouseButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
            SCHECK(editor_->color[0] == before0);
            // Schieber ziehen
            for (const Widget& w : buildEditorLayout(fbW_, fbH_, *editor_, editorUi_).widgets)
                if (w.id == W_ED_G) {
                    cursorTo(w.pos + glm::vec2(w.size.x * 0.5f, w.size.y * 0.5f));
                    handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
                    SCHECK(std::abs(editor_->color[1] - 128) <= 2);
                    handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
                }
        }
        SCHECK(clickEditor(W_ED_SAVE) && std::filesystem::exists(textures_.tilePath(TILE_STICK)));
        SCHECK(clickEditor(W_ED_EXPORT) && std::filesystem::exists(editor_->exportPath()));
        SCHECK(clickEditor(W_ED_RESET) && textures_.tile(TILE_STICK).rgba == textures_.defaultTile(TILE_STICK).rgba &&
               !std::filesystem::exists(textures_.tilePath(TILE_STICK)));
        SCHECK(clickEditor(W_ED_IMPORT) && textures_.tile(TILE_STICK).at(3, 3)[0] == static_cast<uint8_t>(std::lround(editorPalette()[9].x * 255.0f)));
        renderer_.syncTextures(textures_);  // Hochladen ohne GL-Fehler
        render();                           // Editor mit Vorschau zeichnen
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(paused_ && screen_ == MenuScreen::Settings);
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(!paused_);
        textures_.resetTile(TILE_STICK);
        std::filesystem::remove_all("olicraft_selftest");
    }

    // 18. Fackeln: auf Boden und Wand setzen, leuchten, fallen ab, werden aufgesammelt
    {
        dead_ = false;
        creative_ = false;
        paused_ = false;
        inventoryOpen_ = false;
        inventory_.clear();
        entities_.clear();
        slot_ = 0;
        inventory_.slots[0] = {TORCH, 5};
        placeTimer_ = 0.0f;
        player_.pos = glm::vec3(0.5f, static_cast<float>(gy), 8.5f);
        player_.vel = glm::vec3(0.0f);
        for (int x = 8; x <= 14; ++x)  // Platz freiraeumen (aus frueheren Schritten kann Wasser hierhin geflossen sein)
            for (int z = -10; z <= -2; ++z)
                for (int y = gy; y <= gy + 4; ++y)
                    if (world_->getBlock(x, y, z) != AIR) world_->setBlock(x, y, z, AIR);
        target_ = world_->raycast(glm::vec3(10.5f, gy + 1.5f, -3.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);
        SCHECK(target_.hit && target_.block == glm::ivec3(10, gy - 1, -4) && target_.normal == glm::ivec3(0, 1, 0));
        placeBlockOrUse();
        SCHECK(world_->getBlock(10, gy, -4) == TORCH && inventory_.slots[0].count == 4 && world_->lightAt(10, gy, -4) == 14);
        // an eine Wand: Saeule bei x = 13
        for (int y = gy; y <= gy + 2; ++y) world_->setBlock(13, y, -4, STONE);
        placeTimer_ = 0.0f;
        target_ = world_->raycast(glm::vec3(11.5f, gy + 1.5f, -3.5f), glm::vec3(1.0f, 0.0f, 0.0f), 5.0f);
        SCHECK(target_.hit && target_.block == glm::ivec3(13, gy + 1, -4) && target_.normal == glm::ivec3(-1, 0, 0));
        placeBlockOrUse();
        SCHECK(world_->getBlock(12, gy + 1, -4) == TORCH_PX && inventory_.slots[0].count == 3);
        // an der Decke: nicht moeglich
        for (int x = 9; x <= 11; ++x) world_->setBlock(x, gy + 3, -8, STONE);
        placeTimer_ = 0.0f;
        target_ = world_->raycast(glm::vec3(10.5f, gy + 1.5f, -7.5f), glm::vec3(0.0f, 1.0f, 0.0f), 5.0f);
        SCHECK(target_.hit && target_.normal == glm::ivec3(0, -1, 0));
        placeBlockOrUse();
        SCHECK(inventory_.slots[0].count == 3);
        // auf eine Fackel selbst: nicht moeglich
        placeTimer_ = 0.0f;
        target_ = world_->raycast(glm::vec3(10.5f, gy + 2.5f, -3.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);
        SCHECK(target_.hit && target_.block == glm::ivec3(10, gy, -4));
        placeBlockOrUse();
        SCHECK(inventory_.slots[0].count == 3);
        // Traeger abbauen: die Wandfackel faellt ab
        breakBlock(glm::ivec3(13, gy + 1, -4));
        SCHECK(world_->getBlock(12, gy + 1, -4) == AIR && world_->lightAt(12, gy + 1, -4) < 14);  // nur noch Licht der anderen Fackel
        auto drops = world_->takeTorchDrops();
        SCHECK(drops.size() == 1 && drops[0] == glm::ivec3(12, gy + 1, -4));
        // Fackel abbauen: sofort, mit Beute
        entities_.clear();
        breakBlock(glm::ivec3(10, gy, -4));
        SCHECK(world_->getBlock(10, gy, -4) == AIR && entities_.items().size() == 1 && entities_.items()[0].stack.id == TORCH);
        world_->setBlock(13, gy + 1, -4, AIR);
        world_->setBlock(13, gy, -4, AIR);
        world_->setBlock(13, gy + 2, -4, AIR);
        for (int x = 9; x <= 11; ++x) world_->setBlock(x, gy + 3, -8, AIR);
    }

    // 9. Inventar per Mausklick
    dead_ = false;
    health_ = 20;
    inventory_.clear();
    inventory_.slots[9] = {STONE, 10};
    openInventory();
    SCHECK(inventoryOpen_);
    {
        InvLayout L = InvLayout::make(fbW_, fbH_, paletteWindow_, craftSize_);
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

    // 13. Pause- und Einstellungsmenue
    {
        creative_ = false;
        opt_.noSave = false;
        opt_.settingsPath = "olicraft_selftest/settings.cfg";
        std::filesystem::create_directories("olicraft_selftest");
        const int oldDist = settings_.renderDistance;
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(paused_ && screen_ == MenuScreen::Pause);
        auto clickWidget = [&](int id) {
            const MenuLayout m = buildMenu(screen_, menuState(), fbW_, fbH_);
            for (const Widget& w : m.widgets)
                if (w.id == id) {
                    glm::vec2 c = w.pos + w.size * 0.5f;
                    curX_ = c.x * static_cast<double>(winW_cur_) / fbW_;
                    curY_ = c.y * static_cast<double>(winH_cur_) / fbH_;
                    handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
                    handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
                    return true;
                }
            return false;
        };
        SCHECK(clickWidget(W_SETTINGS) && screen_ == MenuScreen::Settings);
        // Schieberegler an den rechten Rand ziehen
        {
            const MenuLayout m = buildMenu(screen_, menuState(), fbW_, fbH_);
            const Widget* dist = nullptr;
            for (const Widget& w : m.widgets)
                if (w.id == W_DISTANCE) dist = &w;
            SCHECK(dist != nullptr);
            if (dist) {
                const float y = (dist->pos.y + dist->size.y * 0.5f);
                auto cursorTo = [&](float fx) {
                    curX_ = fx * static_cast<double>(winW_cur_) / fbW_;
                    curY_ = y * static_cast<double>(winH_cur_) / fbH_;
                };
                cursorTo(dist->pos.x + 2.0f);
                handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
                SCHECK(settings_.renderDistance == Settings::MIN_DISTANCE);
                handleCursor(curX_ + 0.0, curY_);  // Ziehen
                cursorTo(dist->pos.x + dist->size.x - 1.0f);
                handleCursor(curX_, curY_);
                SCHECK(settings_.renderDistance == Settings::MAX_DISTANCE);
                handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
                cursorTo(dist->pos.x + dist->size.x * 0.5f);
                handleCursor(curX_, curY_);  // ohne gedrueckte Taste keine Aenderung
                SCHECK(settings_.renderDistance == Settings::MAX_DISTANCE);
            }
        }
        SCHECK(clickWidget(W_MODE) && creative_);
        SCHECK(clickWidget(W_MODE) && !creative_);
        const bool oldVsync = settings_.vsync;
        SCHECK(clickWidget(W_VSYNC) && settings_.vsync != oldVsync);
        SCHECK(clickWidget(W_VSYNC));
        SCHECK(clickWidget(W_BACK) && screen_ == MenuScreen::Pause);
        Settings reread;
        SCHECK(loadSettings(opt_.settingsPath, reread) && reread.renderDistance == Settings::MAX_DISTANCE);
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);  // Esc im Pausemenue: weiter
        SCHECK(!paused_ && screen_ == MenuScreen::None);
        // Esc in den Einstellungen geht erst zurueck
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        clickWidget(W_SETTINGS);
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(paused_ && screen_ == MenuScreen::Pause);
        SCHECK(clickWidget(W_RESUME) && !paused_);
        settings_.renderDistance = oldDist;
        opt_.renderDistance = oldDist;
    }

    // 19. Ton: Klangdateien laden (Nullgeraet), Spielereignisse loesen Klaenge aus, Lautstaerke-Regler
    {
        const std::string sdir = "olicraft_selftest_sounds";
        std::filesystem::remove_all(sdir);
        std::filesystem::create_directories(sdir);
        auto wav = [&](const char* name) {
            std::vector<int16_t> pcm(2205, 0);
            for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = static_cast<int16_t>(8000.0 * std::sin(i * 0.05));
            std::ofstream f(sdir + "/" + name + ".wav", std::ios::binary);
            const uint32_t rate = 22050, dataLen = static_cast<uint32_t>(pcm.size() * 2), riff = 36 + dataLen, fmt = 16,
                           byteRate = rate * 2;
            const uint16_t pcmFmt = 1, ch = 1, align = 2, bits = 16;
            f.write("RIFF", 4);
            f.write(reinterpret_cast<const char*>(&riff), 4);
            f.write("WAVEfmt ", 8);
            f.write(reinterpret_cast<const char*>(&fmt), 4);
            f.write(reinterpret_cast<const char*>(&pcmFmt), 2);
            f.write(reinterpret_cast<const char*>(&ch), 2);
            f.write(reinterpret_cast<const char*>(&rate), 4);
            f.write(reinterpret_cast<const char*>(&byteRate), 4);
            f.write(reinterpret_cast<const char*>(&align), 2);
            f.write(reinterpret_cast<const char*>(&bits), 2);
            f.write("data", 4);
            f.write(reinterpret_cast<const char*>(&dataLen), 4);
            f.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(dataLen));
        };
        for (const char* n : {"block_stone", "block_wood", "player_hurt", "pig_idle", "eat", "click"}) wav(n);
        SCHECK(audio_.active());
        SCHECK(audio_.loadSounds(sdir) == 6);
        SCHECK(audio_.variantCount(SoundId::BlockStone) == 1 && audio_.variantCount(SoundId::StepGrass) == 0);
        applyVolumes();

        dead_ = false;
        creative_ = false;
        paused_ = false;
        inventoryOpen_ = false;
        health_ = 20;
        invulnerable_ = 0.0f;
        uint64_t played = audio_.playedCount();
        world_->setBlock(10, gy - 1, -4, STONE);
        breakBlock(glm::ivec3(10, gy - 1, -4));  // Stein abbauen
        SCHECK(audio_.playedCount() == played + 1);
        played = audio_.playedCount();
        playSound(SoundId::StepGrass, player_.pos);  // fehlende Datei: still
        SCHECK(audio_.playedCount() == played);
        damagePlayer(2, glm::vec3(0.0f));
        SCHECK(health_ == 18 && audio_.playedCount() == played + 1);
        played = audio_.playedCount();
        // Mob-Ereignis wird abgeholt und abgespielt
        entities_.soundEvents().push_back({SoundId::PigIdle, player_.pos.x + 3.0f, player_.pos.y, player_.pos.z, 1.0f, 1.0f});
        updateAudio(0.016f);
        SCHECK(audio_.playedCount() == played + 1 && entities_.soundEvents().empty());
        SCHECK(audio_.voiceCount() >= 1);
        // Lautstaerke-Regler in den Einstellungen
        paused_ = true;
        screen_ = MenuScreen::Settings;
        {
            const MenuLayout m = buildMenu(screen_, menuState(), fbW_, fbH_);
            const Widget* vm = nullptr;
            for (const Widget& w : m.widgets)
                if (w.id == W_VOL_MASTER) vm = &w;
            SCHECK(vm != nullptr);
            if (vm) {
                const float old = settings_.masterVolume;
                updateSlider(W_VOL_MASTER, vm->pos.x);
                SCHECK(settings_.masterVolume == 0.0f);
                updateSlider(W_VOL_MASTER, vm->pos.x + vm->size.x);
                SCHECK(settings_.masterVolume == 1.0f);
                settings_.masterVolume = old;
            }
        }
        {   // Skin-Knopf wechselt den Platz und die getragene Textur
            const MenuLayout m = buildMenu(screen_, menuState(), fbW_, fbH_);
            int idx = -1;
            for (size_t i = 0; i < m.widgets.size(); ++i)
                if (m.widgets[i].id == W_SKIN) idx = static_cast<int>(i);
            SCHECK(idx >= 0);
            if (idx >= 0) {
                const int before = settings_.playerSkin;
                activateWidget(idx, m, glm::vec2(0.0f));
                SCHECK(settings_.playerSkin == (before + 1) % PLAYER_SKIN_SLOTS && textures_.playerSlot() == settings_.playerSkin);
                SCHECK(textures_.skin(SKIN_PLAYER).rgba == textures_.playerSkinImage(settings_.playerSkin).rgba);
                for (int i = 1; i < PLAYER_SKIN_SLOTS; ++i) activateWidget(idx, m, glm::vec2(0.0f));
                SCHECK(settings_.playerSkin == before);
            }
        }
        paused_ = false;
        screen_ = MenuScreen::None;
        audio_.stopAll();
        health_ = 20;
        std::filesystem::remove_all(sdir);
    }

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
    const CameraPose cam = computeCamera(*world_, player_.eyePos(), player_.front(), camMode_);
    f.camPos = cam.pos;
    f.view = glm::lookAt(cam.pos, cam.pos + cam.forward, glm::vec3(0.0f, 1.0f, 0.0f));
    const float farPlane = static_cast<float>(opt_.renderDistance + 3) * CHUNK_W * 1.5f;
    f.proj = glm::perspective(glm::radians(settings_.fov), static_cast<float>(fbW_) / static_cast<float>(fbH_), 0.05f, farPlane);
    f.timeOfDay = timeOfDay_;
    f.renderDistance = opt_.renderDistance;
    f.underwater = world_->inWaterAt(f.camPos);
    f.underLava = world_->inLavaAt(f.camPos);
    f.hasTarget = target_.hit && !inventoryOpen_ && !dead_;
    f.target = target_.block;

    entityBatches_.clear();
    entities_.buildMesh(*world_, f.camPos, gameTime_, entityBatches_);
    if (camMode_ != CameraMode::First) {  // eigenes Spielermodell in der Ansicht von hinten oder vorn
        Pose pose;
        pose.pos = player_.pos - f.camPos;
        pose.yaw = glm::radians(player_.yaw);
        pose.walkPhase = walkPhase_;
        pose.walkAmount = walkAmount_;
        pose.time = gameTime_;
        pose.headPitch = glm::radians(player_.pitch);
        pose.roll = dead_ ? std::min(1.0f, deathAnim_ / 0.4f) * 1.5708f : 0.0f;
        const int bx = static_cast<int>(std::floor(player_.pos.x)), bz = static_cast<int>(std::floor(player_.pos.z));
        bool sky = true;
        for (int y = static_cast<int>(std::floor(player_.pos.y + 1.8f)); y < CHUNK_H && sky; ++y)
            sky = !blockInfo(world_->getBlock(bx, y, bz)).blocksSky;
        pose.sky = sky ? 1.0f : 0.0f;
        pose.light = static_cast<float>(world_->lightAt(bx, static_cast<int>(std::floor(player_.pos.y + 1.0f)), bz)) / MAX_LIGHT;
        const bool hurt = damageFlash_ > 0.35f || dead_;
        appendModel(SKIN_PLAYER, pose, hurt ? entityBatches_.skinHurt[SKIN_PLAYER] : entityBatches_.skin[SKIN_PLAYER]);
    }
    f.entities = &entityBatches_;

    HudState& h = f.hud;
    h.show = showHud_;
    h.creative = creative_;
    h.health = health_;
    h.showAir = player_.headInWater || air_ < MAX_AIR - 0.01f;
    h.air = air_ / MAX_AIR;
    h.inventory = &inventory_;
    h.selectedSlot = slot_;
    h.inventoryOpen = inventoryOpen_;
    h.paletteOpen = paletteWindow_;
    h.furnaceOpen = furnaceWindow_;
    if (furnaceWindow_) {
        if (const Furnace* fu = world_->findFurnace(furnacePos_.x, furnacePos_.y, furnacePos_.z)) {
            h.furnaceIn = fu->input;
            h.furnaceFuel = fu->fuel;
            h.furnaceOut = fu->output;
            h.furnaceBurn = fu->burnTotal > 0.0f ? fu->burnTime / fu->burnTotal : 0.0f;
            h.furnaceCook = fu->cookTime / SMELT_SECONDS;
        }
    }
    h.craftSize = craftSize_;
    for (int i = 0; i < 9; ++i) h.craftCells[i] = craft_[i];
    h.craftResult = craftSize_ > 0 ? matchRecipe(craft_, craftSize_) : ItemStack();
    h.cursor = cursor_;
    h.mouse = mousePx();
    h.breakProgress = breakProgress_;
    h.damageFlash = damageFlash_;
    h.dead = dead_;
    if (paused_ && screen_ == MenuScreen::Editor && editor_) {
        EditorHud& eh = h.editor;
        eh.open = true;
        eh.layout = buildEditorLayout(fbW_, fbH_, *editor_, editorUi_);
        eh.hover = hitEditorWidget(eh.layout, mousePx());
        eh.view = &editor_->view();
        eh.regions = editor_->regions();
        eh.grid = editorUi_.grid;
        eh.net = editorUi_.net;
        int hx, hy;
        if (canvasPixel(eh.layout, *editor_, mousePx(), hx, hy)) {
            eh.hoverPixel = {hx, hy};
            eh.hoverEditable = editor_->editable(hx, hy);
        }
        eh.quads = editor_->preview(editorUi_.previewYaw, editorUi_.previewPitch);
        eh.previewSkin = editor_->entry().kind == EditorEntry::Skin ? static_cast<int>(editor_->entry().skin) : -1;
        std::string edName = editor_->entry().name;
        if (editor_->entry().kind == EditorEntry::Skin && editor_->entry().skin == SKIN_PLAYER)
            edName += std::string(" (Platz ") + std::to_string(textures_.playerSlot() + 1) + ": " + playerSkinName(textures_.playerSlot()) + ")";
        eh.title = "Textur-Editor: " + edName + (editor_->modified() ? " *" : "");
        eh.status = editorUi_.status;
    } else if (paused_ && screen_ != MenuScreen::None) {
        h.menu = buildMenu(screen_, menuState(), fbW_, fbH_);
        h.menuHover = hitWidget(h.menu, mousePx());
        if (h.menu.skinPreview.z > 0.0f) {
            if (!skinPreviewEd_) {
                skinPreviewEd_ = std::make_unique<TextureEditor>(textures_);
                for (size_t i = 0; i < skinPreviewEd_->entries().size(); ++i)
                    if (skinPreviewEd_->entries()[i].kind == EditorEntry::Skin && skinPreviewEd_->entries()[i].skin == SKIN_PLAYER)
                        skinPreviewEd_->select(static_cast<int>(i));
            }
            h.menuPreview = skinPreviewEd_->preview(static_cast<float>(glfwGetTime()) * 0.7f, 0.12f);
        }
    }
    h.burning = burnTimer_ > 0.0f;
    h.message = statusMsg_;
    h.messageAlpha = statusTimer_;
    h.itemName = shownItem_ == ITEM_NONE ? "" : itemInfo(shownItem_).name;
    h.itemNameAlpha = itemNameTimer_;
    renderer_.syncTextures(textures_);
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
    world_->exportFurnaces(sf.furnaces);
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
                  player_.flying ? " | Flug" : "", paused_ ? " | PAUSE" : "");
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
        if (paused_ && (opt_.benchFrames > 0 || shotMode) && opt_.demo != "menu" && opt_.demo != "settings" && opt_.demo.rfind("editor", 0) != 0) setPaused(false);
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
