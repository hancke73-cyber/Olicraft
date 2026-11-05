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
#include "Trades.h"
#include "Noise.h"

namespace oc {

namespace {
constexpr int32_t MENU_BACKGROUND_SEED = 2;  // Hintergrundwelt im Hauptmenue
}  // namespace

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
    setSteampunkEnabled(settings_.steampunk);
    if (opt_.distanceCli) settings_.renderDistance = opt_.renderDistance;
    if (opt_.vsyncCli) settings_.vsync = opt_.vsync;
    settings_.clamp();
    setGuiSize(settings_.guiSize);
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
    glfwSetCharCallback(window_, [](GLFWwindow* win, unsigned int cp) { gameFrom(win)->handleChar(cp); });
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
    const bool menuStart = menuAtStart();
    if (!menuStart && !opt_.noSave && !opt_.newWorld && std::filesystem::exists(opt_.savePath)) {
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
    else if (menuStart) seed = MENU_BACKGROUND_SEED;  // Hintergrund des Hauptmenues, wird nie gespeichert
    else seed = static_cast<int32_t>(std::random_device{}() & 0x7FFFFFFFu);
    std::printf("Welt-Seed: %d%s\n", seed, loaded ? " (Spielstand geladen)" : "");
    if (menuStart) opt_.savePath.clear();  // die Hintergrundwelt wird nicht gespeichert
    else if (!opt_.noSave) currentWorld_ = std::filesystem::path(opt_.savePath).stem().string();

    world_ = std::make_unique<World>(seed);
    entities_ = EntityManager(static_cast<uint32_t>(seed) ^ 0xA5A5u);
    if (loaded) {
        applySave(sf);
    } else {
        player_.pos = world_->findSpawn();
        player_.yaw = -90.0f;
        player_.pitch = -10.0f;
        spawnPoint_ = player_.pos;
    }
    if (opt_.forceMode >= 0) creative_ = opt_.forceMode == 1;
    if (creative_ && inventory_.isEmpty())
        for (int i = 0; i < Inventory::HOTBAR; ++i) inventory_.slots[i] = {DEFAULT_HOTBAR[i], 64};
    if (opt_.gotoVillage) {
        if (const Village* v = world_->terrain().villages().nearest(static_cast<int>(player_.pos.x), static_cast<int>(player_.pos.z), 4)) {
            player_.pos = glm::vec3(v->center.x + 0.5f, static_cast<float>(v->groundY + 8), v->center.y + 0.5f);
            player_.flying = true;
            creative_ = creative_ || opt_.forceMode == 1;
            std::printf("Dorf bei %d %d (%zu Gebaeude)\n", v->center.x, v->center.y, v->buildings.size() - 1);
        }
    }
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
    if (opt_.demo == "inv") {
        if (const char* tab = std::getenv("OLICRAFT_TAB")) setCreativeTab(std::atoi(tab));  // Test: Reiter des Kreativ-Inventars
        if (const char* sc = std::getenv("OLICRAFT_SCROLL")) scrollPalette(std::atoi(sc));  // Test: Seite des Auswahlfelds
        openInventory();
    }
    if (opt_.demo == "armor" || opt_.demo == "armorinv") {
        inventory_.clear();
        inventory_.armor[0] = {makeArmorItem(1, 0), 1, 30};
        inventory_.armor[1] = {makeArmorItem(3, 1), 1, 100};
        inventory_.armor[2] = {makeArmorItem(2, 2), 1, 0};
        inventory_.armor[3] = {makeArmorItem(0, 3), 1, 20};
        inventory_.add(makeArmorItem(3, 0), 1);
        inventory_.add(ITEM_LEATHER, 7);
        inventory_.add(ITEM_IRON_INGOT, 12);
        if (opt_.demo == "armorinv") openInventory();
    }
    if (opt_.demo == "trade") {
        inventory_.clear();
        inventory_.add(ITEM_GOLD_INGOT, 5);
        inventory_.add(ITEM_COAL, 20);
        inventory_.add(ITEM_IRON_INGOT, 3);
        openTrade(PROF_SMITH);
    }
    if (opt_.demo == "chest") {
        inventory_.clear();
        inventory_.add(PLANKS, 20);
        inventory_.add(ITEM_STICK, 12);
        inventory_.add(CHEST, 2);
        const glm::ivec3 cp(static_cast<int>(std::floor(player_.pos.x)) + 2, static_cast<int>(std::floor(player_.pos.y)),
                            static_cast<int>(std::floor(player_.pos.z)));
        world_->setBlock(cp.x, cp.y, cp.z, CHEST);
        Chest& ch = world_->chestAt(cp.x, cp.y, cp.z);
        ch.add(ITEM_DIAMOND, 12);
        ch.add(IRON_BLOCK, 3);
        ch.add(ITEM_IRON_PICKAXE, 1, 40);
        ch.add(WOOL_RED, 64);
        ch.add(FLOWER_BLUE, 9);
        openChest(cp);
    }
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
    if (menuAtStart()) showMainMenu();
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

// Partikel: Ereignisse der Mobs, Fackeln, Lava und Oefen (einmal je Frame; ein Chunk pro Frame wird nach Quellen abgesucht)
void Game::updateParticles(float dt) {
    if (!settings_.particles) {
        particles_.clear();
        particleVerts_.clear();
        entities_.effects().clear();
        return;
    }
    for (const EntityManager::Effect& e : entities_.effects()) {
        if (e.type == 0) particles_.emitBlood(e.pos, 6);
        else particles_.emitPuff(e.pos, 10);
    }
    entities_.effects().clear();

    // Quellen in der Naehe: je Frame einen Chunk in 3 Chunks Umgebung nach Fackeln, Lava, Oefen absuchen
    const int pcx = floorDiv(static_cast<int>(std::floor(player_.pos.x)), CHUNK_W), pcz = floorDiv(static_cast<int>(std::floor(player_.pos.z)), CHUNK_W);
    std::vector<std::pair<int, int>> near;
    for (int dz = -3; dz <= 3; ++dz)
        for (int dx = -3; dx <= 3; ++dx) near.emplace_back(pcx + dx, pcz + dz);
    if (!near.empty()) {
        const auto cc = near[emitterScanIdx_++ % near.size()];
        if (const Chunk* ch = world_->findChunk(cc.first, cc.second)) {
            std::vector<Emitter> list;
            for (int i = 0; i < Chunk::VOLUME; ++i) {
                const BlockId b = ch->blocks[static_cast<size_t>(i)];
                if (b == AIR || b == STONE || b == DIRT || b == GRASS) continue;
                const int lx = i % CHUNK_W, lz = (i / CHUNK_W) % CHUNK_W, y = i / (CHUNK_W * CHUNK_W);
                const glm::ivec3 p(cc.first * CHUNK_W + lx, y, cc.second * CHUNK_W + lz);
                if (isTorch(b)) list.push_back({p, 0});
                else if (b == FURNACE_LIT || b == BOILER_LIT) list.push_back({p, 2});
                else if (b == MOTOR || b == STILL) list.push_back({p, 3});
                else if (b == LAVA && y + 1 < CHUNK_H && ch->blocks[static_cast<size_t>(i + CHUNK_W * CHUNK_W)] == AIR && (hash3(p.x, p.y, p.z, 7u) & 3u) == 0u) list.push_back({p, 1});
            }
            emitters_[(static_cast<int64_t>(cc.first) << 32) ^ static_cast<uint32_t>(cc.second)] = std::move(list);
        }
    }
    for (auto it = emitters_.begin(); it != emitters_.end();) {
        const int cx = static_cast<int>(it->first >> 32), cz = static_cast<int>(static_cast<int32_t>(it->first & 0xFFFFFFFF));
        if (std::abs(cx - pcx) > 3 || std::abs(cz - pcz) > 3) {
            it = emitters_.erase(it);
            continue;
        }
        for (const Emitter& e : it->second) {
            const float dx = e.pos.x + 0.5f - player_.pos.x, dz = e.pos.z + 0.5f - player_.pos.z, dy = e.pos.y - player_.pos.y;
            if (dx * dx + dz * dz + dy * dy > 22.0f * 22.0f) continue;
            const float r = static_cast<float>(hash3(e.pos.x, e.pos.y, e.pos.z, static_cast<uint32_t>(frameCounter_)) & 0xFFFFu) / 65535.0f;
            if (e.kind == 0) {
                const BlockId b = world_->getBlock(e.pos.x, e.pos.y, e.pos.z);
                int sx, sy, sz;
                torchSupportOffset(b, sx, sy, sz);
                const glm::vec3 top(e.pos.x + 0.5f + sx * 0.15f, e.pos.y + (b == TORCH ? 0.8f : 0.95f), e.pos.z + 0.5f + sz * 0.15f);
                if (r < dt * 7.0f) particles_.emitFlame(top);
                if (r > 1.0f - dt * 1.2f) particles_.emitSmoke(top + glm::vec3(0.0f, 0.05f, 0.0f));
            } else if (e.kind == 2) {
                if (r < dt * 2.5f) particles_.emitSmoke(glm::vec3(e.pos.x + 0.5f, e.pos.y + 1.05f, e.pos.z + 0.5f));
            } else if (e.kind == 3) {
                const Machine* m = world_->findMachine(e.pos.x, e.pos.y, e.pos.z);
                if (m && m->working && r < dt * 4.0f) particles_.emitPuff(glm::vec3(e.pos.x + 0.5f, e.pos.y + 1.05f, e.pos.z + 0.5f), 1);
            } else if (e.kind == 1) {
                if (r < dt * 0.6f) particles_.emitEmber(glm::vec3(e.pos.x + 0.5f, e.pos.y + 0.9f, e.pos.z + 0.5f));
            }
        }
        ++it;
    }
    particles_.update(dt, *world_);
}

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

bool Game::menuAtStart() const {
    if (opt_.demo == "mainmenu") return true;
    return !opt_.noMenu && !opt_.selfTest && opt_.screenshotPath.empty() && opt_.benchFrames == 0 && opt_.demo.empty() &&
           !opt_.hasPos && !opt_.gotoVillage && !opt_.hasSeed && !opt_.newWorld;
}

void Game::selectWorld(int index) {
    const int n = static_cast<int>(worlds_.size());
    worldSel_ = n == 0 ? -1 : std::max(0, std::min(n - 1, index));
    if (worldSel_ >= 0) {
        if (worldSel_ < worldScroll_) worldScroll_ = worldSel_;
        if (worldSel_ >= worldScroll_ + WORLD_ROWS) worldScroll_ = worldSel_ - WORLD_ROWS + 1;
    }
    worldScroll_ = std::max(0, std::min(std::max(0, n - WORLD_ROWS), worldScroll_));
}

// Liest die Welten im Ordner neu ein und waehlt die laufende (sonst die zuletzt gespielte)
void Game::refreshWorlds() {
    worlds_.clear();
    if (!opt_.noSave) worlds_ = listWorlds(opt_.savesDir);
    int sel = worlds_.empty() ? -1 : 0;
    for (size_t i = 0; i < worlds_.size(); ++i)
        if (worlds_[i].name == currentWorld_) sel = static_cast<int>(i);
    worldScroll_ = 0;
    selectWorld(sel);
}

// Uebernimmt Welt und Spieler aus einer Speicherdatei (World und Entities existieren bereits)
void Game::applySave(const SaveFile& sf) {
    world_->importChunks(sf.chunks);
    world_->importFurnaces(sf.furnaces);
    world_->importChests(sf.chests);
    world_->importSigns(sf.signs);
    world_->importMachines(sf.machines);
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
    spawnPoint_ = sf.player.hasSpawn ? sf.player.spawn : player_.pos;
}

bool Game::loadWorld(const WorldEntry& e) {
    SaveFile sf;
    std::string err;
    if (!readSaveFile(e.path, sf, &err)) {
        showMessage("Welt nicht lesbar: " + err);
        return false;
    }
    resetWorld(sf.seed, sf.player.creative, false);
    applySave(sf);
    world_->loadAround(player_.pos, 3);
    opt_.savePath = e.path;
    currentWorld_ = e.name;
    return true;
}

// Legt aus dem Bildschirm "Neue Welt" eine Welt mit eigener Datei an und startet sie
void Game::createWorld() {
    int32_t seed = static_cast<int32_t>(std::random_device{}() & 0x7FFFFFFFu);
    if (!newSeedText_.empty()) seed = static_cast<int32_t>(std::strtoll(newSeedText_.c_str(), nullptr, 10) & 0x7FFFFFFF);
    std::string name = sanitizeWorldName(newName_);
    if (name.empty()) name = "Neue Welt";
    if (!opt_.noSave) {
        std::error_code ec;
        std::filesystem::create_directories(opt_.savesDir, ec);
        name = uniqueWorldName(opt_.savesDir, name);
        opt_.savePath = worldPathFor(opt_.savesDir, name);
    } else {
        opt_.savePath.clear();
    }
    resetWorld(seed, newCreative_);
    currentWorld_ = name;
    setPaused(false);
    player_.flying = false;
    if (!opt_.noSave) saveWorld();  // die Welt steht sofort in der Liste
}

void Game::showMainMenu() {
    inventoryOpen_ = false;
    furnaceWindow_ = chestWindow_ = tradeWindow_ = false;
    craftSize_ = 0;
    paused_ = true;
    screen_ = MenuScreen::Main;
    dragWidget_ = W_NONE;
    firstMouse_ = true;
    refreshWorlds();
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwGetWindowSize(window_, &winW_cur_, &winH_cur_);
    curX_ = winW_cur_ * 0.5;
    curY_ = winH_cur_ * 0.5;
    glfwSetCursorPos(window_, curX_, curY_);
    camMode_ = CameraMode::First;
    player_.flying = true;  // Kamera schwebt, solange das Menue offen ist
    if (currentWorld_.empty() && opt_.savePath.empty()) player_.pos.y += 14.0f;  // Hintergrundwelt: Kamera etwas hoeher, mehr Aussicht
}

// Beginnt eine neue Welt (der alte Spielstand wird erst beim naechsten Speichern ersetzt)
void Game::resetWorld(int32_t seed, bool creative, bool findSpawn) {
    world_.reset();  // GL-Objekte der Chunks freigeben
    world_ = std::make_unique<World>(seed);
    entities_ = EntityManager(static_cast<uint32_t>(seed) ^ 0xA5A5u);
    inventory_ = Inventory();
    cursor_.clear();
    for (ItemStack& c : craft_) c.clear();
    creative_ = creative;
    health_ = 20;
    dead_ = false;
    air_ = MAX_AIR;
    burnTimer_ = lavaTimer_ = drownTimer_ = 0.0f;
    invulnerable_ = sinceDamage_ = 0.0f;
    damageFlash_ = 0.0f;
    timeOfDay_ = 0.30f;
    dayCount_ = 0;
    slot_ = 0;
    player_.vel = glm::vec3(0.0f);
    // Beim Laden darf noch kein Chunk erzeugt sein: die gespeicherten muessen zuerst eingelesen werden
    player_.pos = findSpawn ? world_->findSpawn() : glm::vec3(0.0f, 80.0f, 0.0f);
    player_.yaw = -90.0f;
    player_.pitch = -10.0f;
    player_.flying = false;
    spawnPoint_ = player_.pos;
    if (creative_)
        for (int i = 0; i < Inventory::HOTBAR; ++i) inventory_.slots[i] = {DEFAULT_HOTBAR[i], 64};
    if (findSpawn) world_->loadAround(player_.pos, 3);
    breaking_ = false;
    target_ = RayHit{};
    std::printf("Neue Welt, Seed: %d\n", seed);
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
    s.shadows = settings_.shadows;
    s.clouds = settings_.clouds;
    s.particles = settings_.particles;
    s.texFilter = settings_.texFilter;
    s.anisotropy = settings_.anisotropy;
    s.guiSize = settings_.guiSize;
    s.steampunk = settings_.steampunk;
    for (const WorldEntry& w : worlds_)
        s.worldLabels.push_back(w.name + "  (" + (w.creative ? "Kreativ" : "Survival") + ", Seed " + std::to_string(w.seed) + ")");
    s.worldSel = worldSel_;
    s.worldScroll = worldScroll_;
    s.newName = newName_;
    s.nameFocus = newNameFocus_;
    s.seedText = newSeedText_;
    s.newCreative = newCreative_;
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
    if (!menu.widgets[index].enabled) return;
    const int id = menu.widgets[index].id;
    if (id >= W_WORLD_BASE && id < W_WORLD_BASE + WORLD_ROWS) {  // Welt waehlen, Doppelklick startet sie
        const int idx = worldScroll_ + (id - W_WORLD_BASE);
        const double now = glfwGetTime();
        const bool twice = idx == worldSel_ && now - lastWorldClick_ < 0.45;
        selectWorld(idx);
        lastWorldClick_ = twice ? -10.0 : now;
        if (twice) {
            for (size_t i = 0; i < menu.widgets.size(); ++i)
                if (menu.widgets[i].id == W_MAIN_PLAY) activateWidget(static_cast<int>(i), menu, mouse);
        }
        return;
    }
    switch (id) {
        case W_WORLD_UP: worldScroll_ = std::max(0, worldScroll_ - 1); break;
        case W_WORLD_DOWN: worldScroll_ = std::min(std::max(0, static_cast<int>(worlds_.size()) - WORLD_ROWS), worldScroll_ + 1); break;
        case W_RESUME: setPaused(false); break;
        case W_SETTINGS:
            settingsFrom_ = screen_ == MenuScreen::Main ? MenuScreen::Main : MenuScreen::Pause;
            screen_ = MenuScreen::Settings;
            break;
        case W_GRAPHICS: screen_ = MenuScreen::Graphics; break;
        case W_GFX_SHADOWS: settings_.shadows = (settings_.shadows + 1) % 3; persistSettings(); break;
        case W_STEAMPUNK: settings_.steampunk = !settings_.steampunk; setSteampunkEnabled(settings_.steampunk); updateSearch(); persistSettings(); break;
        case W_GFX_CLOUDS: settings_.clouds = !settings_.clouds; persistSettings(); break;
        case W_GFX_PARTICLES: settings_.particles = !settings_.particles; persistSettings(); break;
        case W_GFX_FILTER: settings_.texFilter = 1 - settings_.texFilter; persistSettings(); break;
        case W_GUI_SIZE: settings_.guiSize = (settings_.guiSize + 1) % 5; setGuiSize(settings_.guiSize); persistSettings(); break;
        case W_GFX_ANISO: settings_.anisotropy = settings_.anisotropy >= 16 ? 1 : settings_.anisotropy * 2; persistSettings(); break;
        case W_MAIN_PLAY: {
            if (worldSel_ < 0 || worldSel_ >= static_cast<int>(worlds_.size())) break;
            const WorldEntry entry = worlds_[static_cast<size_t>(worldSel_)];
            // Die Welt, die gerade im Speicher ist, wird einfach fortgesetzt, jede andere aus ihrer Datei geladen
            const bool running = !opt_.savePath.empty() && entry.name == currentWorld_;
            if (!running && !loadWorld(entry)) break;
            setPaused(false);
            player_.flying = false;
            break;
        }
        case W_MAIN_NEW:
            screen_ = MenuScreen::NewWorld;
            newSeedText_.clear();
            newName_ = "Neue Welt";
            newNameFocus_ = true;
            newNameFresh_ = true;
            newCreative_ = false;
            break;
        case W_NEW_NAME: newNameFocus_ = true; break;
        case W_NEW_SEED: newNameFocus_ = false; break;
        case W_NEW_MODE: newCreative_ = !newCreative_; break;
        case W_NEW_BACK: screen_ = MenuScreen::Main; break;
        case W_NEW_CREATE: createWorld(); break;
        case W_TO_MAIN:
            if (!opt_.noSave) saveWorld();
            showMainMenu();
            break;
        case W_SAVE: wantSave_ = true; break;
        case W_QUIT: glfwSetWindowShouldClose(window_, GLFW_TRUE); break;
        case W_BACK:
            screen_ = screen_ == MenuScreen::Graphics ? MenuScreen::Settings : settingsFrom_;
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

const std::vector<ItemId>& Game::paletteItems() const {
    return creativeTab_ == CREATIVE_SEARCH_TAB ? searchResults_ : creativeTabItems(creativeTab_);
}

void Game::updateSearch() {
    searchResults_ = creativeSearch(creativeQuery_);
    creativeScroll_ = 0;
}

void Game::setCreativeTab(int tab) {
    creativeTab_ = std::max(0, std::min(CREATIVE_SEARCH_TAB, tab));
    creativeScroll_ = 0;
    if (creativeTab_ == CREATIVE_SEARCH_TAB) updateSearch();
}

void Game::scrollPalette(int rows) {
    const int total = (static_cast<int>(paletteItems().size()) + InvLayout::PALETTE_COLS - 1) / InvLayout::PALETTE_COLS;
    creativeScroll_ = std::max(0, std::min(std::max(0, total - InvLayout::PALETTE_ROWS), creativeScroll_ + rows));
}

void Game::openInventory(bool table) {
    if (dead_) return;
    inventoryOpen_ = true;
    furnaceWindow_ = false;
    chestWindow_ = false;
    tradeWindow_ = false;
    bookScroll_ = 0;
    craftSize_ = table ? 3 : (creative_ ? 0 : 2);
    paletteWindow_ = creative_ && !table;
    for (ItemStack& c : craft_) c.clear();
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwGetWindowSize(window_, &winW_cur_, &winH_cur_);
    curX_ = winW_cur_ * 0.5;
    curY_ = winH_cur_ * 0.5;
    glfwSetCursorPos(window_, curX_, curY_);
}

void Game::openSign(const glm::ivec3& pos) {
    openInventory(true);
    craftSize_ = 0;
    paletteWindow_ = furnaceWindow_ = chestWindow_ = tradeWindow_ = false;
    signWindow_ = true;
    signPos_ = pos;
    signLine_ = 0;
    signText_ = SignText();
    if (const SignText* t = world_->findSign(pos.x, pos.y, pos.z)) signText_ = *t;
}

// Angaben zur angesehenen Dampfmaschine (Name, Zustand, Antrieb)
void Game::openMachine(const glm::ivec3& pos) {
    openInventory(true);
    furnaceWindow_ = true;
    machineWindow_ = true;
    machinePos_ = pos;
    world_->machineAt(pos.x, pos.y, pos.z);
}

ItemStack* Game::machineSlot(Machine& m, int ui) const {
    if (m.kind == MachineKind::Boiler) return ui == 0 ? &m.slots[1] : (ui == 1 ? &m.slots[0] : nullptr);
    if (m.kind == MachineKind::Still) return ui == 0 ? &m.slots[0] : (ui == 1 ? &m.slots[2] : &m.slots[1]);
    return nullptr;
}

bool Game::machineSlotAccepts(const Machine& m, int ui, const ItemStack& s) const {
    if (s.empty()) return true;
    if (m.kind == MachineKind::Boiler) return ui == 0 ? (s.id == ITEM_BUCKET_WATER || s.id == ITEM_BUCKET_DISTILLED) : (ui == 1 && boilerFuelSeconds(s.id) > 0.0f);
    if (m.kind == MachineKind::Still) return ui == 0 && stillRecipe(s.id) != nullptr;
    return false;
}

void Game::openFurnace(const glm::ivec3& pos) {
    openInventory(true);
    furnaceWindow_ = true;
    furnacePos_ = pos;
    world_->furnaceAt(pos.x, pos.y, pos.z);
}

InvLayout Game::invLayout() const {
    return InvLayout::make(fbW_, fbH_, paletteWindow_, craftSize_, furnaceWindow_, chestWindow_,
                           tradeWindow_ ? static_cast<int>(tradesFor(tradeProfession_).size()) : 0);
}

void Game::openTrade(int profession) {
    openInventory(false);
    craftSize_ = 0;
    paletteWindow_ = false;
    tradeWindow_ = true;
    tradeProfession_ = profession;
}

// Kauft das Angebot: Kosten aus dem Rucksack nehmen, Ergebnis hineinlegen (im Kreativmodus kostenlos)
bool Game::buyTrade(int index) {
    const std::vector<Trade>& list = tradesFor(tradeProfession_);
    if (index < 0 || index >= static_cast<int>(list.size())) return false;
    const Trade& t = list[static_cast<size_t>(index)];
    Inventory trial = inventory_;
    if (!creative_) {
        if (trial.count(t.cost.id) < t.cost.count) {
            showMessage(std::string("Es fehlen: ") + std::to_string(t.cost.count) + "x " + itemInfo(t.cost.id).name);
            return false;
        }
        int need = t.cost.count;
        for (int i = Inventory::SIZE - 1; i >= 0 && need > 0; --i) {  // erst aus dem Rucksack, dann aus der Hotbar
            ItemStack& st = trial.slots[static_cast<size_t>(i)];
            if (st.empty() || st.id != t.cost.id) continue;
            const int n = std::min(need, st.count);
            trial.take(i, n);
            need -= n;
        }
    }
    if (trial.add(t.result.id, t.result.count, t.result.damage) > 0) {
        showMessage("Rucksack voll");
        return false;
    }
    inventory_ = trial;
    uiSound(SoundId::VillagerTrade);
    return true;
}

void Game::openChest(const glm::ivec3& pos) {
    openInventory(false);
    craftSize_ = 0;
    paletteWindow_ = false;
    chestWindow_ = true;
    chestPos_ = pos;
    world_->chestAt(pos.x, pos.y, pos.z);
}

bool Game::equipArmor(ItemStack& from) {
    if (from.empty() || !isArmorItem(from.id)) return false;
    ItemStack& slot = inventory_.armor[static_cast<size_t>(armorSlotOf(from.id))];
    std::swap(slot, from);
    uiSound(SoundId::Click, 0.8f);
    return true;
}

void Game::useBed(const glm::ivec3& pos) {
    spawnPoint_ = glm::vec3(pos.x + 0.5f, pos.y + 1.0f, pos.z + 0.5f);
    if (Renderer::sunHeight(timeOfDay_) > 0.05f) {
        showMessage("Wiedereinstiegspunkt gesetzt (schlafen nur nachts)");
        return;
    }
    for (const Mob& m : entities_.mobs())
        if (!m.dead() && mobSpec(m.type).hostile && glm::length(m.pos - spawnPoint_) < 10.0f) {
            showMessage("Du kannst nicht schlafen: Monster in der Naehe");
            return;
        }
    timeOfDay_ = 0.02f;  // Sonnenaufgang
    ++dayCount_;
    if (!creative_) health_ = 20;
    showMessage("Gut geschlafen. Wiedereinstiegspunkt gesetzt");
}

void Game::closeInventory() {
    if (!inventoryOpen_) return;
    if (signWindow_) {  // Text uebernehmen (leerer Text loescht den Eintrag)
        world_->setSign(signPos_.x, signPos_.y, signPos_.z, signText_);
        signWindow_ = false;
    }
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
    machineWindow_ = false;
    chestWindow_ = false;
    tradeWindow_ = false;
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

std::vector<Game::BookItem> Game::buildBook() const {
    // Vorrat: Rucksack und Handwerksraster
    auto have = [&](ItemId id) {
        int n = inventory_.count(id);
        for (const ItemStack& cs : craft_)
            if (!cs.empty() && cs.id == id) n += cs.count;
        return n;
    };
    struct Group {
        ItemId id;
        int count;
        std::vector<const Recipe*> variants;
    };
    std::vector<Group> groups;
    for (const Recipe& r : recipes()) {
        bool found = false;
        for (Group& g : groups)
            if (g.id == r.result.id && g.count == r.result.count) {
                g.variants.push_back(&r);
                found = true;
                break;
            }
        if (!found) groups.push_back({r.result.id, r.result.count, {&r}});
    }
    std::vector<BookItem> items;
    for (const Group& g : groups) {
        BookItem best;
        for (const Recipe* r : g.variants) {
            BookItem it;
            it.recipe = r;
            it.hud.result = r->result;
            it.hud.fits = r->gridNeeded() <= craftSize_;
            it.hud.ingredients = recipeIngredients(*r);
            bool ok = true;
            for (const Ingredient& ing : it.hud.ingredients) ok = ok && (creative_ || have(ing.item) >= ing.count);
            it.hud.craftable = ok && it.hud.fits;
            if (!best.recipe || (it.hud.craftable && !best.hud.craftable)) best = it;
        }
        items.push_back(best);
    }
    std::stable_partition(items.begin(), items.end(), [](const BookItem& b) { return b.hud.craftable; });
    if (bookOnlyCraftable_) items.erase(std::remove_if(items.begin(), items.end(), [](const BookItem& b) { return !b.hud.craftable; }), items.end());
    return items;
}

void Game::clampBookScroll(int total) {
    const InvLayout L = invLayout();
    const BookLayout B = BookLayout::make(L, fbW_, fbH_);
    bookScroll_ = std::max(0, std::min(bookScroll_, B.maxScroll(total)));
}

// Raster nach dem Rezept belegen: das Raster wird zuerst in den Rucksack zurueckgelegt, dann je Feld eine Zutat entnommen
void Game::autofillRecipe(const Recipe& r) {
    const int n = craftSize_;
    if (r.gridNeeded() > n) {
        showMessage("Dafuer braucht man eine Werkbank");
        return;
    }
    Inventory trial = inventory_;
    for (const ItemStack& cs : craft_)
        if (!cs.empty() && trial.add(cs.id, cs.count, cs.damage) > 0) {
            showMessage("Rucksack voll");
            return;
        }
    const std::vector<ItemId> layout = recipeLayout(r, n);
    ItemStack cells[9];
    for (int i = 0; i < n * n; ++i) {
        const ItemId want = layout[static_cast<size_t>(i)];
        if (want == ITEM_NONE) continue;
        if (creative_) {
            cells[i] = {want, 1, 0};
            continue;
        }
        int slot = -1;
        for (int k = 0; k < Inventory::SIZE && slot < 0; ++k)
            if (!trial.slots[static_cast<size_t>(k)].empty() && trial.slots[static_cast<size_t>(k)].id == want) slot = k;
        if (slot < 0) {
            showMessage("Es fehlen Zutaten");
            return;
        }
        cells[i] = {want, 1, trial.slots[static_cast<size_t>(slot)].damage};
        trial.take(slot, 1);
    }
    inventory_ = trial;
    for (int i = 0; i < 9; ++i) craft_[i] = cells[i];
    uiSound(SoundId::Click, 0.6f);
}

void Game::inventoryClick(int button, int mods) {
    if (signWindow_) return;
    const bool right = button == GLFW_MOUSE_BUTTON_RIGHT;
    const InvLayout layout = invLayout();
    const glm::vec2 m = mousePx();
    uiSound(SoundId::Click, 0.4f);

    // Rezeptbuch
    if (bookVisible()) {
        const BookLayout B = BookLayout::make(layout, fbW_, fbH_);
        if (B.hitFilter(m)) {
            bookOnlyCraftable_ = !bookOnlyCraftable_;
            bookScroll_ = 0;
            return;
        }
        if (B.inside(m)) {
            const std::vector<BookItem> list = buildBook();
            clampBookScroll(static_cast<int>(list.size()));
            const int idx = B.hitEntry(m, static_cast<int>(list.size()), bookScroll_);
            if (idx >= 0 && !right) autofillRecipe(*list[static_cast<size_t>(idx)].recipe);
            return;
        }
    }

    // Handel
    if (tradeWindow_) {
        const int ti = layout.hitTrade(m);
        if (ti >= 0) {
            if (!right) buyTrade(ti);
            return;
        }
    }

    // Truhe
    if (chestWindow_) {
        Chest* ch = world_->findChest(chestPos_.x, chestPos_.y, chestPos_.z);
        if (ch) {
            const int cs = layout.hitChest(m);
            if (cs >= 0) {
                ItemStack& st = ch->slots[static_cast<size_t>(cs)];
                if ((mods & GLFW_MOD_SHIFT) && !right) {
                    if (!st.empty()) {
                        const int left = inventory_.add(st.id, st.count, st.damage);
                        if (left <= 0) st.clear();
                        else st.count = left;
                    }
                } else {
                    Inventory::click(st, cursor_, right);
                }
                return;
            }
            const int is = layout.hitSlot(m);
            if (is >= 0 && (mods & GLFW_MOD_SHIFT) && !right) {  // Rucksack -> Truhe
                ItemStack& st = inventory_.slots[static_cast<size_t>(is)];
                if (!st.empty()) {
                    const int left = ch->add(st.id, st.count, st.damage);
                    if (left <= 0) st.clear();
                    else st.count = left;
                }
                return;
            }
        }
    }

    if (furnaceWindow_ && machineWindow_) {
        Machine* mach = world_->findMachine(machinePos_.x, machinePos_.y, machinePos_.z);
        const int fslot = layout.hitFurnace(m);
        ItemStack* st = mach && fslot >= 0 ? machineSlot(*mach, fslot) : nullptr;
        if (st) {
            const bool output = mach->kind == MachineKind::Still && fslot != 0;
            if ((mods & GLFW_MOD_SHIFT) && !right) {
                if (!st->empty()) {  // in den Rucksack verschieben
                    const int left = inventory_.add(st->id, st->count, st->damage);
                    if (left <= 0) st->clear();
                    else st->count = left;
                }
            } else if (output) {  // Ausgabe: nur entnehmen
                if (cursor_.empty()) {
                    cursor_ = *st;
                    st->clear();
                } else if (cursor_.sameKind(*st) && cursor_.count + st->count <= itemInfo(cursor_.id).maxStack) {
                    cursor_.count += st->count;
                    st->clear();
                }
            } else if (cursor_.empty() || machineSlotAccepts(*mach, fslot, cursor_)) {
                Inventory::click(*st, cursor_, right);
            }
            return;
        }
        if (fslot >= 0) return;  // unbenutzter Platz
    } else if (furnaceWindow_) {
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

    if (paletteWindow_) {
        const int tab = layout.hitTab(m);
        if (tab >= 0) {
            setCreativeTab(tab);
            uiSound(SoundId::Click, 0.7f);
            return;
        }
        if (layout.hitScrollbar(m)) {  // Klick in die Leiste springt an die Stelle
            const int total = (static_cast<int>(paletteItems().size()) + InvLayout::PALETTE_COLS - 1) / InvLayout::PALETTE_COLS;
            const int maxScroll = std::max(0, total - InvLayout::PALETTE_ROWS);
            const float f = std::max(0.0f, std::min(1.0f, (m.y - layout.scrollbarTop()) / layout.scrollbarHeight()));
            creativeScroll_ = static_cast<int>(std::lround(f * static_cast<float>(maxScroll)));
            return;
        }
    }
    const int pal = layout.hitPalette(m);
    if (pal >= 0) {
        const size_t idx = static_cast<size_t>(creativeScroll_ * InvLayout::PALETTE_COLS + pal);
        if (idx >= paletteItems().size()) return;
        const ItemId id = paletteItems()[idx];
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
    {
        const int as = layout.hitArmor(m);
        if (as >= 0) {
            ItemStack& st = inventory_.armor[static_cast<size_t>(as)];
            if (cursor_.empty()) {
                cursor_ = st;
                st.clear();
            } else if (isArmorItem(cursor_.id) && armorSlotOf(cursor_.id) == as) {
                std::swap(st, cursor_);
            }
            return;
        }
    }
    const int slot = layout.hitSlot(m);
    if (slot >= 0) {
        if ((mods & GLFW_MOD_SHIFT) && !right && !inventory_.slots[static_cast<size_t>(slot)].empty() && isArmorItem(inventory_.slots[static_cast<size_t>(slot)].id) &&
            inventory_.armor[static_cast<size_t>(armorSlotOf(inventory_.slots[static_cast<size_t>(slot)].id))].empty()) {
            equipArmor(inventory_.slots[static_cast<size_t>(slot)]);
        } else if ((mods & GLFW_MOD_SHIFT) && !right) inventory_.quickMove(slot);
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
    if (paused_ && screen_ == MenuScreen::NewWorld) {  // Name und Seed eintippen (Zeichen kommen ueber handleChar)
        std::string& field = newNameFocus_ ? newName_ : newSeedText_;
        if (key == GLFW_KEY_BACKSPACE && !field.empty()) {
            if (newNameFocus_ && newNameFresh_) field.clear();
            else field.pop_back();
            newNameFresh_ = false;
        } else if (key == GLFW_KEY_TAB) {
            newNameFocus_ = !newNameFocus_;
        } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            createWorld();
            return;
        }
    }
    if (paused_ && screen_ == MenuScreen::Main) {  // Weltliste mit Pfeiltasten
        if (key == GLFW_KEY_UP) selectWorld(worldSel_ - 1);
        else if (key == GLFW_KEY_DOWN) selectWorld(worldSel_ + 1);
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            const MenuLayout menu = buildMenu(screen_, menuState(), fbW_, fbH_);
            for (size_t i = 0; i < menu.widgets.size(); ++i)
                if (menu.widgets[i].id == W_MAIN_PLAY) activateWidget(static_cast<int>(i), menu, glm::vec2(0.0f));
            return;
        }
    }
    switch (key) {
        case GLFW_KEY_ESCAPE:
            if (inventoryOpen_) closeInventory();
            else if (paused_ && screen_ == MenuScreen::Editor) {
                screen_ = MenuScreen::Settings;
                dragWidget_ = W_NONE;
                painting_ = false;
            } else if (paused_ && screen_ == MenuScreen::Graphics) {
                screen_ = MenuScreen::Settings;
                persistSettings();
            } else if (paused_ && screen_ == MenuScreen::Settings) {
                screen_ = settingsFrom_;
                persistSettings();
            } else if (paused_ && screen_ == MenuScreen::NewWorld) {
                screen_ = MenuScreen::Main;
            } else if (paused_ && screen_ == MenuScreen::Main) {
                // Esc im Hauptmenue tut nichts (Beenden ueber den Knopf)
            } else setPaused(!paused_);
            return;
        case GLFW_KEY_Q:
            if (paused_ && screen_ != MenuScreen::NewWorld) glfwSetWindowShouldClose(window_, GLFW_TRUE);
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
    if (inventoryOpen_ && signWindow_) {  // Tasten gehoeren dem Schildtext (Zeichen kommen ueber handleChar)
        std::string& line = signText_.lines[static_cast<size_t>(signLine_)];
        if (key == GLFW_KEY_BACKSPACE && !line.empty()) line.pop_back();
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_DOWN) signLine_ = (signLine_ + 1) % SignText::LINES;
        else if (key == GLFW_KEY_UP) signLine_ = (signLine_ + SignText::LINES - 1) % SignText::LINES;
        return;
    }
    if (inventoryOpen_ && paletteWindow_ && creativeTab_ == CREATIVE_SEARCH_TAB) {  // Tasten gehoeren dem Suchfeld
        if (key == GLFW_KEY_BACKSPACE && !creativeQuery_.empty()) {
            creativeQuery_.pop_back();
            updateSearch();
        }
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

void Game::handleChar(unsigned int cp) {
    if (!paused_ && inventoryOpen_ && signWindow_) {  // Schildtext
        std::string& line = signText_.lines[static_cast<size_t>(signLine_)];
        const std::string add = signCharText(cp);
        if (!add.empty() && line.size() + add.size() <= SignText::MAX_CHARS) line += add;
        return;
    }
    if (!paused_ && inventoryOpen_ && paletteWindow_ && creativeTab_ == CREATIVE_SEARCH_TAB) {  // Suchfeld
        std::string add;
        if (cp >= 32 && cp < 127) add = std::string(1, static_cast<char>(cp));
        else if (cp == 0xE4 || cp == 0xC4) add = "ae";
        else if (cp == 0xF6 || cp == 0xD6) add = "oe";
        else if (cp == 0xFC || cp == 0xDC) add = "ue";
        else if (cp == 0xDF) add = "ss";
        if (!add.empty() && creativeQuery_.size() + add.size() <= 20) {
            creativeQuery_ += add;
            updateSearch();
        }
        return;
    }
    if (!paused_ || screen_ != MenuScreen::NewWorld) return;
    if (newNameFocus_) {
        if (!worldNameChar(static_cast<int>(cp))) return;
        if (newNameFresh_) newName_.clear();
        newNameFresh_ = false;
        if (newName_.size() < MAX_WORLD_NAME) newName_ += static_cast<char>(cp);
    } else if (cp >= '0' && cp <= '9' && newSeedText_.size() < 9) {
        newSeedText_ += static_cast<char>(cp);
    }
}

void Game::handleScroll(double dy) {
    if (paused_ && screen_ == MenuScreen::Main) {
        worldScroll_ += dy > 0 ? -1 : (dy < 0 ? 1 : 0);
        worldScroll_ = std::max(0, std::min(std::max(0, static_cast<int>(worlds_.size()) - WORLD_ROWS), worldScroll_));
        return;
    }
    if (paused_ && screen_ == MenuScreen::Editor) {
        editorScroll(dy);
        return;
    }
    if (inventoryOpen_ && bookVisible() && !paused_) {
        const InvLayout L = invLayout();
        const BookLayout B = BookLayout::make(L, fbW_, fbH_);
        if (B.inside(mousePx())) {
            bookScroll_ += dy > 0 ? -1 : (dy < 0 ? 1 : 0);
            clampBookScroll(static_cast<int>(buildBook().size()));
        }
        return;
    }
    if (inventoryOpen_ && paletteWindow_ && !paused_) {
        scrollPalette(dy > 0 ? -1 : (dy < 0 ? 1 : 0));
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
    // Ruestung mindert Treffer von Monstern (4 % je Punkt, hoechstens 80 %) und nutzt sich ab
    if (glm::length(knockback) > 0.0f) {
        const int pts = inventory_.armorPoints();
        if (pts > 0) {
            amount = std::max(1, static_cast<int>(std::ceil(static_cast<float>(amount) * (1.0f - std::min(0.8f, pts * 0.04f)) - 0.001f)));
            for (ItemStack& a : inventory_.armor) {
                if (a.empty()) continue;
                if (++a.damage >= itemInfo(a.id).maxDurability) {
                    a.clear();
                    showMessage("Ruestungsteil zerbrochen");
                }
            }
        }
    }
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

    // Kaktus: Beruehrung verletzt
    if (!creative_ && !dead_) {
        bool touching = false;
        for (float dx : {-0.42f, 0.0f, 0.42f})
            for (float dz : {-0.42f, 0.0f, 0.42f})
                for (float dy : {0.1f, 0.9f, 1.7f})
                    touching = touching || world_->getBlock(static_cast<int>(std::floor(player_.pos.x + dx)),
                                                            static_cast<int>(std::floor(player_.pos.y + dy)),
                                                            static_cast<int>(std::floor(player_.pos.z + dz))) == CACTUS;
        if (touching) {
            cactusTimer_ += dt;
            if (cactusTimer_ >= 0.5f) {
                cactusTimer_ = 0.0f;
                damagePlayer(1, glm::vec3(0.0f));
            }
        } else {
            cactusTimer_ = 0.0f;
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
        const float wraps = std::floor(timeOfDay_);
        timeOfDay_ -= wraps;
        dayCount_ += static_cast<int>(wraps);
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
        env.timeOfDay = timeOfDay_;
        env.playerDead = dead_;
        env.allowSpawns = opt_.mobs && (!shotMode || opt_.demo == "village");
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
            if (player_.inWater && !wasInWater_ && !player_.inLava) {
                playSound(SoundId::WaterSplash, player_.pos);
                if (settings_.particles) particles_.emitSplash(player_.pos + glm::vec3(0.0f, 0.5f, 0.0f), 18, std::min(4.0f, std::fabs(player_.vel.y) * 0.3f));
            }
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
        for (const MachineDrop& d : world_->takeMachineDrops())
            entities_.spawnItem(d.item, glm::vec3(d.pos) + glm::vec3(0.5f, 1.1f, 0.5f), glm::vec3(0.0f, 2.0f, 0.0f), 0.4f);
        for (const glm::ivec3& p : world_->takeTorchDrops())
            if (!creative_) entities_.spawnItem({TORCH, 1}, glm::vec3(p.x + 0.5f, p.y + 0.2f, p.z + 0.5f), glm::vec3(0.0f, 2.0f, 0.0f), 0.3f);
        for (const World::PlantDrop& d : world_->takePlantDrops()) {
            const ItemId item = blockDropItem(d.block);
            if (!creative_ && item != ITEM_NONE)
                entities_.spawnItem({item, 1}, glm::vec3(d.pos.x + 0.5f, d.pos.y + 0.2f, d.pos.z + 0.5f), glm::vec3(0.0f, 2.0f, 0.0f), 0.3f);
        }
    } else {
        target_ = RayHit{};
        if (screen_ == MenuScreen::Editor && editorUi_.rotate) editorUi_.previewYaw += dt * 0.9f;
        if (screen_ == MenuScreen::Main || screen_ == MenuScreen::NewWorld) {  // Kamerafahrt als Hintergrund
            player_.yaw += dt * 5.0f;
            player_.pitch = -8.0f;
            timeOfDay_ += dt / DAY_SECONDS * 0.5f;
            timeOfDay_ -= std::floor(timeOfDay_);
        }
    }

    // Anzeige des gewaehlten Gegenstands
    {
        const ItemId cur = inventory_.slots[slot_].empty() ? ITEM_NONE : inventory_.slots[slot_].id;
        if (cur != shownItem_) {
            shownItem_ = cur;
            itemNameTimer_ = cur == ITEM_NONE ? 0.0f : 2.0f;
        }
    }

    updateParticles(dt);
    updateAudio(dt);
    world_->update(player_.pos, opt_.renderDistance, 5.0);

    if (wantSave_) {
        wantSave_ = false;
        if (saveWorld()) showMessage("Gespeichert");
    }
}

void Game::breakBlock(const glm::ivec3& pIn) {
    glm::ivec3 p = pIn;
    BlockId b = world_->getBlock(p.x, p.y, p.z);
    if (isDoor(b) && doorUpper(b)) {  // die obere Haelfte gehoert zur unteren
        p.y -= 1;
        b = world_->getBlock(p.x, p.y, p.z);
        if (!isDoor(b)) {
            p = pIn;
            b = world_->getBlock(p.x, p.y, p.z);
        }
    }
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
    if (isMachineBlock(b)) {  // Inhalt von Kessel und Destille faellt heraus
        Machine content;
        if (world_->removeMachine(p.x, p.y, p.z, &content) && !creative_)
            for (const ItemStack& st : content.slots)
                if (!st.empty())
                    entities_.spawnItem(st, glm::vec3(p.x + 0.5f, p.y + 0.4f, p.z + 0.5f), glm::vec3(0.0f, 2.5f, 0.0f), 0.4f);
    }
    if (b == CHEST) {  // Inhalt faellt heraus
        Chest content;
        if (world_->removeChest(p.x, p.y, p.z, &content))
            for (const ItemStack& st : content.slots)
                if (!st.empty())
                    entities_.spawnItem(st, glm::vec3(p.x + 0.5f, p.y + 0.4f, p.z + 0.5f),
                                        glm::vec3(static_cast<float>(hash3(p.x, p.y, p.z, st.id) & 7u) / 4.0f - 1.0f, 2.5f, 0.5f), 0.4f);
    }
    world_->setBlock(p.x, p.y, p.z, AIR);
    if (settings_.particles) particles_.emitBlockBreak(*world_, p, b);
    playSound(blockSound(blockMaterial(b)), glm::vec3(p.x + 0.5f, p.y + 0.5f, p.z + 0.5f));
    if (!creative_) {
        ItemId drop = harvest ? blockDropItem(b) : ITEM_NONE;
        // Laub gibt nur manchmal etwas her
        if (isLeaves(b) && (hash3(p.x, p.y, p.z, 99u) & 7u) >= 3u) drop = ITEM_NONE;
        // Samen aus Gras, mehr Ertrag bei reifen Pflanzen
        const uint32_t hv = hash3(p.x, p.y, p.z, 31u);
        int extraSeeds = 0, extraCarrots = 0;
        if (b == TALL_GRASS && (hv & 7u) == 0u) drop = ITEM_SEEDS;
        if (cropFullyGrown(b) && b <= WHEAT_3) extraSeeds = 1 + static_cast<int>(hv % 3u);
        if (cropFullyGrown(b) && b >= CARROT_0) extraCarrots = 1 + static_cast<int>(hv % 3u);
        if (extraSeeds > 0)
            entities_.spawnItem({ITEM_SEEDS, extraSeeds}, glm::vec3(p.x + 0.5f, p.y + 0.25f, p.z + 0.5f), glm::vec3(0.5f, 3.0f, -0.5f), 0.3f);
        if (extraCarrots > 0)
            entities_.spawnItem({ITEM_CARROT, extraCarrots}, glm::vec3(p.x + 0.5f, p.y + 0.25f, p.z + 0.5f), glm::vec3(-0.5f, 3.0f, 0.5f), 0.3f);
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
        if (tb == CHEST) {
            openChest(target_.block);
            return;
        }
        if (isAnyBed(tb)) {
            useBed(target_.block);
            return;
        }
        if (isSign(tb)) {
            openSign(target_.block);
            return;
        }
        if ((isBoiler(tb) || tb == STILL) && steampunkEnabled()) {
            openMachine(target_.block);
            return;
        }
        if (isDoor(tb) || isGate(tb)) {
            if (world_->toggleOpenGroup(target_.block.x, target_.block.y, target_.block.z) > 0) {
                placeTimer_ = 0.25f;
                playSound(SoundId::BlockWood, glm::vec3(target_.block) + glm::vec3(0.5f), 0.6f, 0.75f);
            }
            return;
        }
    }
    if (sel.empty() || (!steampunkEnabled() && isSteampunkItem(sel.id))) return;
    if (sel.id == ITEM_BUCKET || sel.id == ITEM_BUCKET_WATER || sel.id == ITEM_BUCKET_LAVA) {
        useBucket();
        return;
    }

    // Ruestung anlegen
    if (isArmorItem(sel.id)) {
        equipArmor(sel);
        placeTimer_ = 0.3f;
        return;
    }
    // Hacke: Gras oder Erde zu Acker umgraben
    if (itemInfo(sel.id).tool == TOOL_HOE) {
        if (!target_.hit || target_.normal.y <= 0) return;
        const BlockId tb = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
        const BlockId above = world_->getBlock(target_.block.x, target_.block.y + 1, target_.block.z);
        if ((tb == GRASS || tb == DIRT) && (above == AIR || isPlant(above))) {
            if (isPlant(above)) world_->setBlock(target_.block.x, target_.block.y + 1, target_.block.z, AIR);
            world_->setBlock(target_.block.x, target_.block.y, target_.block.z, FARMLAND);
            placeTimer_ = 0.25f;
            playSound(SoundId::BlockDirt, glm::vec3(target_.block) + glm::vec3(0.5f), 0.8f, 0.9f);
            useHeldTool(1);
        }
        return;
    }
    // Samen und Karotten pflanzen (auf Acker), sonst Karotten essen
    if (sel.id == ITEM_SEEDS || sel.id == ITEM_CARROT) {
        if (target_.hit && target_.normal.y > 0) {
            const glm::ivec3 tp = target_.block;
            if (world_->getBlock(tp.x, tp.y, tp.z) == FARMLAND && world_->getBlock(tp.x, tp.y + 1, tp.z) == AIR) {
                world_->setBlock(tp.x, tp.y + 1, tp.z, sel.id == ITEM_SEEDS ? WHEAT_0 : CARROT_0);
                placeTimer_ = 0.2f;
                playSound(SoundId::BlockGrass, glm::vec3(tp) + glm::vec3(0.5f, 1.0f, 0.5f), 0.7f, 1.1f);
                if (!creative_) inventory_.take(slot_, 1);
                return;
            }
        }
        if (sel.id == ITEM_SEEDS) return;
    }
    if (sel.id == ITEM_MEAT || sel.id == ITEM_COOKED_MEAT || sel.id == ITEM_BERRIES || sel.id == ITEM_BREAD || sel.id == ITEM_CARROT) {
        if (!creative_ && health_ < 20 && eatTimer_ <= 0.0f) {
            const int heal = sel.id == ITEM_COOKED_MEAT ? 8 : (sel.id == ITEM_BREAD ? 5 : (sel.id == ITEM_CARROT ? 3 : (sel.id == ITEM_BERRIES ? 2 : 4)));
            health_ = std::min(20, health_ + heal);
            inventory_.take(slot_, 1);
            eatTimer_ = 0.8f;
            uiSound(SoundId::Eat);
        }
        return;
    }
    // Tueren sind Gegenstaende, die zwei Bloecke belegen
    if ((sel.id == ITEM_DOOR_WOOD || sel.id == ITEM_DOOR_IRON) && target_.hit) {
        glm::ivec3 p = target_.block + target_.normal;
        const BlockId tb = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
        if (tb == TALL_GRASS || tb == FERN || tb == DEAD_BUSH) p = target_.block;
        auto freeCell = [&](int y) {
            const BlockId c = world_->getBlock(p.x, y, p.z);
            return c == AIR || isPlant(c);
        };
        const BlockId below = world_->getBlock(p.x, p.y - 1, p.z);
        if (p.y < 1 || p.y + 1 >= CHUNK_H || !freeCell(p.y) || !freeCell(p.y + 1) || !blockInfo(below).solid || isDoor(below) ||
            blockInfo(below).shape == SH_LANTERN)
            return;
        const int mat = sel.id == ITEM_DOOR_IRON ? 1 : 0;
        const glm::vec3 f = player_.front();
        const int facing = std::fabs(f.x) >= std::fabs(f.z) ? (f.x > 0 ? 0 : 1) : (f.z > 0 ? 2 : 3);
        Box door[MAX_BOXES];
        auto get = [this](int gx, int gy, int gz) { return world_->getBlock(gx, gy, gz); };
        int nb = blockBoxes(makeDoor(mat, false, false, facing), p.x, p.y, p.z, get, false, door);
        if (player_.intersectsBoxes(p.x, p.y, p.z, door, nb) || player_.intersectsBoxes(p.x, p.y + 1, p.z, door, nb)) return;
        world_->setBlock(p.x, p.y, p.z, makeDoor(mat, false, false, facing));
        world_->setBlock(p.x, p.y + 1, p.z, makeDoor(mat, true, false, facing));
        placeTimer_ = 0.25f;
        playSound(blockSound(Material::Wood), glm::vec3(p) + glm::vec3(0.5f), 0.8f, 0.9f);
        if (!creative_) inventory_.take(slot_, 1);
        return;
    }
    if (!itemIsBlock(sel.id) || !target_.hit) return;

    glm::ivec3 p = target_.block + target_.normal;
    if (target_.normal.x == 0 && target_.normal.y == 0 && target_.normal.z == 0) return;
    // Hohes Gras, Farn und Toter Busch werden einfach ersetzt
    const BlockId targetBlock = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
    const bool replace = targetBlock == TALL_GRASS || targetBlock == FERN || targetBlock == DEAD_BUSH;
    if (replace) p = target_.block;
    if (p.y < 0 || p.y >= CHUNK_H) return;
    const BlockId cur = world_->getBlock(p.x, p.y, p.z);
    if (cur != AIR && !replace && !blockInfo(cur).liquid) return;
    BlockId id = itemBlock(sel.id);
    if (isPlant(id) && (cur != AIR && !replace)) return;
    if (isPlant(id) && !plantCanStandOn(id, world_->getBlock(p.x, p.y - 1, p.z))) return;
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
    // Bauteile: Ausrichtung und Besonderheiten
    {
        const glm::vec3 f = player_.front();
        const int lookDir = std::fabs(f.x) >= std::fabs(f.z) ? (f.x > 0 ? 0 : 1) : (f.z > 0 ? 2 : 3);
        const glm::vec3 hitPoint = player_.eyePos() + f * (target_.dist + 0.002f);
        if (isStairs(id)) {
            // wie Platten: an die Unterseite oder in die obere Haelfte einer Seitenflaeche gesetzt, steht die Treppe umgedreht
            bool top;
            if (target_.normal.y > 0) top = false;
            else if (target_.normal.y < 0) top = true;
            else top = hitPoint.y - static_cast<float>(p.y) > 0.5f;
            id = makeStairs(stairMaterial(id), lookDir, top);
        } else if (isDrill(id)) {
            // der Bohrer zeigt in Blickrichtung, bei steilem Blick nach oben oder unten
            id = makeDrill(f.y > 0.75f ? 4 : (f.y < -0.75f ? 5 : lookDir));
        } else if (isConveyor(id)) {
            id = makeConveyor(lookDir);  // das Band laeuft in Blickrichtung
        } else if (isSign(id)) {
            // Von oben auf einen Block: Stehschild, dessen Vorderseite zum Spieler zeigt; an eine Seitenflaeche: Wandschild
            if (target_.normal.y > 0) id = makeSign(false, lookDir ^ 1);
            else if (target_.normal.y == 0) id = makeSign(true, target_.normal.x > 0 ? 0 : (target_.normal.x < 0 ? 1 : (target_.normal.z > 0 ? 2 : 3)));
            else return;
        } else if (isWedge(id)) {
            id = makeWedge(wedgeMaterial(id), lookDir);  // hohe Seite in Blickrichtung
        } else if (isSlab(id)) {
            const int mat = slabMaterial(id);
            const BlockId tb2 = world_->getBlock(target_.block.x, target_.block.y, target_.block.z);
            // Gleiche Platte von der offenen Seite: ergibt einen vollen Block
            if (isSlab(tb2) && slabMaterial(tb2) == mat &&
                ((target_.normal.y > 0 && !slabIsTop(tb2)) || (target_.normal.y < 0 && slabIsTop(tb2)))) {
                world_->setBlock(target_.block.x, target_.block.y, target_.block.z, stairMaterialBlock(mat));
                placeTimer_ = 0.2f;
                playSound(blockSound(blockMaterial(id)), glm::vec3(target_.block) + glm::vec3(0.5f), 0.8f, 0.85f);
                if (!creative_) inventory_.take(slot_, 1);
                return;
            }
            bool top;
            if (target_.normal.y > 0) top = false;
            else if (target_.normal.y < 0) top = true;
            else top = hitPoint.y - static_cast<float>(p.y) > 0.5f;
            id = makeSlab(mat, top);
        } else if (isGate(id)) {
            id = makeGate(gateMaterial(id), false, std::fabs(f.x) >= std::fabs(f.z) ? 1 : 0);
        } else if (id == LANTERN) {
            const BlockId bBelow = world_->getBlock(p.x, p.y - 1, p.z), bAbove = world_->getBlock(p.x, p.y + 1, p.z);
            const bool canFloor = blockInfo(bBelow).solid && !isLantern(bBelow) && blockInfo(bBelow).shape != SH_DOOR;
            const bool canHang = blockInfo(bAbove).solid && !isLantern(bAbove);
            if (target_.normal.y < 0 && canHang) id = LANTERN_HANG;
            else if (canFloor) id = LANTERN;
            else if (canHang) id = LANTERN_HANG;
            else return;
        } else if (isLadder(id)) {
            // Seite des Traegers: bei Seitenflaechen der getroffene Block, sonst in Blickrichtung oder rundherum
            static const int DIRV[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            auto supported = [&](int fdir) {
                const BlockId sb = world_->getBlock(p.x + DIRV[fdir][0], p.y, p.z + DIRV[fdir][1]);
                return blockInfo(sb).solid && blockInfo(sb).shape != SH_LADDER;
            };
            int fdir = -1;
            if (target_.normal.y == 0) {
                for (int d = 0; d < 4; ++d)
                    if (DIRV[d][0] == -target_.normal.x && DIRV[d][1] == -target_.normal.z) fdir = d;
            } else if (supported(lookDir)) {
                fdir = lookDir;
            } else {
                for (int d = 0; d < 4 && fdir < 0; ++d)
                    if (supported(d)) fdir = d;
            }
            if (fdir < 0 || !supported(fdir)) return;
            id = makeLadder(fdir);
        } else if (id == BED) {
            static const int DIRV[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            const glm::ivec3 hp(p.x + DIRV[lookDir][0], p.y, p.z + DIRV[lookDir][1]);
            auto freeCell = [&](const glm::ivec3& c) {
                const BlockId cb = world_->getBlock(c.x, c.y, c.z);
                return (cb == AIR || isPlant(cb)) && blockInfo(world_->getBlock(c.x, c.y - 1, c.z)).solid;
            };
            if (!freeCell(p) || !freeCell(hp)) return;
            Box bb[1] = {{0, 0, 0, 1, 9.0f / 16.0f, 1}};
            if (player_.intersectsBoxes(p.x, p.y, p.z, bb, 1) || player_.intersectsBoxes(hp.x, hp.y, hp.z, bb, 1)) return;
            world_->setBlock(p.x, p.y, p.z, makeBedPart(false, lookDir));
            world_->setBlock(hp.x, hp.y, hp.z, makeBedPart(true, lookDir));
            placeTimer_ = 0.25f;
            playSound(SoundId::BlockGrass, glm::vec3(p) + glm::vec3(0.5f), 0.8f, 0.85f);
            if (!creative_) inventory_.take(slot_, 1);
            return;
        }
    }
    {
        Box boxes[MAX_BOXES];
        auto get = [this](int gx, int gy, int gz) { return world_->getBlock(gx, gy, gz); };
        const int nb = blockBoxes(id, p.x, p.y, p.z, get, false, boxes);
        if (nb > 0 && player_.intersectsBoxes(p.x, p.y, p.z, boxes, nb)) return;
    }
    if (world_->setBlock(p.x, p.y, p.z, id)) {
        placeTimer_ = 0.2f;
        playSound(blockSound(blockMaterial(id)), glm::vec3(p.x + 0.5f, p.y + 0.5f, p.z + 0.5f), 0.8f, 0.85f);
        if (!creative_) inventory_.take(slot_, 1);
        if (isSign(id)) openSign(p);  // gleich beschriften
    }
}

// Eimer: leer nimmt eine Wasser- oder Lavaquelle auf, gefuellt setzt eine Quelle an die angeklickte Stelle
void Game::useBucket() {
    ItemStack& sel = inventory_.slots[slot_];
    const RayHit h = world_->raycast(player_.eyePos(), player_.front(), REACH, true);
    if (!h.hit) return;
    ItemId result = ITEM_NONE;
    glm::vec3 fx(0.0f);
    if (sel.id == ITEM_BUCKET) {
        const BlockId b = world_->getBlock(h.block.x, h.block.y, h.block.z);
        if (b != WATER && b != LAVA) return;  // nur Quellen, fliessendes Wasser bleibt
        world_->setBlock(h.block.x, h.block.y, h.block.z, AIR);
        result = b == WATER ? ITEM_BUCKET_WATER : ITEM_BUCKET_LAVA;
        fx = glm::vec3(h.block) + glm::vec3(0.5f);
        playSound(b == WATER ? SoundId::WaterSplash : SoundId::LavaPop, fx, 0.7f, 1.2f);
    } else {
        // Zelle: der getroffene Block selbst, wenn man ihn ersetzen darf (Fluessigkeit, Pflanze), sonst davor
        const BlockId hb = world_->getBlock(h.block.x, h.block.y, h.block.z);
        glm::ivec3 p = h.block;
        if (!(isLiquid(hb) || isPlant(hb))) p = h.block + h.normal;
        const BlockId cur = world_->getBlock(p.x, p.y, p.z);
        if (p.y < 1 || p.y >= CHUNK_H || !(cur == AIR || isLiquid(cur) || isPlant(cur))) return;
        const BlockId put = sel.id == ITEM_BUCKET_WATER ? WATER : LAVA;
        if (cur == put) return;  // dort ist schon eine Quelle
        world_->setBlock(p.x, p.y, p.z, put);
        result = ITEM_BUCKET;
        fx = glm::vec3(p) + glm::vec3(0.5f);
        playSound(put == WATER ? SoundId::WaterSplash : SoundId::LavaPop, fx, 0.8f, 0.9f);
        if (settings_.particles && put == WATER) particles_.emitSplash(fx, 10, 2.0f);
    }
    placeTimer_ = 0.3f;
    if (sel.count <= 1) {
        sel = {result, 1, 0};
    } else {
        sel.count -= 1;
        const int left = inventory_.add(result, 1);
        if (left > 0) entities_.spawnItem({result, left}, player_.eyePos() + player_.front() * 0.5f, player_.front() * 2.0f, 0.3f);
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

    // Rechte Maustaste: handeln, setzen / essen
    if (right && placeTimer_ <= 0.0f) {
        if (mobFirst && mob->type == MobType::Villager && !mob->dead()) {
            placeTimer_ = 0.4f;
            openTrade(mob->profession);
        } else {
            placeBlockOrUse();
        }
    }

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
    if (settings_.particles) {
        const glm::mat4 vm = f.view;
        const glm::vec3 right(vm[0][0], vm[1][0], vm[2][0]), up(vm[0][1], vm[1][1], vm[2][1]);
        particles_.build(f.camPos, right, up, particleVerts_);
        f.particles = &particleVerts_;
    }
    f.shadows = settings_.shadows;
    f.clouds = settings_.clouds;
    f.texFilter = settings_.texFilter;
    f.anisotropy = settings_.anisotropy;
    f.moonPhase = opt_.moonPhase >= 0 ? opt_.moonPhase : dayCount_ % MOON_PHASES;
    f.time = static_cast<float>(glfwGetTime());
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
        for (int sl = 0; sl < 4; ++sl) {  // getragene Ruestung ueber dem Koerper
            const ItemStack& as = inventory_.armor[static_cast<size_t>(sl)];
            if (as.empty() || !isArmorItem(as.id)) continue;
            const SkinId askin = armorSkinFor(armorMaterialOf(as.id));
            appendArmor(askin, sl, pose, hurt ? entityBatches_.skinHurt[askin] : entityBatches_.skin[askin]);
        }
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
    h.armorPoints = inventory_.armorPoints();
    h.armorSlots = inventory_.armor;
    if (inventoryOpen_ && signWindow_) {
        h.signOpen = true;
        for (size_t i = 0; i < 4; ++i) h.signLines[i] = signText_.lines[i];
        h.signLine = signLine_;
    } else if (target_.hit && !inventoryOpen_ && !dead_ &&
               (isMachineBlock(world_->getBlock(target_.block.x, target_.block.y, target_.block.z)) || isSteamPart(world_->getBlock(target_.block.x, target_.block.y, target_.block.z)) ||
                isMechPart(world_->getBlock(target_.block.x, target_.block.y, target_.block.z)))) {
        h.infoLines = machineInfo(*world_, target_.block);
        h.infoShow = true;
    } else if (target_.hit && !inventoryOpen_ && !dead_ && isSign(world_->getBlock(target_.block.x, target_.block.y, target_.block.z))) {
        if (const SignText* t = world_->findSign(target_.block.x, target_.block.y, target_.block.z)) {
            h.signShow = true;
            for (size_t i = 0; i < 4; ++i) h.signShowLines[i] = t->lines[i];
        }
    }
    if (inventoryOpen_ && InvLayout::make(fbW_, fbH_, paletteWindow_, craftSize_, furnaceWindow_, chestWindow_, 0).figure.z > 0.0f) {
        if (!skinPreviewEd_) {
            skinPreviewEd_ = std::make_unique<TextureEditor>(textures_);
            for (size_t i = 0; i < skinPreviewEd_->entries().size(); ++i)
                if (skinPreviewEd_->entries()[i].kind == EditorEntry::Skin && skinPreviewEd_->entries()[i].skin == SKIN_PLAYER)
                    skinPreviewEd_->select(static_cast<int>(i));
        }
        h.invPreview = skinPreviewEd_->preview(static_cast<float>(glfwGetTime()) * 0.7f, 0.12f);
    }
    h.paletteOpen = paletteWindow_;
    h.creativeTab = creativeTab_;
    h.creativeScroll = creativeScroll_;
    h.creativeQuery = creativeQuery_;
    h.paletteItems = &paletteItems();
    h.furnaceOpen = furnaceWindow_;
    h.chestOpen = chestWindow_;
    h.tradeOpen = tradeWindow_;
    if (tradeWindow_) {
        h.windowTitle = std::string("Dorfbewohner: ") + professionName(tradeProfession_);
        for (const Trade& t : tradesFor(tradeProfession_)) {
            HudState::TradeRowHud r;
            r.cost = t.cost;
            r.result = t.result;
            r.affordable = creative_ || inventory_.count(t.cost.id) >= t.cost.count;
            h.trades.push_back(r);
        }
    }
    h.chest = chestWindow_ ? world_->findChest(chestPos_.x, chestPos_.y, chestPos_.z) : nullptr;
    if (bookVisible()) {
        const std::vector<BookItem> list = buildBook();
        clampBookScroll(static_cast<int>(list.size()));
        h.book.open = true;
        h.book.scroll = bookScroll_;
        h.book.onlyCraftable = bookOnlyCraftable_;
        for (const BookItem& it : list) h.book.entries.push_back(it.hud);
    }
    if (furnaceWindow_ && machineWindow_) {
        if (Machine* mach = world_->findMachine(machinePos_.x, machinePos_.y, machinePos_.z)) {
            h.machineMode = mach->kind == MachineKind::Boiler ? 1 : 2;
            h.windowTitle = mach->kind == MachineKind::Boiler ? "Dampfkessel" : "Destillierapparat";
            if (const ItemStack* s0 = machineSlot(*mach, 0)) h.furnaceIn = *s0;
            if (const ItemStack* s1 = machineSlot(*mach, 1)) h.furnaceFuel = *s1;
            if (const ItemStack* s2 = machineSlot(*mach, 2)) h.furnaceOut = *s2;
            h.furnaceBurn = mach->burnTotal > 0.0f ? mach->burnTime / mach->burnTotal : 0.0f;
            h.furnaceCook = mach->progress;
            h.machineWater = mach->water / BOILER_WATER_MAX;
            h.machinePurity = mach->purity;
            h.machineSteam = mach->steam;
            h.machineLoad = mach->load;
        }
    } else if (furnaceWindow_) {
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
    if (opt_.noSave || opt_.savePath.empty()) return true;
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
    world_->exportChests(sf.chests);
    world_->exportSigns(sf.signs);
    world_->exportMachines(sf.machines);
    sf.player.hasSpawn = true;
    sf.player.spawn = spawnPoint_;
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
        if (paused_ && (opt_.benchFrames > 0 || shotMode) && opt_.demo != "menu" && opt_.demo != "mainmenu" && opt_.demo != "settings" && opt_.demo.rfind("editor", 0) != 0) setPaused(false);
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

    if (!shotMode && !(paused_ && screen_ == MenuScreen::Main)) saveWorld();  // im Hauptmenue ist schon gespeichert
    return exitCode;
}

}  // namespace oc
