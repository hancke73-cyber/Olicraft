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
                else if (b == FURNACE_LIT) list.push_back({p, 2});
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
        if (isDoor(tb) || isGate(tb)) {
            if (world_->toggleOpenGroup(target_.block.x, target_.block.y, target_.block.z) > 0) {
                placeTimer_ = 0.25f;
                playSound(SoundId::BlockWood, glm::vec3(target_.block) + glm::vec3(0.5f), 0.6f, 0.75f);
            }
            return;
        }
    }
    if (sel.empty()) return;
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
    } else if (opt_.demo == "parts") {
        // Flache Buehne mit allen neuen Bauteilen (Blick entlang +x)
        const int ox = static_cast<int>(std::floor(player_.pos.x)) + 4, oz = static_cast<int>(std::floor(player_.pos.z)) - 12;
        const int gy = static_cast<int>(groundAt(player_.pos.x, player_.pos.z)) - 1;
        for (int z = 0; z <= 26; ++z)
            for (int x = -2; x <= 16; ++x) {
                for (int y = gy + 1; y <= gy + 8; ++y) world_->setBlock(ox + x, y, oz + z, AIR);
                world_->setBlock(ox + x, gy, oz + z, GRASS);
            }
        auto put = [&](int x, int y, int z, BlockId b) { world_->setBlock(ox + x, gy + 1 + y, oz + z, b); };
        // Reihe 1: Treppen und Platten (z = 0..)
        for (int m = 0; m < STAIR_MATERIALS; ++m) {
            put(0, 0, m * 2, makeStairs(m, 0));
            put(1, 0, m * 2, makeStairs(m, 2));
            put(3, 0, m * 2, makeSlab(m, false));
            put(4, 0, m * 2, makeSlab(m, true));
            put(4, -1 + 0, m * 2, stairMaterialBlock(m));
        }
        // Zaun mit Tor, Mauer mit grossem Tor
        for (int x = 0; x <= 8; ++x) put(x, 0, 12, FENCE);
        put(4, 0, 12, makeGate(0, false, 0));
        put(5, 0, 12, makeGate(0, true, 0));
        for (int x = 0; x <= 12; ++x) {
            put(x, 0, 15, WALL_COBBLE);
            put(x, 1, 15, x % 4 == 0 ? WALL_BRICKS : AIR);
        }
        for (int x = 4; x <= 7; ++x)
            for (int y = 0; y < 3; ++y) put(x, y, 15, makeGate(0, false, 0));
        // Haus mit Tuer, Fenster, Laterne
        for (int z = 18; z <= 22; ++z)
            for (int x = 0; x <= 4; ++x)
                for (int y = 0; y < 3; ++y) {
                    const bool edge = x == 0 || x == 4 || z == 18 || z == 22;
                    if (edge) put(x, y, z, PLANKS);
                }
        put(2, 0, 18, makeDoor(0, false, false, 3));
        put(2, 1, 18, makeDoor(0, true, false, 3));
        put(4, 1, 20, GLASS_PANE);
        put(0, 1, 20, GLASS_PANE);
        put(0, 1, 19, GLASS_PANE);
        put(0, 1, 21, GLASS_PANE);
        put(4, 0, 21, IRON_BARS);
        for (int y = 0; y < 4; ++y) {
            put(5, y, 23, STONE);
            put(4, y, 23, makeLadder(0));
        }
        put(10, 0, 22, makeBedPart(false, 2));
        put(10, 0, 23, makeBedPart(true, 2));
        put(4, 1, 21, IRON_BARS);
        put(2, 0, 16, LANTERN);
        put(6, 3, 20, STONE);
        put(6, 2, 20, LANTERN_HANG);
        // zwei offene und geschlossene Tueren nebeneinander
        put(8, 0, 18, makeDoor(1, false, true, 0));
        put(8, 1, 18, makeDoor(1, true, true, 0));
        put(10, 0, 18, makeDoor(0, false, false, 0));
        put(10, 1, 18, makeDoor(0, true, false, 0));
        // Acker mit Wasser und Feldfruechten
        for (int z = 24; z <= 26; ++z)
            for (int x = 0; x <= 8; ++x) {
                world_->setBlock(ox + x, gy, oz + z, z == 25 && x % 4 == 0 ? WATER : FARMLAND);
                if (z != 25) put(x, 0, z, x < 4 ? static_cast<BlockId>(WHEAT_0 + (x % 4)) : static_cast<BlockId>(CARROT_0 + (x % 4)));
            }
        // Walmdach aus Treppen: die Ecken ergeben sich aus den Nachbarn
        for (int k = 0; k < 3; ++k) {
            const int lo = 14 + k, hi = 20 - k, zlo = k, zhi = 6 - k;
            for (int x = lo; x <= hi; ++x)
                for (int z = zlo; z <= zhi; ++z) {
                    if (x != lo && x != hi && z != zlo && z != zhi) continue;
                    put(x, k, z, makeStairs(3, x == lo ? 0 : (x == hi ? 1 : (z == zlo ? 2 : 3))));
                }
        }
        put(17, 3, 3, BRICKS);
        // glatter Giebel aus Dachkeilen (zwei Reihen mit First in der Mitte) und eine Rampe
        for (int x = 30; x <= 36; ++x) {
            put(x, 0, 0, makeWedge(3, 3));  // hohe Seiten zeigen nach aussen, in der Mitte laeuft der First
            put(x, 0, 1, makeWedge(3, 2));
        }
        for (int k = 0; k < 3; ++k) put(30 + k, 0, 4, makeWedge(1, 0));
        // Schilder: Stehschild mit Text und ein Wandschild
        put(40, 0, 0, makeSign(false, 3));
        put(43, 0, 0, STONE);
        put(43, 1, 0, STONE);
        put(42, 1, 0, makeSign(true, 1));
        {
            SignText t;
            t.lines = {"Willkommen", "im Dorf", "Haus 3 links", ""};
            world_->setSign(ox + 40, gy + 1, oz + 0, t);
            SignText t2;
            t2.lines = {"Schmiede", "", "", ""};
            world_->setSign(ox + 42, gy + 2, oz + 0, t2);
        }
        // Bogen mit umgedrehten Treppen unter der Decke
        for (int z = 0; z <= 2; ++z) {
            for (int y = 0; y <= 2; ++y) {
                put(24, y, z, STONE_BRICKS);
                put(28, y, z, STONE_BRICKS);
            }
            put(25, 2, z, makeStairs(2, 0, true));
            put(26, 2, z, makeSlab(2, true));
            put(27, 2, z, makeStairs(2, 1, true));
        }
        player_.pos = glm::vec3(static_cast<float>(ox + 5), static_cast<float>(gy + 11), static_cast<float>(oz - 3));
        player_.yaw = 90.0f;
        player_.pitch = -42.0f;
        player_.flying = true;
        creative_ = true;
        if (const char* cam = std::getenv("OLICRAFT_DEMO_CAM")) {  // Test: Kamera relativ zur Buehne "dx dy dz yaw pitch"
            float dx, dy, dz, yaw, pitch;
            if (std::sscanf(cam, "%f %f %f %f %f", &dx, &dy, &dz, &yaw, &pitch) == 5) {
                player_.pos = glm::vec3(ox + dx, gy + dy, oz + dz);
                player_.yaw = yaw;
                player_.pitch = pitch;
            }
        }
        if (std::getenv("OLICRAFT_SIGN")) openSign(glm::ivec3(ox + 40, gy + 1, oz));  // Test: Schildfenster
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
            for (int scroll = 0; scroll < 500; ++scroll) {
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

    // 20. Rezeptbuch, Truhe, Bett, Kaktus, Pflanzen
    {
        dead_ = false;
        creative_ = false;
        paused_ = false;
        health_ = 20;
        invulnerable_ = 0.0f;
        cursor_.clear();
        entities_.clear();
        inventory_.clear();
        for (int x = 6; x <= 14; ++x)
            for (int z = -12; z <= -2; ++z) {
                for (int y = gy; y <= gy + 4; ++y) world_->setBlock(x, y, z, AIR);
                world_->setBlock(x, gy - 1, z, x % 2 ? STONE : GRASS);
            }
        // Rezeptbuch: Truhe (8 Bretter) ist herstellbar, Ofen (8 Bruchstein) nicht
        inventory_.add(PLANKS, 8);
        openInventory(true);
        {
            const std::vector<BookItem> book = buildBook();
            SCHECK(!book.empty() && book[0].hud.craftable);
            const BookItem* chestItem = nullptr;
            const BookItem* furnaceItem = nullptr;
            for (const BookItem& it : book) {
                if (it.hud.result.id == CHEST) chestItem = &it;
                if (it.hud.result.id == FURNACE) furnaceItem = &it;
            }
            SCHECK(chestItem && chestItem->hud.craftable && furnaceItem && !furnaceItem->hud.craftable);
            if (chestItem) {
                autofillRecipe(*chestItem->recipe);
                SCHECK(matchRecipe(craft_, craftSize_).id == CHEST && inventory_.count(PLANKS) == 0);
                // Klick auf das Ergebnis, Raster leeren
                for (ItemStack& cs : craft_) cs.clear();
            }
            bookOnlyCraftable_ = true;
            inventory_.add(PLANKS, 8);
            const std::vector<BookItem> onlyOk = buildBook();
            bool allOk = !onlyOk.empty();
            for (const BookItem& it : onlyOk) allOk = allOk && it.hud.craftable;
            SCHECK(allOk && onlyOk.size() < book.size() + 8);
            bookOnlyCraftable_ = false;
            // im Inventar (2x2) passen Rezepte fuer 3x3 nicht
            closeInventory();
            openInventory(false);
            bool bigNotFit = false;
            for (const BookItem& it : buildBook())
                if (it.recipe->gridNeeded() == 3) bigNotFit = !it.hud.fits;
            SCHECK(bigNotFit);
            closeInventory();
        }
        // Truhe: setzen, oeffnen, befuellen, speichern
        inventory_.clear();
        world_->setBlock(9, gy, -9, CHEST);
        openChest(glm::ivec3(9, gy, -9));
        SCHECK(inventoryOpen_ && chestWindow_ && world_->findChest(9, gy, -9) != nullptr);
        world_->chestAt(9, gy, -9).add(ITEM_DIAMOND, 5);
        world_->chestAt(9, gy, -9).slots[26] = {ITEM_IRON_PICKAXE, 1, 7};
        closeInventory();
        SCHECK(!chestWindow_ && !inventoryOpen_);
        // Truhe abbauen: Inhalt faellt heraus
        world_->setBlock(11, gy, -9, CHEST);
        world_->chestAt(11, gy, -9).add(COAL_BLOCK, 3);
        breakBlock(glm::ivec3(11, gy, -9));
        SCHECK(world_->getBlock(11, gy, -9) == AIR && world_->findChest(11, gy, -9) == nullptr);
        SCHECK(!entities_.items().empty());
        entities_.clear();
        // Bett: tagsueber nur Wiedereinstiegspunkt, nachts schlafen
        world_->setBlock(7, gy, -5, BED);
        timeOfDay_ = 0.25f;
        useBed(glm::ivec3(7, gy, -5));
        SCHECK(std::fabs(timeOfDay_ - 0.25f) < 1e-4f && spawnPoint_ == glm::vec3(7.5f, gy + 1.0f, -4.5f));
        timeOfDay_ = 0.80f;
        const int dayBefore = dayCount_;
        health_ = 10;
        useBed(glm::ivec3(7, gy, -5));
        SCHECK(timeOfDay_ < 0.1f && dayCount_ == dayBefore + 1 && health_ == 20);
        // Monster in der Naehe verhindern das Schlafen
        timeOfDay_ = 0.80f;
        entities_.spawnMob(MobType::Zombie, glm::vec3(8.5f, static_cast<float>(gy), -5.5f));
        useBed(glm::ivec3(7, gy, -5));
        SCHECK(std::fabs(timeOfDay_ - 0.80f) < 1e-4f);
        entities_.clear();
        // Kaktus verletzt bei Beruehrung
        world_->setBlock(12, gy, -3, CACTUS);
        player_.pos = glm::vec3(11.6f, static_cast<float>(gy), -2.5f);
        player_.vel = glm::vec3(0.0f);
        health_ = 20;
        invulnerable_ = 0.0f;
        for (int i = 0; i < 4; ++i) updateSurvival(0.2f);
        SCHECK(health_ < 20);
        world_->setBlock(12, gy, -3, AIR);
        // Pflanzen: Blume auf Gras setzen, auf Stein nicht; Gras wird ersetzt
        inventory_.clear();
        inventory_.slots[0] = {FLOWER_RED, 5};
        slot_ = 0;
        placeTimer_ = 0.0f;
        target_ = world_->raycast(glm::vec3(8.5f, gy + 1.5f, -8.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);  // x = 8: Gras
        SCHECK(target_.hit && world_->getBlock(8, gy - 1, -9) == GRASS);
        placeBlockOrUse();
        SCHECK(world_->getBlock(8, gy, -9) == FLOWER_RED && inventory_.slots[0].count == 4);
        placeTimer_ = 0.0f;
        target_ = world_->raycast(glm::vec3(9.5f, gy + 1.5f, -7.5f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f);  // x = 9: Stein
        placeBlockOrUse();
        SCHECK(world_->getBlock(9, gy - 1, -8) == STONE && world_->getBlock(9, gy, -8) == AIR && inventory_.slots[0].count == 4);
        // Beute: Blume abbauen, danach liegt sie als Gegenstand da
        entities_.clear();
        breakBlock(glm::ivec3(8, gy, -9));
        SCHECK(world_->getBlock(8, gy, -9) == AIR && !entities_.items().empty() && entities_.items()[0].stack.id == FLOWER_RED);
        entities_.clear();
        inventory_.clear();
    }

    // 21. Handel mit Dorfbewohnern
    {
        paused_ = false;
        creative_ = false;
        cursor_.clear();
        inventory_.clear();
        entities_.clear();
        inventory_.add(ITEM_GOLD_INGOT, 4);
        inventory_.add(ITEM_COAL, 40);
        openTrade(PROF_SMITH);
        SCHECK(inventoryOpen_ && tradeWindow_ && !bookVisible());
        const std::vector<Trade>& tr = tradesFor(PROF_SMITH);
        SCHECK(tr.size() >= 4 && tr[0].cost.id == ITEM_COAL && tr[2].result.id == ITEM_IRON_PICKAXE);
        SCHECK(invLayout().tradeRows == static_cast<int>(tr.size()));
        // 15 Kohle -> 1 Goldbarren
        SCHECK(buyTrade(0) && inventory_.count(ITEM_COAL) == 25 && inventory_.count(ITEM_GOLD_INGOT) == 5);
        SCHECK(buyTrade(2) && inventory_.count(ITEM_GOLD_INGOT) == 2 && inventory_.count(ITEM_IRON_PICKAXE) == 1);
        SCHECK(!buyTrade(2) && inventory_.count(ITEM_GOLD_INGOT) == 2 && inventory_.count(ITEM_IRON_PICKAXE) == 1);  // zu wenig Gold
        SCHECK(!buyTrade(99) && !buyTrade(-1));
        // Voller Rucksack: Handel schlaegt fehl, nichts geht verloren
        inventory_.add(ITEM_GOLD_INGOT, 3);
        for (int i = 0; i < Inventory::SIZE; ++i)
            if (inventory_.slots[static_cast<size_t>(i)].empty()) inventory_.slots[static_cast<size_t>(i)] = {LOG, 64};
        const int goldBefore = inventory_.count(ITEM_GOLD_INGOT);
        SCHECK(!buyTrade(3) && inventory_.count(ITEM_GOLD_INGOT) == goldBefore);
        inventory_.clear();
        // Kreativ: kostenlos
        creative_ = true;
        SCHECK(buyTrade(2) && inventory_.count(ITEM_IRON_PICKAXE) == 1 && inventory_.count(ITEM_GOLD_INGOT) == 0);
        creative_ = false;
        closeInventory();
        SCHECK(!tradeWindow_ && !inventoryOpen_);
        inventory_.clear();
    }

    // 22. Bauteile setzen und benutzen: Treppe, Platte, Tor, Tuer, Laterne, Hacke, Samen
    {
        paused_ = false;
        creative_ = false;
        cursor_.clear();
        inventory_.clear();
        entities_.clear();
        for (int x = 6; x <= 16; ++x)
            for (int z = -14; z <= -2; ++z) {
                for (int y = gy; y <= gy + 6; ++y) world_->setBlock(x, y, z, AIR);
                world_->setBlock(x, gy - 1, z, STONE);
            }
        player_.pos = glm::vec3(5.5f, static_cast<float>(gy), -8.5f);
        player_.vel = glm::vec3(0.0f);
        player_.yaw = 0.0f;  // Blick nach +x
        player_.pitch = 0.0f;
        slot_ = 0;
        auto aim = [&](float x, float y, float z, float dx, float dy, float dz) {
            target_ = world_->raycast(glm::vec3(x, y, z), glm::vec3(dx, dy, dz), 8.0f);
            player_.yaw = 0.0f;  // Blickrichtung +x fuer die Ausrichtung; der Spieler steht abseits
            player_.pitch = 0.0f;
        };
        // Treppe auf den Boden: hohe Seite in Blickrichtung (+x)
        inventory_.slots[0] = {makeStairs(0, 0), 4};
        aim(8.5f, gy + 2.0f, -12.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(8, gy, -13) == makeStairs(0, 0) && inventory_.slots[0].count == 3);
        // Treppe von unten an die Decke gesetzt oder an die obere Haelfte einer Wand: umgedreht
        world_->setBlock(6, gy + 3, -13, STONE);
        inventory_.slots[0] = {makeStairs(0, 0), 4};
        aim(6.5f, gy, -12.5f, 0.0f, 1.0f, 0.0f);  // von unten gegen die Decke
        SCHECK(target_.hit && target_.normal == glm::ivec3(0, -1, 0));
        placeBlockOrUse();
        SCHECK(world_->getBlock(6, gy + 2, -13) == makeStairs(0, 0, true) && inventory_.slots[0].count == 3);
        // Vorn davon eine Treppe in Querrichtung: die erste zeigt dann eine Ecke (aus den Nachbarn abgeleitet)
        world_->setBlock(7, gy + 2, -13, makeStairs(0, 2, true));
        {
            Box cb[MAX_BOXES];
            SCHECK(world_->collisionBoxes(6, gy + 2, -13, cb) == 2);
            SCHECK(cb[1].x0 == 0.5f && cb[1].z0 == 0.5f && cb[1].y1 == 0.5f);
        }
        world_->setBlock(7, gy + 2, -13, AIR);
        world_->setBlock(6, gy + 2, -13, AIR);
        world_->setBlock(6, gy + 3, -13, AIR);
        // Dachkeil auf den Boden: hohe Seite in Blickrichtung (+x)
        inventory_.slots[0] = {makeWedge(2, 0), 4};
        aim(7.5f, gy + 2.0f, -12.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(7, gy, -13) == makeWedge(2, 0) && inventory_.slots[0].count == 3);
        world_->setBlock(7, gy, -13, AIR);
        // Platte: von oben unten, von unten oben, zweite Platte ergibt einen Vollblock
        inventory_.slots[0] = {makeSlab(1, false), 6};
        aim(10.5f, gy + 2.0f, -12.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(10, gy, -13) == makeSlab(1, false));
        aim(10.5f, gy + 2.0f, -12.5f, 0.0f, -1.0f, 0.0f);  // trifft die obere Plattenflaeche
        SCHECK(target_.hit && target_.block == glm::ivec3(10, gy, -13) && target_.normal == glm::ivec3(0, 1, 0));
        placeBlockOrUse();
        SCHECK(world_->getBlock(10, gy, -13) == COBBLE && inventory_.slots[0].count == 4);
        world_->setBlock(12, gy + 3, -12, STONE);
        aim(12.5f, gy + 1.0f, -11.5f, 0.0f, 1.0f, 0.0f);
        SCHECK(target_.hit && target_.normal == glm::ivec3(0, -1, 0));
        placeBlockOrUse();
        SCHECK(isSlab(world_->getBlock(12, gy + 2, -12)) && slabIsTop(world_->getBlock(12, gy + 2, -12)));
        world_->setBlock(12, gy + 3, -12, AIR);
        world_->setBlock(12, gy + 2, -12, AIR);
        // Tor: Blick +x, das Tor spannt sich quer dazu (Achse 1)
        inventory_.slots[0] = {makeGate(0, false, 0), 3};
        aim(8.5f, gy + 2.0f, -9.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(isGate(world_->getBlock(8, gy, -10)) && gateAxis(world_->getBlock(8, gy, -10)) == 1 && !gateOpen(world_->getBlock(8, gy, -10)));
        // Tuer (zwei Bloecke)
        inventory_.slots[0] = {ITEM_DOOR_WOOD, 2};
        aim(11.5f, gy + 2.0f, -8.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(isDoor(world_->getBlock(11, gy, -9)) && isDoor(world_->getBlock(11, gy + 1, -9)) && doorUpper(world_->getBlock(11, gy + 1, -9)) &&
               doorFacing(world_->getBlock(11, gy, -9)) == 0 && inventory_.slots[0].count == 1);
        // Rechtsklick auf die Tuer oeffnet sie (Strahl von der Seite trifft die Platte bei x = 11.81)
        inventory_.slots[0].clear();
        aim(9.5f, gy + 0.5f, -8.5f, 1.0f, 0.0f, 0.0f);
        SCHECK(target_.hit && target_.block == glm::ivec3(11, gy, -9));
        placeBlockOrUse();
        SCHECK(doorOpen(world_->getBlock(11, gy, -9)) && doorOpen(world_->getBlock(11, gy + 1, -9)));
        // Laterne auf dem Boden und unter der Decke
        inventory_.slots[0] = {LANTERN, 3};
        aim(13.5f, gy + 2.0f, -9.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(13, gy, -10) == LANTERN && world_->lightAt(13, gy, -10) == 15);
        world_->setBlock(14, gy + 3, -9, STONE);
        aim(14.5f, gy + 1.0f, -8.5f, 0.0f, 1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(14, gy + 2, -9) == LANTERN_HANG);
        world_->setBlock(14, gy + 3, -9, AIR);  // Halterung weg: die Laterne faellt ab
        world_->setBlock(14, gy + 2, -9, AIR);
        world_->takePlantDrops();
        // Hacke macht aus Gras Acker, Samen werden darauf gepflanzt, Brot essen heilt
        world_->setBlock(15, gy - 1, -5, GRASS);
        inventory_.slots[0] = {ITEM_HOE_IRON, 1};
        aim(15.5f, gy + 2.0f, -4.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(15, gy - 1, -5) == FARMLAND && inventory_.slots[0].damage == 1);
        inventory_.slots[0] = {ITEM_SEEDS, 2};
        aim(15.5f, gy + 2.0f, -4.5f, 0.0f, -1.0f, 0.0f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(15, gy, -5) == WHEAT_0 && inventory_.slots[0].count == 1);
        inventory_.slots[0] = {ITEM_BREAD, 1};
        health_ = 10;
        eatTimer_ = 0.0f;
        target_ = RayHit{};
        placeBlockOrUse();
        SCHECK(health_ == 15 && inventory_.slots[0].empty());
        // Reifen Weizen abbauen: Weizen und Samen als Beute
        world_->setBlock(15, gy, -5, WHEAT_3);
        entities_.clear();
        breakBlock(glm::ivec3(15, gy, -5));
        bool gotWheat = false;
        for (const ItemEntity& it : entities_.items()) gotWheat = gotWheat || it.stack.id == ITEM_WHEAT;
        SCHECK(world_->getBlock(15, gy, -5) == AIR && gotWheat);
        // Tuer abbauen (obere Haelfte): beide verschwinden, eine Tuer faellt als Gegenstand
        entities_.clear();
        breakBlock(glm::ivec3(11, gy + 1, -9));
        SCHECK(world_->getBlock(11, gy, -9) == AIR && world_->getBlock(11, gy + 1, -9) == AIR);
        bool gotDoor = false;
        for (const ItemEntity& it : entities_.items()) gotDoor = gotDoor || it.stack.id == ITEM_DOOR_WOOD;
        SCHECK(gotDoor);
        entities_.clear();
        inventory_.clear();
        player_.pitch = 0.0f;
        player_.yaw = -90.0f;
    }

    // 23. Ruestung: anlegen per Rechtsklick, tauschen, Schaden mindern, abnutzen
    {
        paused_ = false;
        creative_ = false;
        dead_ = false;
        cursor_.clear();
        inventory_.clear();
        health_ = 20;
        invulnerable_ = 0.0f;
        target_ = RayHit{};
        slot_ = 0;
        inventory_.slots[0] = {makeArmorItem(3, 1), 1, 0};
        placeBlockOrUse();
        SCHECK(inventory_.armor[1].id == makeArmorItem(3, 1) && inventory_.slots[0].empty() && inventory_.armorPoints() == 8);
        inventory_.slots[0] = {makeArmorItem(1, 1), 1, 5};  // anderer Brustpanzer: wird getauscht
        placeBlockOrUse();
        SCHECK(inventory_.armor[1].id == makeArmorItem(1, 1) && inventory_.armor[1].damage == 5 && inventory_.slots[0].id == makeArmorItem(3, 1));
        for (int sl = 0; sl < 4; ++sl) inventory_.armor[static_cast<size_t>(sl)] = {makeArmorItem(3, sl), 1, 0};
        SCHECK(inventory_.armorPoints() == 20);
        damagePlayer(10, glm::vec3(2.0f, 1.0f, 0.0f));  // Monstertreffer: 10 * (1 - 0.8) = 2
        SCHECK(health_ == 18 && inventory_.armor[0].damage == 1 && inventory_.armor[3].damage == 1);
        invulnerable_ = 0.0f;
        damagePlayer(3, glm::vec3(0.0f));  // Lava, Ertrinken usw. ignorieren die Ruestung
        SCHECK(health_ == 15);
        inventory_.armor[2] = {makeArmorItem(3, 2), 1, itemInfo(makeArmorItem(3, 2)).maxDurability - 1};
        invulnerable_ = 0.0f;
        damagePlayer(1, glm::vec3(1.0f, 1.0f, 0.0f));
        SCHECK(inventory_.armor[2].empty());  // zerbrochen
        inventory_.clear();
        health_ = 20;
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
    SCHECK(sf.chests.size() == 1 && sf.chests[0].x == 9 && sf.chests[0].chest.slots[0].id == ITEM_DIAMOND && sf.chests[0].chest.slots[0].count == 5);
    SCHECK(sf.chests[0].chest.slots[26].id == ITEM_IRON_PICKAXE && sf.chests[0].chest.slots[26].damage == 7);
    SCHECK(sf.player.hasSpawn && sf.player.spawn == spawnPoint_);
    std::filesystem::remove_all("olicraft_selftest");

    // 24. Hauptmenue und Weltverwaltung: mehrere Welten, jede mit Namen und eigener Datei
    {
        auto clickId = [&](int id) {
            const MenuLayout menu = buildMenu(screen_, menuState(), fbW_, fbH_);
            for (size_t i = 0; i < menu.widgets.size(); ++i)
                if (menu.widgets[i].id == id) {
                    activateWidget(static_cast<int>(i), menu, glm::vec2(0.0f));
                    return true;
                }
            return false;
        };
        auto typeText = [&](const std::string& t) {
            for (char c : t) {  // wie GLFW: erst die Taste, dann das Zeichen
                handleKey(std::toupper(static_cast<unsigned char>(c)), GLFW_PRESS, 0);
                handleChar(static_cast<unsigned char>(c));
            }
        };
        std::filesystem::remove_all("olicraft_selftest/saves");
        opt_.savesDir = "olicraft_selftest/saves";
        opt_.noSave = false;
        opt_.savePath.clear();
        currentWorld_.clear();
        showMainMenu();
        SCHECK(paused_ && screen_ == MenuScreen::Main && worlds_.empty() && worldSel_ == -1);
        SCHECK(!buildMenu(screen_, menuState(), fbW_, fbH_).widgets.empty());
        SCHECK(clickId(W_MAIN_PLAY) && paused_);  // ohne Welt tut "Spielen" nichts
        // Welt A: Name tippen (die Vorgabe wird beim ersten Zeichen ersetzt), Seed, Modus
        SCHECK(clickId(W_MAIN_NEW) && screen_ == MenuScreen::NewWorld && newName_ == "Neue Welt");
        const bool creativeBefore = creative_;
        typeText("Gtef Alpha");  // Buchstaben, die im Spiel Funktionen haben, landen nur im Namen
        SCHECK(newName_ == "gtef Alpha" || newName_ == "Gtef Alpha");
        SCHECK(!inventoryOpen_ && creative_ == creativeBefore);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        SCHECK(newName_.size() == 4);
        handleChar('?');  // ungueltige Zeichen werden verworfen
        handleChar(0x00e4);
        SCHECK(newName_.size() == 4);
        typeText("x");  // "Gtefx"
        SCHECK(clickId(W_NEW_SEED) && !newNameFocus_);
        handleChar('1'); handleChar('2'); handleChar('a'); handleChar('3'); handleChar('7');
        SCHECK(newSeedText_ == "1237");
        SCHECK(clickId(W_NEW_MODE) && newCreative_);
        const int oldSeed = world_->seed();
        SCHECK(clickId(W_NEW_CREATE) && !paused_ && screen_ == MenuScreen::None);
        SCHECK(world_->seed() == 1237 && world_->seed() != oldSeed && creative_ && inventory_.slots[0].id == DEFAULT_HOTBAR[0]);
        SCHECK(health_ == 20 && !dead_ && entities_.mobs().empty());
        SCHECK(currentWorld_ == "Gtefx" && opt_.savePath == "olicraft_selftest/saves/Gtefx.dat" && std::filesystem::exists(opt_.savePath));
        // in Welt A etwas bauen, zurueck ins Hauptmenue (speichert)
        const int ax = 5, ay = 110, az = 5;
        world_->loadAround(glm::vec3(ax + 0.5f, ay, az + 0.5f), 1);
        world_->setBlock(ax, ay, az, DIAMOND_BLOCK);
        setPaused(true);
        SCHECK(clickId(W_TO_MAIN) && screen_ == MenuScreen::Main);
        SCHECK(worlds_.size() == 1 && worlds_[0].name == "Gtefx" && worlds_[0].seed == 1237 && worlds_[0].creative && worldSel_ == 0);
        // Welt B mit gleichem Namen: bekommt " 2", Survival, zufaelliger Seed
        SCHECK(clickId(W_MAIN_NEW) && screen_ == MenuScreen::NewWorld);
        typeText("gtefx");
        SCHECK(clickId(W_NEW_CREATE) && !paused_);
        SCHECK(currentWorld_ == "gtefx" || currentWorld_ == "Gtefx 2" || currentWorld_ == "gtefx 2");
        SCHECK(!creative_ && std::filesystem::exists(opt_.savePath));
        SCHECK(world_->getBlock(ax, ay, az) != DIAMOND_BLOCK);
        setPaused(true);
        SCHECK(clickId(W_TO_MAIN) && worlds_.size() == 2);
        // Welt A wieder laden: der Block ist da, Modus Kreativ, Seed 1237
        int idxA = -1;
        for (size_t i = 0; i < worlds_.size(); ++i)
            if (worlds_[i].seed == 1237) idxA = static_cast<int>(i);
        SCHECK(idxA >= 0);
        selectWorld(idxA);
        SCHECK(clickId(W_MAIN_PLAY) && !paused_ && world_->seed() == 1237 && creative_);
        SCHECK(currentWorld_ == "Gtefx" && opt_.savePath == "olicraft_selftest/saves/Gtefx.dat");
        world_->loadAround(glm::vec3(ax + 0.5f, ay, az + 0.5f), 1);
        SCHECK(world_->getBlock(ax, ay, az) == DIAMOND_BLOCK);
        // Esc im Seed-Bildschirm geht zurueck, Einstellungen aus dem Hauptmenue fuehren auch dorthin zurueck
        setPaused(true);
        SCHECK(clickId(W_TO_MAIN));
        SCHECK(clickId(W_SETTINGS) && screen_ == MenuScreen::Settings);
        SCHECK(clickId(W_BACK) && screen_ == MenuScreen::Main);
        SCHECK(clickId(W_MAIN_NEW) && screen_ == MenuScreen::NewWorld);
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(screen_ == MenuScreen::Main);
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(screen_ == MenuScreen::Main && paused_);  // Esc tut im Hauptmenue nichts
        // Pfeiltasten waehlen, Enter startet die gewaehlte Welt (die laufende wird nur fortgesetzt)
        handleKey(GLFW_KEY_DOWN, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_UP, GLFW_PRESS, 0);
        SCHECK(worldSel_ >= 0 && worldSel_ < 2);
        for (size_t i = 0; i < worlds_.size(); ++i)
            if (worlds_[i].seed == 1237) idxA = static_cast<int>(i);  // die Reihenfolge wechselt mit dem Speichern
        selectWorld(idxA);
        handleKey(GLFW_KEY_ENTER, GLFW_PRESS, 0);
        SCHECK(!paused_ && world_->seed() == 1237);
        // Pausemenue -> Hauptmenue
        setPaused(true);
        SCHECK(clickId(W_TO_MAIN) && screen_ == MenuScreen::Main);
        setPaused(false);
        std::filesystem::remove_all("olicraft_selftest/saves");
        opt_.noSave = true;  // keine Dateien mehr anfassen
    }

    // 25. Kreativ-Inventar: Reiter nach Themen, Suche, Bildlauf, GUI-Groesse
    {
        setPaused(false);
        creative_ = true;
        if (inventoryOpen_) closeInventory();
        cursor_.clear();
        openInventory();
        SCHECK(inventoryOpen_ && paletteWindow_);
        auto clickAt = [&](glm::vec2 fbPos, int button, int mods) {
            const double sx = static_cast<double>(winW_cur_) / fbW_, sy = static_cast<double>(winH_cur_) / fbH_;
            curX_ = fbPos.x * sx;
            curY_ = fbPos.y * sy;
            inventoryClick(button, mods);
        };
        const glm::vec2 half = glm::vec2(invLayout().pitch() * 0.5f);
        // jeder Themenreiter: Klick waehlt ihn, das erste Feld liefert dessen ersten Gegenstand
        for (int t = 0; t < CREATIVE_TAB_COUNT; ++t) {
            clickAt(invLayout().tabPos(t) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
            SCHECK(creativeTab_ == t && creativeScroll_ == 0);
            cursor_.clear();
            clickAt(invLayout().palettePos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
            SCHECK(cursor_.id == creativeTabItems(t)[0] && cursor_.count == itemInfo(cursor_.id).maxStack);
            clickAt(invLayout().palettePos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);  // erneuter Klick gibt zurueck
            SCHECK(cursor_.empty());
        }
        // leeres Feld hinter dem letzten Eintrag tut nichts
        clickAt(invLayout().tabPos(4) + half, GLFW_MOUSE_BUTTON_LEFT, 0);  // Stationen: wenige Eintraege
        clickAt(invLayout().palettePos(InvLayout::PALETTE_PAGE - 1) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.empty());
        // Hotkeys wirken ausserhalb der Suche weiter: E schliesst das Inventar
        handleKey(GLFW_KEY_E, GLFW_PRESS, 0);
        SCHECK(!inventoryOpen_);
        openInventory();
        SCHECK(creativeTab_ == 4);  // der Reiter bleibt gemerkt
        // Suche
        clickAt(invLayout().tabPos(CREATIVE_SEARCH_TAB) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(creativeTab_ == CREATIVE_SEARCH_TAB && creativeQuery_.empty());
        SCHECK(static_cast<int>(paletteItems().size()) == creativeItemCount());
        // mit leerem Suchtext viele Seiten: Mausrad und Leiste scrollen
        SCHECK(creativeScroll_ == 0);
        handleScroll(-1.0);
        SCHECK(creativeScroll_ == 1);
        handleScroll(1.0);
        handleScroll(1.0);
        SCHECK(creativeScroll_ == 0);
        {
            const InvLayout L = invLayout();
            clickAt(glm::vec2(L.paletteTop.x + InvLayout::PALETTE_COLS * L.pitch() + 2.0f, L.scrollbarTop() + L.scrollbarHeight() - 1.0f), GLFW_MOUSE_BUTTON_LEFT, 0);
            const int rows = (creativeItemCount() + InvLayout::PALETTE_COLS - 1) / InvLayout::PALETTE_COLS;
            SCHECK(creativeScroll_ == rows - InvLayout::PALETTE_ROWS);
            for (int i = 0; i < 50; ++i) handleScroll(-1.0);
            SCHECK(creativeScroll_ == rows - InvLayout::PALETTE_ROWS);  // nicht ueber das Ende hinaus
        }
        // Tippen: Tasten wie E oder Zahlen schliessen nichts und waehlen keinen Hotbar-Platz
        const int slotBefore = slot_;
        for (char c : std::string("diamant")) {
            handleKey(std::toupper(static_cast<unsigned char>(c)), GLFW_PRESS, 0);
            handleChar(static_cast<unsigned char>(c));
        }
        handleKey(GLFW_KEY_E, GLFW_PRESS, 0);
        handleChar('e');
        handleKey(GLFW_KEY_5, GLFW_PRESS, 0);
        SCHECK(inventoryOpen_ && slot_ == slotBefore && creativeQuery_ == "diamante");
        SCHECK(creativeScroll_ == 0 && static_cast<int>(paletteItems().size()) < creativeItemCount());
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        SCHECK(creativeQuery_ == "diamant" && !paletteItems().empty());
        bool pick = false;
        for (ItemId id : paletteItems()) pick = pick || id == ITEM_DIAMOND_PICKAXE;
        SCHECK(pick);
        // Umlaut, Gross/Klein, Treffer anklicken
        handleChar(0xFC);
        SCHECK(creativeQuery_ == "diamantue" && paletteItems().empty());
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        for (int i = 0; i < 4; ++i) handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);  // "dia"
        handleChar('M');
        SCHECK(creativeQuery_ == "diaM" && !paletteItems().empty());
        const ItemId firstHit = paletteItems()[0];
        clickAt(invLayout().palettePos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.id == firstHit);
        cursor_.clear();
        for (int i = 0; i < 30; ++i) handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);
        SCHECK(creativeQuery_.empty());
        for (int i = 0; i < 40; ++i) handleChar('a');
        SCHECK(creativeQuery_.size() == 20);  // Laenge begrenzt
        // zurueck zu einem Themenreiter: Tasten wirken wieder
        clickAt(invLayout().tabPos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        handleKey(GLFW_KEY_E, GLFW_PRESS, 0);
        SCHECK(!inventoryOpen_);
        // Survival-Inventar hat keine Reiter
        creative_ = false;
        openInventory();
        SCHECK(!paletteWindow_ && invLayout().hitTab(invLayout().panelPos + glm::vec2(20.0f, 20.0f)) == -1);
        closeInventory();
        creative_ = true;
        // GUI-Groesse im Einstellungsmenue
        paused_ = true;
        screen_ = MenuScreen::Settings;
        auto clickMenu = [&](int id) {
            const MenuLayout menu = buildMenu(screen_, menuState(), fbW_, fbH_);
            for (size_t i = 0; i < menu.widgets.size(); ++i)
                if (menu.widgets[i].id == id) {
                    activateWidget(static_cast<int>(i), menu, glm::vec2(0.0f));
                    return true;
                }
            return false;
        };
        SCHECK(settings_.guiSize == 0);
        SCHECK(clickMenu(W_GUI_SIZE) && settings_.guiSize == 1 && guiSizeSetting() == 1);
        for (int i = 0; i < 4; ++i) clickMenu(W_GUI_SIZE);
        SCHECK(settings_.guiSize == 0 && guiSizeSetting() == 0);
        setPaused(false);
    }

    // 26. Ruestung anlegen: Kreativ-Inventar (Reihe unter dem Auswahlfeld) und Werkbank
    {
        setPaused(false);
        if (inventoryOpen_) closeInventory();
        cursor_.clear();
        for (ItemStack& a : inventory_.armor) a.clear();
        auto clickAt = [&](glm::vec2 fbPos, int button, int mods) {
            const double sx = static_cast<double>(winW_cur_) / fbW_, sy = static_cast<double>(winH_cur_) / fbH_;
            curX_ = fbPos.x * sx;
            curY_ = fbPos.y * sy;
            inventoryClick(button, mods);
        };
        creative_ = true;
        openInventory();
        SCHECK(paletteWindow_ && invLayout().armor);
        const glm::vec2 half = glm::vec2(invLayout().pitch() * 0.5f);
        for (int i = 0; i < 4; ++i) {  // vier Plaetze, alle im Fenster und unter dem Auswahlfeld
            const glm::vec2 c = invLayout().armorPos(i) + half;
            SCHECK(invLayout().hitArmor(c) == i && invLayout().inside(c) && invLayout().hitPalette(c) == -1 && invLayout().hitSlot(c) == -1);
        }
        // Aus dem Auswahlfeld holen: Reiter Werkzeuge, zweite Seite, Eisenbrustpanzer
        const ItemId chest = makeArmorItem(1, 1);
        clickAt(invLayout().tabPos(5) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        scrollPalette(1);
        int idx = -1;
        for (size_t i = 0; i < paletteItems().size(); ++i)
            if (paletteItems()[i] == chest) idx = static_cast<int>(i);
        const int vi = idx - creativeScroll_ * InvLayout::PALETTE_COLS;
        SCHECK(idx >= 0 && vi >= 0 && vi < InvLayout::PALETTE_PAGE);
        clickAt(invLayout().palettePos(vi) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.id == chest);
        clickAt(invLayout().armorPos(2) + half, GLFW_MOUSE_BUTTON_LEFT, 0);  // falscher Platz (Hose)
        SCHECK(cursor_.id == chest && inventory_.armor[2].empty());
        clickAt(invLayout().armorPos(1) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.empty() && inventory_.armor[1].id == chest && inventory_.armorPoints() == itemInfo(chest).armorPoints);
        clickAt(invLayout().armorPos(1) + half, GLFW_MOUSE_BUTTON_LEFT, 0);  // wieder abnehmen
        SCHECK(cursor_.id == chest && inventory_.armor[1].empty());
        cursor_.clear();
        // Umschalt-Klick aus dem Rucksack legt an
        inventory_.slots[12] = {makeArmorItem(3, 0), 1, 0};
        clickAt(invLayout().slotPos(12) + half, GLFW_MOUSE_BUTTON_LEFT, GLFW_MOD_SHIFT);
        SCHECK(inventory_.armor[0].id == makeArmorItem(3, 0) && inventory_.slots[12].empty());
        closeInventory();
        // Werkbank
        for (ItemStack& a : inventory_.armor) a.clear();
        openInventory(true);
        SCHECK(craftSize_ == 3 && !paletteWindow_ && invLayout().armor);
        for (int i = 0; i < 4; ++i) {
            const glm::vec2 c = invLayout().armorPos(i) + half;
            SCHECK(invLayout().hitArmor(c) == i && invLayout().inside(c) && invLayout().hitCraft(c) == -1 && invLayout().hitSlot(c) == -1);
        }
        cursor_ = {makeArmorItem(2, 3), 1, 0};
        clickAt(invLayout().armorPos(3) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.empty() && inventory_.armor[3].id == makeArmorItem(2, 3));
        closeInventory();
        for (ItemStack& a : inventory_.armor) a.clear();
        creative_ = false;
    }

    // 27. Eimer: Wasser und Lava aufnehmen und setzen
    {
        setPaused(false);
        creative_ = false;
        cursor_.clear();
        inventory_.clear();
        entities_.clear();
        for (int x = 18; x <= 24; ++x)
            for (int z = -6; z <= 4; ++z) {
                for (int y = gy; y <= gy + 5; ++y) world_->setBlock(x, y, z, AIR);
                world_->setBlock(x, gy - 1, z, STONE);
            }
        world_->setBlock(20, gy, 0, WATER);
        world_->setBlock(22, gy, 0, LAVA);
        player_.pos = glm::vec3(20.5f, static_cast<float>(gy), -2.5f);
        player_.vel = glm::vec3(0.0f);
        player_.yaw = 90.0f;  // nach +z
        auto lookAt = [&](float tx, float ty, float tz) {
            const glm::vec3 d = glm::normalize(glm::vec3(tx, ty, tz) - player_.eyePos());
            player_.yaw = glm::degrees(std::atan2(d.z, d.x));
            player_.pitch = glm::degrees(std::asin(d.y));
        };
        inventory_.slots[0] = {ITEM_BUCKET, 3};
        slot_ = 0;
        // leerer Eimer auf Boden oder Luft tut nichts
        lookAt(20.5f, gy + 0.0f, -0.5f);
        placeBlockOrUse();
        SCHECK(inventory_.slots[0] == ItemStack({ITEM_BUCKET, 3}));
        // Wasserquelle aufnehmen: ein Eimer wird zum Wassereimer, die anderen bleiben
        lookAt(20.5f, gy + 0.5f, 0.5f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(20, gy, 0) == AIR);
        SCHECK(inventory_.slots[0] == ItemStack({ITEM_BUCKET, 2}) && inventory_.count(ITEM_BUCKET_WATER) == 1);
        // Lava aufnehmen
        lookAt(22.5f, gy + 0.5f, 0.5f);
        placeBlockOrUse();
        SCHECK(world_->getBlock(22, gy, 0) == AIR && inventory_.count(ITEM_BUCKET_LAVA) == 1 && inventory_.count(ITEM_BUCKET) == 1);
        // Wasser setzen: auf den Boden vor dem Spieler; der Eimer ist danach leer
        int wslot = -1, lslot = -1;
        for (int i = 0; i < Inventory::SIZE; ++i) {
            if (inventory_.slots[i].id == ITEM_BUCKET_WATER) wslot = i;
            if (inventory_.slots[i].id == ITEM_BUCKET_LAVA) lslot = i;
        }
        SCHECK(wslot >= 0 && lslot >= 0);
        slot_ = std::min(wslot, Inventory::HOTBAR - 1);
        if (wslot >= Inventory::HOTBAR) std::swap(inventory_.slots[Inventory::HOTBAR - 1], inventory_.slots[wslot]);
        slot_ = wslot >= Inventory::HOTBAR ? Inventory::HOTBAR - 1 : wslot;
        lookAt(19.5f, gy - 1.0f + 1.0f, -1.5f);  // Boden bei (19, gy-1, -2)
        placeBlockOrUse();
        SCHECK(world_->getBlock(19, gy, -2) == WATER || world_->getBlock(19, gy, -1) == WATER || world_->getBlock(19, gy, -3) == WATER);
        SCHECK(inventory_.count(ITEM_BUCKET_WATER) == 0 && inventory_.count(ITEM_BUCKET) == 2);
        // das gesetzte Wasser fliesst
        for (int i = 0; i < 40; ++i) world_->tick(0.25f);
        int waterCells = 0;
        for (int x = 17; x <= 21; ++x)
            for (int z = -4; z <= 0; ++z) waterCells += isWater(world_->getBlock(x, gy, z)) ? 1 : 0;
        SCHECK(waterCells >= 3);
        // Lava setzen: Quelle entsteht, Eimer leer
        for (int i = 0; i < Inventory::SIZE; ++i)
            if (inventory_.slots[i].id == ITEM_BUCKET_LAVA) {
                std::swap(inventory_.slots[0], inventory_.slots[i]);
                break;
            }
        slot_ = 0;
        SCHECK(inventory_.slots[0].id == ITEM_BUCKET_LAVA);
        player_.pos = glm::vec3(23.5f, static_cast<float>(gy), -5.5f);  // an trockener Stelle, abseits vom gesetzten Wasser
        player_.vel = glm::vec3(0.0f);
        lookAt(23.5f, static_cast<float>(gy), -3.5f);
        placeBlockOrUse();
        int lavaCells = 0;
        for (int x = 22; x <= 24; ++x)
            for (int z = -5; z <= -2; ++z) lavaCells += isLava(world_->getBlock(x, gy, z)) ? 1 : 0;
        SCHECK(lavaCells >= 1 && inventory_.count(ITEM_BUCKET_LAVA) == 0 && inventory_.count(ITEM_BUCKET) == 3);
        // Fliessendes Wasser nimmt der Eimer nicht auf
        const glm::ivec3 flow = [&]() {
            for (int x = 17; x <= 21; ++x)
                for (int z = -6; z <= 0; ++z)
                    if (world_->getBlock(x, gy, z) >= WATER_1 && world_->getBlock(x, gy, z) <= WATER_7) return glm::ivec3(x, gy, z);
            return glm::ivec3(0, -1, 0);
        }();
        if (flow.y >= 0) {
            slot_ = 0;
            inventory_.slots[0] = {ITEM_BUCKET, 3};
            player_.pos = glm::vec3(flow.x + 0.5f, static_cast<float>(gy), flow.z - 2.5f);
            player_.vel = glm::vec3(0.0f);
            lookAt(flow.x + 0.5f, gy + 0.3f, flow.z + 0.5f);
            const BlockId before = world_->getBlock(flow.x, flow.y, flow.z);
            placeBlockOrUse();
            SCHECK(world_->getBlock(flow.x, flow.y, flow.z) == before && inventory_.slots[0] == ItemStack({ITEM_BUCKET, 3}));
        }
        inventory_.clear();
    }

    // 28. Schilder: setzen, beschriften, wieder oeffnen, entfernen
    {
        setPaused(false);
        creative_ = false;
        cursor_.clear();
        inventory_.clear();
        entities_.clear();
        for (int x = 28; x <= 34; ++x)
            for (int z = -6; z <= 4; ++z) {
                for (int y = gy; y <= gy + 5; ++y) world_->setBlock(x, y, z, AIR);
                world_->setBlock(x, gy - 1, z, STONE);
            }
        for (int y = gy; y <= gy + 2; ++y) world_->setBlock(33, y, 0, STONE);  // Wand fuer das Wandschild
        player_.pos = glm::vec3(30.5f, static_cast<float>(gy), -4.5f);
        player_.vel = glm::vec3(0.0f);
        auto lookAt = [&](float tx, float ty, float tz) {
            const glm::vec3 d = glm::normalize(glm::vec3(tx, ty, tz) - player_.eyePos());
            player_.yaw = glm::degrees(std::atan2(d.z, d.x));
            player_.pitch = glm::degrees(std::asin(d.y));
        };
        auto type = [&](const std::string& s) {
            for (char c : s) {
                handleKey(std::toupper(static_cast<unsigned char>(c)), GLFW_PRESS, 0);  // Tasten wie E tun hier nichts ausser Tippen
                handleChar(static_cast<unsigned char>(c));
            }
        };
        inventory_.slots[0] = {SIGN_BASE, 4};
        slot_ = 0;
        // Stehschild auf den Boden, Blick nach +z: die Vorderseite zeigt zum Spieler (-z)
        lookAt(30.5f, static_cast<float>(gy), -2.5f);
        target_ = world_->raycast(player_.eyePos(), player_.front(), 6.0f);
        placeBlockOrUse();
        glm::ivec3 sp = target_.block + target_.normal;
        SCHECK(isSign(world_->getBlock(sp.x, sp.y, sp.z)) && !signIsWall(world_->getBlock(sp.x, sp.y, sp.z)));
        SCHECK(signFacing(world_->getBlock(sp.x, sp.y, sp.z)) == 3 && inventory_.slots[0].count == 3);
        SCHECK(inventoryOpen_ && signWindow_ && signPos_ == sp);
        // Beschriften: E und Zahlen sind Buchstaben, Enter springt in die naechste Zeile, Umlaut wird ae
        type("Hallo E 5");
        handleKey(GLFW_KEY_ENTER, GLFW_PRESS, 0);
        type("Welt");
        handleChar(0xE4);
        handleKey(GLFW_KEY_ENTER, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_UP, GLFW_PRESS, 0);
        handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);  // loescht das e von "Weltae"
        SCHECK(inventoryOpen_ && signWindow_ && slot_ == 0 && signText_.lines[1] == "Welta");
        for (int i = 0; i < 20; ++i) handleChar('x');  // Zeile ist begrenzt
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(!inventoryOpen_ && !signWindow_);
        const SignText* st = world_->findSign(sp.x, sp.y, sp.z);
        SCHECK(st && st->lines[0] == "Hallo E 5" && st->lines[1].size() == SignText::MAX_CHARS && st->lines[1].rfind("Welta", 0) == 0 && st->lines[1][5] == 'x' && st->lines[2].empty());
        // Rechtsklick auf das Schild oeffnet es mit dem Text
        lookAt(sp.x + 0.5f, sp.y + 0.8f, sp.z + 0.5f);
        target_ = world_->raycast(player_.eyePos(), player_.front(), 6.0f);
        SCHECK(target_.hit && target_.block == sp);
        placeBlockOrUse();
        SCHECK(inventoryOpen_ && signWindow_ && signText_.lines[0] == "Hallo E 5");
        for (int i = 0; i < 12; ++i) handleKey(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0);  // erste Zeile leeren
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(world_->findSign(sp.x, sp.y, sp.z) && world_->findSign(sp.x, sp.y, sp.z)->lines[0].empty());
        // Wandschild an die -x-Seite der Wand bei x = 33 (Vorderseite -x)
        player_.pos = glm::vec3(31.5f, static_cast<float>(gy), 0.5f);
        lookAt(33.0f, gy + 1.5f, 0.5f);
        target_ = world_->raycast(player_.eyePos(), player_.front(), 6.0f);
        SCHECK(target_.hit && target_.normal == glm::ivec3(-1, 0, 0));
        placeBlockOrUse();
        SCHECK(world_->getBlock(32, target_.block.y, 0) == makeSign(true, 1));
        SCHECK(inventoryOpen_ && signWindow_);
        type("Tor");
        handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        SCHECK(world_->findSign(32, target_.block.y, 0) && world_->findSign(32, target_.block.y, 0)->lines[0] == "Tor");
        // Abbauen: Schild weg, Text weg, Gegenstand faellt
        world_->setBlock(32, target_.block.y, 0, AIR);
        SCHECK(world_->findSign(32, target_.block.y, 0) == nullptr);
        // Von unten gegen eine Decke geht nicht
        world_->setBlock(30, gy + 3, 1, STONE);
        player_.pos = glm::vec3(30.5f, static_cast<float>(gy), 1.5f);
        lookAt(30.5f, gy + 3.0f, 1.5f);
        target_ = world_->raycast(player_.eyePos(), player_.front(), 6.0f);
        const int before = inventory_.count(SIGN_BASE);
        placeBlockOrUse();
        SCHECK(!inventoryOpen_ && inventory_.count(SIGN_BASE) == before);
        inventory_.clear();
    }

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
