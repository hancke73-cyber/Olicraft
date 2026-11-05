#include "Game.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>

#include "Crafting.h"
#include "Models.h"
#include "Trades.h"
#include "Noise.h"

namespace oc {

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
                const int before2 = settings_.playerSkin;
                activateWidget(idx, m, glm::vec2(0.0f));
                SCHECK(settings_.playerSkin == (before2 + 1) % PLAYER_SKIN_SLOTS && textures_.playerSlot() == settings_.playerSkin);
                SCHECK(textures_.skin(SKIN_PLAYER).rgba == textures_.playerSkinImage(settings_.playerSkin).rgba);
                for (int i = 1; i < PLAYER_SKIN_SLOTS; ++i) activateWidget(idx, m, glm::vec2(0.0f));
                SCHECK(settings_.playerSkin == before2);
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
            const BlockId before2 = world_->getBlock(flow.x, flow.y, flow.z);
            placeBlockOrUse();
            SCHECK(world_->getBlock(flow.x, flow.y, flow.z) == before2 && inventory_.slots[0] == ItemStack({ITEM_BUCKET, 3}));
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
        const int before2 = inventory_.count(SIGN_BASE);
        placeBlockOrUse();
        SCHECK(!inventoryOpen_ && inventory_.count(SIGN_BASE) == before2);
        inventory_.clear();
    }

    // 29. Dampfmaschinen im Spiel: setzen, Fenster, Anzeige, Beute, Abbauen
    {
        setPaused(false);
        creative_ = false;
        cursor_.clear();
        inventory_.clear();
        entities_.clear();
        for (int x = 40; x <= 47; ++x)
            for (int z = -6; z <= 6; ++z) {
                for (int y = gy; y <= gy + 5; ++y) world_->setBlock(x, y, z, AIR);
                world_->setBlock(x, gy - 1, z, STONE);
            }
        player_.pos = glm::vec3(41.5f, static_cast<float>(gy), -4.5f);
        player_.vel = glm::vec3(0.0f);
        auto lookAt = [&](float tx, float ty, float tz) {
            const glm::vec3 d = glm::normalize(glm::vec3(tx, ty, tz) - player_.eyePos());
            player_.yaw = glm::degrees(std::atan2(d.z, d.x));
            player_.pitch = glm::degrees(std::asin(d.y));
        };
        auto clickAt = [&](glm::vec2 fbPos, int button, int mods) {
            const double sx = static_cast<double>(winW_cur_) / fbW_, sy = static_cast<double>(winH_cur_) / fbH_;
            curX_ = fbPos.x * sx;
            curY_ = fbPos.y * sy;
            inventoryClick(button, mods);
        };
        auto place = [&](ItemId item, float tx, float tz) {  // von oben auf den Boden bei (tx, tz) setzen
            inventory_.slots[0] = {item, 8};
            slot_ = 0;
            placeTimer_ = 0.0f;
            lookAt(tx, static_cast<float>(gy), tz);
            target_ = world_->raycast(player_.eyePos(), player_.front(), 6.0f);
            placeBlockOrUse();
        };
        // Bohrer zuerst: Blick nach +x (Gelaende fuer den Treffer unter dem Blick), danach Kessel, Rohr, Motor davor
        inventory_.slots[0] = {DRILL_BASE, 4};
        slot_ = 0;
        placeTimer_ = 0.0f;
        player_.pos = glm::vec3(41.5f, static_cast<float>(gy), -3.5f);
        player_.yaw = 0.0f;
        player_.pitch = -40.0f;
        target_ = world_->raycast(player_.eyePos(), player_.front(), 8.0f);
        placeBlockOrUse();
        SCHECK(isDrill(world_->getBlock(43, gy, -4)) && drillFacing(world_->getBlock(43, gy, -4)) == 0);
        player_.pos = glm::vec3(41.5f, static_cast<float>(gy), -7.5f);
        place(BOILER, 40.5f, -3.5f);
        place(PIPE, 41.5f, -3.5f);
        place(MOTOR, 42.5f, -3.5f);
        SCHECK(world_->getBlock(40, gy, -4) == BOILER && world_->getBlock(41, gy, -4) == PIPE && world_->getBlock(42, gy, -4) == MOTOR);
        SCHECK(world_->findMachine(40, gy, -4) && world_->findMachine(42, gy, -4) && world_->findMachine(43, gy, -4));
        // Foerderband in Blickrichtung
        inventory_.slots[0] = {CONVEYOR_BASE, 4};
        placeTimer_ = 0.0f;
        player_.yaw = 90.0f;
        player_.pitch = -40.0f;
        player_.pos = glm::vec3(46.5f, static_cast<float>(gy), -4.5f);
        target_ = world_->raycast(player_.eyePos(), player_.front(), 8.0f);
        const glm::ivec3 cp = target_.block + target_.normal;
        placeBlockOrUse();
        SCHECK(isConveyor(world_->getBlock(cp.x, cp.y, cp.z)) && conveyorFacing(world_->getBlock(cp.x, cp.y, cp.z)) == 2);
        world_->setBlock(cp.x, cp.y, cp.z, AIR);
        // Kesselfenster: Rechtsklick, Plaetze mit Annahmeregeln
        player_.pos = glm::vec3(40.5f, static_cast<float>(gy), -6.5f);
        lookAt(40.5f, gy + 0.5f, -3.5f);
        target_ = world_->raycast(player_.eyePos(), player_.front(), 6.0f);
        SCHECK(target_.hit && target_.block == glm::ivec3(40, gy, -4));
        inventory_.slots[0] = {ITEM_NONE, 0};
        placeBlockOrUse();
        SCHECK(inventoryOpen_ && furnaceWindow_ && machineWindow_ && machinePos_ == glm::ivec3(40, gy, -4));
        const glm::vec2 half = glm::vec2(invLayout().pitch() * 0.5f);
        Machine* bm = world_->findMachine(40, gy, -4);
        SCHECK(bm != nullptr);
        cursor_ = {STONE, 5, 0};  // Stein ist weder Wasser noch Brennstoff
        clickAt(invLayout().furnaceSlotPos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        clickAt(invLayout().furnaceSlotPos(1) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.id == STONE && bm->slots[0].empty() && bm->slots[1].empty());
        cursor_ = {ITEM_BUCKET_WATER, 1, 0};
        clickAt(invLayout().furnaceSlotPos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.empty() && bm->slots[1].id == ITEM_BUCKET_WATER);  // Wassereimer im oberen Platz
        cursor_ = {ITEM_COAL, 7, 0};
        clickAt(invLayout().furnaceSlotPos(1) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.empty() && bm->slots[0] == ItemStack({ITEM_COAL, 7}));
        clickAt(invLayout().furnaceSlotPos(2) + half, GLFW_MOUSE_BUTTON_LEFT, 0);  // dritter Platz ist beim Kessel unbenutzt
        SCHECK(cursor_.empty());
        closeInventory();
        SCHECK(!machineWindow_ && !furnaceWindow_);
        // Anzeige: Name und Zustand
        {
            const std::array<std::string, 3> info = machineInfo(*world_, glm::ivec3(40, gy, -4));
            SCHECK(info[0] == "Dampfkessel" && info[1].find("Wasser") == 0);
        }
        // Der Antrieb laeuft: Kessel zuendet, Motor dreht, Bohrer bricht Steine vor sich
        const glm::ivec3 dp = [&]() {
            for (int x = 40; x <= 47; ++x)
                for (int z = -6; z <= 6; ++z)
                    if (isDrill(world_->getBlock(x, gy, z))) return glm::ivec3(x, gy, z);
            return glm::ivec3(0, -1, 0);
        }();
        SCHECK(dp.y >= 0);
        if (dp.y >= 0) {
            const int df = drillFacing(world_->getBlock(dp.x, dp.y, dp.z));
            const int ddx = df == 0 ? 1 : (df == 1 ? -1 : 0), ddz = df == 2 ? 1 : (df == 3 ? -1 : 0);
            for (int d = 1; d <= 3; ++d) world_->setBlock(dp.x + ddx * d, gy, dp.z + ddz * d, STONE);
            const bool touching = std::abs(dp.x - 42) + std::abs(dp.z - (-4)) == 1;
            SCHECK(touching);  // der Bohrer steht am Motor
            for (int i = 0; i < 80; ++i) world_->tick(0.25f);
            SCHECK(world_->getBlock(40, gy, -4) == BOILER_LIT || bm->burnTime > 0.0f || bm->slots[0].count < 7);
            SCHECK(world_->findMachine(42, gy, -4) && world_->findMachine(42, gy, -4)->spin > 1.0f);
            SCHECK(world_->getBlock(dp.x + ddx, gy, dp.z + ddz) == AIR);
            // Beute: Gegenstaende fallen aus dem Bohrer
            for (int i = 0; i < 40; ++i) {
                for (const MachineDrop& d : world_->takeMachineDrops()) entities_.spawnItem(d.item, glm::vec3(d.pos) + glm::vec3(0.5f), glm::vec3(0.0f), 0.0f);
                world_->tick(0.25f);
            }
            const std::array<std::string, 3> di = machineInfo(*world_, dp);
            SCHECK(di[0] == "Bohrmaschine" && di[1].find("Antrieb") == 0);
        }
        // Destille: Eingabe nur mit Rezept, Ausgaben nur entnehmen, Beute nach dem Abbauen
        world_->setBlock(44, gy, 2, BOILER);
        world_->setBlock(45, gy, 2, PIPE);
        world_->setBlock(46, gy, 2, STILL);
        Machine* b2 = world_->findMachine(44, gy, 2);
        b2->slots[0] = {ITEM_COAL, 2, 0};
        b2->water = 800.0f;
        openMachine(glm::ivec3(46, gy, 2));
        SCHECK(machineWindow_);
        Machine* st = world_->findMachine(46, gy, 2);
        cursor_ = {STONE, 1, 0};
        clickAt(invLayout().furnaceSlotPos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.id == STONE && st->slots[0].empty());
        cursor_ = {ITEM_COAL, 2, 0};
        clickAt(invLayout().furnaceSlotPos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
        SCHECK(cursor_.empty() && st->slots[0] == ItemStack({ITEM_COAL, 2}));
        cursor_ = {ITEM_TAR, 1, 0};
        clickAt(invLayout().furnaceSlotPos(2) + half, GLFW_MOUSE_BUTTON_LEFT, 0);  // Ausgabe nimmt nichts an
        SCHECK(cursor_.id == ITEM_TAR && st->slots[1].empty());
        cursor_.clear();
        for (int i = 0; i < 70; ++i) world_->tick(0.25f);
        SCHECK(st->slots[1].id == ITEM_TAR && st->slots[1].count == 2);
        clickAt(invLayout().furnaceSlotPos(2) + half, GLFW_MOUSE_BUTTON_LEFT, 0);  // Ergebnis entnehmen
        SCHECK(cursor_.id == ITEM_TAR && cursor_.count == 2 && st->slots[1].empty());
        closeInventory();
        SCHECK(inventory_.count(ITEM_TAR) == 2);
        // Abbauen: Inhalt faellt heraus, Maschine verschwindet
        st->slots[0] = {ITEM_COAL, 3, 0};
        entities_.clear();
        breakBlock(glm::ivec3(46, gy, 2));
        SCHECK(world_->getBlock(46, gy, 2) == AIR && !world_->findMachine(46, gy, 2) && !entities_.items().empty());
        inventory_.clear();
        entities_.clear();
        world_->tick(0.3f);
    }

    // 30. Fahrzeuge im Spiel: Boot setzen, einsteigen, Bordkessel fuellen, fahren, aussteigen; Luftschiff ausbauen und zerlegen
    {
        setPaused(false);
        creative_ = false;
        cursor_.clear();
        inventory_.clear();
        entities_.clear();
        vehicles_.clear();
        ridingId_ = 0;
        for (int x = 40; x <= 46; ++x)
            for (int z = -6; z <= 8; ++z) {
                for (int y = gy; y <= gy + 5; ++y) world_->setBlock(x, y, z, AIR);
                world_->setBlock(x, gy - 2, z, STONE);
                world_->setBlock(x, gy - 1, z, x >= 44 && z >= 0 && z <= 5 ? WATER : STONE);
            }
        auto lookAt = [&](float tx, float ty, float tz) {
            const glm::vec3 d = glm::normalize(glm::vec3(tx, ty, tz) - player_.eyePos());
            player_.yaw = glm::degrees(std::atan2(d.z, d.x));
            player_.pitch = glm::degrees(std::asin(d.y));
        };
        auto clickAt = [&](glm::vec2 fbPos, int button, int mods) {
            const double sx = static_cast<double>(winW_cur_) / fbW_, sy = static_cast<double>(winH_cur_) / fbH_;
            curX_ = fbPos.x * sx;
            curY_ = fbPos.y * sy;
            inventoryClick(button, mods);
        };
        player_.pos = glm::vec3(45.5f, static_cast<float>(gy), -2.5f);
        player_.vel = glm::vec3(0.0f);
        inventory_.slots[0] = {ITEM_STEAMBOAT, 2};
        slot_ = 0;
        lookAt(45.5f, gy - 0.5f, 1.5f);
        placeTimer_ = 0.0f;
        placeBlockOrUse();
        SCHECK(vehicles_.list().size() == 1 && vehicles_.list()[0].type == VehicleType::Boat && inventory_.slots[0].count == 1);
        // Ein Boot gehoert aufs Wasser, nicht auf festen Boden
        lookAt(41.5f, gy - 0.5f, -1.5f);
        placeTimer_ = 0.0f;
        placeBlockOrUse();
        SCHECK(vehicles_.list().size() == 1 && inventory_.slots[0].count == 1);
        Vehicle& boat = vehicles_.list()[0];
        const int boatId = boat.id;
        inventory_.slots[0].clear();
        interactVehicle(boat);
        SCHECK(ridingId_ == boatId);
        // Bordkessel fuellen (Fenster wie beim Kessel)
        openVehicleEngine(boatId);
        SCHECK(machineWindow_ && engineVehicleId_ == boatId && activeMachine() == &boat.engine);
        {
            const glm::vec2 half = glm::vec2(invLayout().pitch() * 0.5f);
            cursor_ = {ITEM_COAL, 3, 0};
            clickAt(invLayout().furnaceSlotPos(1) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
            cursor_ = {ITEM_BUCKET_WATER, 1, 0};
            clickAt(invLayout().furnaceSlotPos(0) + half, GLFW_MOUSE_BUTTON_LEFT, 0);
            SCHECK(cursor_.empty() && boat.engine.slots[0] == ItemStack({ITEM_COAL, 3}) && boat.engine.slots[1].id == ITEM_BUCKET_WATER);
        }
        closeInventory();
        SCHECK(engineVehicleId_ == 0 && !machineWindow_);
        MoveInput drive;
        drive.forward = 1.0f;
        const glm::vec3 start = boat.pos;
        for (int i = 0; i < 40; ++i) SCHECK(updateVehicles(0.05f, drive) || i < 0);
        SCHECK(glm::length(boat.pos - start) > 0.8f && boat.engine.burning() && boat.engine.water > 0.0f);
        SCHECK(glm::length(glm::vec2(player_.pos.x - vehicleSeat(boat).x, player_.pos.z - vehicleSeat(boat).z)) < 0.01f);
        {
            const std::array<std::string, 3> info = vehicleInfo(boat);
            SCHECK(info[0] == "Dampfboot" && info[1].find("Wasser") == 0);
        }
        handleKey(GLFW_KEY_F, GLFW_PRESS, 0);
        SCHECK(ridingId_ == 0 && glm::length(player_.pos - vehicleSeat(boat)) > 0.9f);

        // Luftschiff: Gondel auf festen Boden, Ballon, Propeller, zu kleiner Ballon fuer zwei Gondeln
        player_.pos = glm::vec3(41.5f, static_cast<float>(gy), -2.5f);
        inventory_.slots[0] = {ITEM_GONDOLA, 3};
        lookAt(41.5f, gy - 0.5f, 2.5f);
        placeTimer_ = 0.0f;
        placeBlockOrUse();
        SCHECK(vehicles_.list().size() == 2);
        Vehicle& ship = vehicles_.list()[1];
        SCHECK(ship.type == VehicleType::Airship && ship.anchored && ship.modules == 1 && !ship.flyable());
        inventory_.slots[0] = {ITEM_BALLOON_S, 1};
        interactVehicle(ship);
        SCHECK(ship.balloon == 0 && inventory_.slots[0].empty());
        inventory_.slots[0] = {ITEM_PROPELLER, 2};
        interactVehicle(ship);
        SCHECK(ship.propeller && ship.flyable() && inventory_.slots[0].count == 1);
        inventory_.slots[0] = {ITEM_GONDOLA, 2};
        interactVehicle(ship);
        SCHECK(ship.modules == 1 && inventory_.slots[0].count == 2);  // der kleine Ballon traegt keine zweite Gondel
        inventory_.slots[0] = {ITEM_BALLOON_M, 1};
        interactVehicle(ship);
        SCHECK(ship.balloon == 0 && inventory_.slots[0].count == 1);  // ein Ballon ist schon da
        // Einsteigen loest das Seil, Aussteigen macht fest
        inventory_.slots[0].clear();
        interactVehicle(ship);
        SCHECK(ridingId_ == ship.id && !ship.anchored);
        dismountVehicle();
        SCHECK(ridingId_ == 0 && ship.anchored);
        // Zerlegen: Bauteile fallen heraus
        entities_.clear();
        ship.engine.slots[0] = {ITEM_COAL, 2, 0};
        creative_ = true;
        attackTimer_ = 0.0f;
        hitVehicle(ship);
        creative_ = false;
        SCHECK(vehicles_.list().size() == 1 && entities_.items().size() >= 4);  // Gondel, Ballon, Propeller, Kohle
        inventory_.clear();
        entities_.clear();
        vehicles_.clear();
    }

    // 31. Dampfgolem: Lampe auf Kessel auf Eisenblock mit Eisenarmen erweckt ihn (nur mit Steampunk), Beute und Spielstand
    {
        setPaused(false);
        creative_ = false;
        cursor_.clear();
        inventory_.clear();
        entities_.clear();
        for (int x = 40; x <= 46; ++x)
            for (int z = -6; z <= 6; ++z) {
                for (int y = gy; y <= gy + 6; ++y) world_->setBlock(x, y, z, AIR);
                world_->setBlock(x, gy - 1, z, STONE);
            }
        // Rumpf aufstellen; der Spieler setzt als letztes Teil den Eisenblock der Beine auf den Boden (Klick auf den Kessel oeffnet sein Fenster)
        auto build = [&](int x, int z, bool bothArms) {
            world_->setBlock(x, gy + 1, z, BOILER);
            world_->setBlock(x, gy + 2, z, LAMP);
            world_->setBlock(x + 1, gy + 1, z, IRON_BLOCK);
            if (bothArms) world_->setBlock(x - 1, gy + 1, z, IRON_BLOCK);
            player_.pos = glm::vec3(x + 0.5f, static_cast<float>(gy), z - 3.0f);
            const glm::vec3 d = glm::normalize(glm::vec3(x + 0.5f, gy - 0.01f, z + 0.5f) - player_.eyePos());
            player_.yaw = glm::degrees(std::atan2(d.z, d.x));
            player_.pitch = glm::degrees(std::asin(d.y));
            inventory_.slots[0] = {IRON_BLOCK, 2};
            slot_ = 0;
            placeTimer_ = 0.0f;
            target_ = world_->raycast(player_.eyePos(), player_.front(), 6.0f);
            placeBlockOrUse();
        };
        auto countGolems = [&]() {
            int n = 0;
            for (const Mob& m : entities_.mobs()) n += m.type == MobType::Golem && m.owned ? 1 : 0;
            return n;
        };
        setSteampunkEnabled(false);
        build(43, 2, true);
        SCHECK(countGolems() == 0 && world_->getBlock(43, gy, 2) == IRON_BLOCK);  // ohne die Erweiterung bleibt es ein Bauwerk
        for (int y = gy; y <= gy + 2; ++y)
            for (int x = 42; x <= 44; ++x) world_->setBlock(x, y, 2, AIR);
        inventory_.clear();
        setSteampunkEnabled(true);
        build(43, 2, true);
        SCHECK(countGolems() == 1);
        SCHECK(world_->getBlock(43, gy, 2) == AIR && world_->getBlock(43, gy + 1, 2) == AIR && world_->getBlock(42, gy + 1, 2) == AIR &&
               world_->getBlock(44, gy + 1, 2) == AIR && world_->getBlock(43, gy + 2, 2) == AIR && !world_->findMachine(43, gy + 1, 2));
        SCHECK(inventory_.slots[0].count == 1);
        // Ohne linken Arm entsteht kein Golem
        build(41, -2, false);
        SCHECK(countGolems() == 1 && world_->getBlock(41, gy, -2) == IRON_BLOCK && world_->getBlock(41, gy + 2, -2) == LAMP);
        inventory_.clear();
        entities_.clear();
    }

#undef SCHECK
    std::printf("Selbsttest: %d Pruefungen, %d fehlgeschlagen\n", checks, failed);
    return failed;
}

}  // namespace oc
