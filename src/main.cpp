#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Game.h"

static void printHelp() {
    std::puts(
        "OliCraft - ein Minecraft-Klon in C++\n"
        "\n"
        "Optionen:\n"
        "  --seed N          Neue Welt mit diesem Seed (ueberschreibt den Spielstand)\n"
        "  --new             Neue Welt mit zufaelligem Seed (ueberschreibt den Spielstand)\n"
        "  --distance N      Sichtweite in Chunks (4..24, Standard 10)\n"
        "  --width N --height N   Fenstergroesse\n"
        "  --fullscreen      Vollbild\n"
        "  --no-vsync        VSync aus\n"
        "  --no-save         Nichts laden oder speichern\n"
        "  --creative        Kreativmodus erzwingen (Standard: Survival, im Spiel mit G umschaltbar)\n"
        "  --survival        Survival erzwingen\n"
        "  --no-mobs         Keine Tiere und Monster\n"
        "  --save-path PFAD  Speicherdatei (Standard: saves/world.dat)\n"
        "  --settings PFAD   Einstellungsdatei (Standard: settings.cfg)\n"
        "  --help            Diese Hilfe\n"
        "\n"
        "Steuerung: WASD, Maus, Leertaste springen, Strg sprinten, F fliegen (Kreativ),\n"
        "  Linksklick abbauen/angreifen, Rechtsklick setzen/essen, Mittelklick Block uebernehmen,\n"
        "  E Inventar, Q Gegenstand werfen, G Modus wechseln, 1-9 / Mausrad Hotbar,\n"
        "  T Tageszeit, F1 HUD, F2 Screenshot, F5 Speichern, F11 Vollbild,\n"
        "  Esc Pause (dort Q beendet das Spiel).");
}

int main(int argc, char** argv) {
    oc::Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](int n) {
            if (i + n >= argc) {
                std::fprintf(stderr, "Option %s braucht %d Wert(e)\n", a.c_str(), n);
                std::exit(2);
            }
        };
        if (a == "--help" || a == "-h") {
            printHelp();
            return 0;
        } else if (a == "--seed") {
            need(1);
            opt.hasSeed = true;
            opt.newWorld = true;
            opt.seed = static_cast<int32_t>(std::strtol(argv[++i], nullptr, 10));
        } else if (a == "--new") {
            opt.newWorld = true;
        } else if (a == "--no-save") {
            opt.noSave = true;
        } else if (a == "--save-path") {
            need(1);
            opt.savePath = argv[++i];
        } else if (a == "--settings") {
            need(1);
            opt.settingsPath = argv[++i];
        } else if (a == "--distance") {
            need(1);
            opt.renderDistance = std::atoi(argv[++i]);
            opt.distanceCli = true;
        } else if (a == "--width") {
            need(1);
            opt.width = std::atoi(argv[++i]);
        } else if (a == "--height") {
            need(1);
            opt.height = std::atoi(argv[++i]);
        } else if (a == "--fullscreen") {
            opt.fullscreen = true;
        } else if (a == "--no-vsync") {
            opt.vsync = false;
            opt.vsyncCli = true;
        } else if (a == "--screenshot") {
            need(1);
            opt.screenshotPath = argv[++i];
        } else if (a == "--frames") {
            need(1);
            opt.screenshotFrames = std::atoi(argv[++i]);
        } else if (a == "--pos") {
            need(3);
            opt.hasPos = true;
            opt.pos.x = static_cast<float>(std::atof(argv[++i]));
            opt.pos.y = static_cast<float>(std::atof(argv[++i]));
            opt.pos.z = static_cast<float>(std::atof(argv[++i]));
        } else if (a == "--look") {
            need(2);
            opt.hasLook = true;
            opt.yaw = static_cast<float>(std::atof(argv[++i]));
            opt.pitch = static_cast<float>(std::atof(argv[++i]));
        } else if (a == "--time") {
            need(1);
            opt.hasTime = true;
            opt.timeOfDay = static_cast<float>(std::atof(argv[++i]));
        } else if (a == "--bench") {
            need(1);
            opt.benchFrames = std::atoi(argv[++i]);
        } else if (a == "--creative") {
            opt.forceMode = 1;
        } else if (a == "--survival") {
            opt.forceMode = 0;
        } else if (a == "--no-mobs") {
            opt.mobs = false;
        } else if (a == "--demo") {
            need(1);
            opt.demo = argv[++i];
        } else if (a == "--selftest") {
            opt.selfTest = true;
            opt.noSave = false;
        } else if (a == "--health") {
            need(1);
            opt.health = std::atoi(argv[++i]);
        } else if (a == "--view") {
            need(1);
            opt.viewMode = std::atoi(argv[++i]);
        } else if (a == "--fly") {
            opt.fly = true;
        } else {
            std::fprintf(stderr, "Unbekannte Option: %s (siehe --help)\n", a.c_str());
            return 2;
        }
    }
    if (opt.renderDistance < 4) opt.renderDistance = 4;
    if (opt.renderDistance > 24) opt.renderDistance = 24;
    if (opt.width < 320) opt.width = 320;
    if (opt.height < 240) opt.height = 240;

    oc::Game game(opt);
    return game.run();
}
