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
        "  --save-path PFAD  Speicherdatei (Standard: saves/world.dat)\n"
        "  --help            Diese Hilfe\n"
        "\n"
        "Steuerung: WASD, Maus, Leertaste springen, Strg sprinten, F fliegen,\n"
        "  Linksklick abbauen, Rechtsklick setzen, Mittelklick Block uebernehmen,\n"
        "  1-9 / Mausrad Hotbar, T Tageszeit, F1 HUD, F2 Screenshot, F5 Speichern,\n"
        "  F11 Vollbild, Esc Pause (dort Q beendet das Spiel).");
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
        } else if (a == "--distance") {
            need(1);
            opt.renderDistance = std::atoi(argv[++i]);
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
