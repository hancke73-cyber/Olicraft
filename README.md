# OliCraft

Ein Minecraft-Klon in C++17 mit OpenGL 3.3, GLFW und GLM. Alle Texturen werden beim Start prozedural erzeugt, es gibt keine externen Assets.

## Funktionen

- Unendliche, chunkbasierte Welt (16 x 128 x 16), Gelände aus Perlin-Rauschen: Ozeane, Strände, Hügel, Berge mit Schneekappen, Wüsten, Wälder, Höhlen
- Bäume (über Chunkgrenzen hinweg konsistent), Meer mit halbtransparentem Wasser, Schwimmen
- Abbauen und Setzen von Blöcken per Raycast, Hotbar mit 9 Blöcken
- Ego-Perspektive mit AABB-Kollision, Springen, Sprinten, Fliegen
- Tag-Nacht-Zyklus (10 Minuten) mit Sonne, Mond und Sternen, Nebel, Unterwasser-Effekt
- Ambient Occlusion und einfaches Himmelslicht (Höhlen und Überhänge sind dunkler)
- Speichern und Laden: Seed, Position, Hotbar, Tageszeit und alle vom Spieler veränderten Chunks
- Screenshots als BMP

Bewusst nicht enthalten: Mobs, Inventar und Crafting, fließendes Wasser, Mehrspieler, Sound.

## Steuerung

| Taste | Aktion |
|---|---|
| W A S D | Laufen |
| Maus | Umsehen |
| Leertaste | Springen, im Wasser schwimmen, beim Fliegen steigen |
| Strg | Sprinten |
| Umschalt | Beim Fliegen sinken |
| F oder Leertaste doppelt | Fliegen an/aus |
| Linksklick (halten) | Block abbauen |
| Rechtsklick (halten) | Block setzen |
| Mittelklick | Block unter dem Fadenkreuz in den aktiven Hotbar-Slot übernehmen |
| 1 bis 9, Mausrad | Hotbar-Slot wählen |
| T | Zwischen Tag und Nacht springen |
| F1 | HUD ein/aus |
| F2 | Screenshot nach `screenshots/` |
| F5 | Speichern (zusätzlich alle 90 s und beim Beenden) |
| F11 | Vollbild |
| Esc | Pause. Klick setzt fort, Q beendet das Spiel |

Position, FPS, Uhrzeit und gewählter Block stehen in der Fenstertitelzeile.

## Bauen

Voraussetzungen: CMake 3.16 oder neuer, ein C++17-Compiler und Git. GLFW und GLM werden beim ersten Konfigurieren automatisch per Git geladen (Internetverbindung nötig). GLAD liegt bereits im Repository.

### Windows

Visual Studio 2022 (Workload "Desktopentwicklung mit C++"):

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\OliCraft.exe
```

Oder `build-windows.bat` doppelklicken. Mit MinGW-w64:

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\OliCraft.exe
```

Das Spiel aus dem Ordner starten, in dem `saves/` und `screenshots/` entstehen sollen (Explorer-Doppelklick auf die .exe genügt).

### Linux

Benötigt die X11-Entwicklungspakete für GLFW. Unter Debian/Ubuntu:

```bash
sudo apt install build-essential cmake git libgl-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build -j
```

```bash
./build/OliCraft
```

## Optionen

```
--seed N           Neue Welt mit diesem Seed (überschreibt den Spielstand)
--new              Neue Welt mit zufälligem Seed (überschreibt den Spielstand)
--distance N       Sichtweite in Chunks, 4 bis 24 (Standard 10)
--width N --height N   Fenstergröße
--fullscreen       Vollbild
--no-vsync         VSync aus
--no-save          Nichts laden oder speichern
--save-path PFAD   Speicherdatei (Standard: saves/world.dat)
```

Entwicklungsoptionen: `--screenshot DATEI --frames N` (Screenshot nach N Frames, dann beenden), `--pos X Y Z`, `--look YAW PITCH`, `--time T` (0 bis 1, 0.25 ist Mittag), `--fly`, `--bench N` (automatischer Flug, gibt Frame-Zeiten aus).

## Tests

Die Tests laufen ohne OpenGL-Kontext und prüfen Rauschen, Terrain-Determinismus, Chunkgrenzen, Raycast, Spielerphysik, Mesher und Speichern.

```bash
cmake -S . -B build -DOLICRAFT_BUILD_TESTS=ON
```

```bash
cmake --build build -j && ctest --test-dir build --output-on-failure
```

## Aufbau

| Datei | Inhalt |
|---|---|
| `src/Game.*` | Fenster, Eingabe, Hauptschleife, Interaktion |
| `src/World.*` | Chunk-Verwaltung, Streaming, Block-Zugriff, Raycast |
| `src/TerrainGen.*`, `src/Noise.*` | Weltgenerierung |
| `src/Mesher.*`, `src/Chunk.*` | Geometrie je Chunk (Face-Culling, AO, Himmelslicht), GPU-Puffer |
| `src/Renderer.*`, `src/Shader.*`, `src/Atlas.*` | Rendering, Himmel, HUD, prozeduraler Texturatlas |
| `src/Player.*` | Bewegung und Kollision |
| `src/Save.*` | Speicherformat (RLE je veränderter Chunk) |
| `tests/` | Tests |

## Drittanbieter

- [GLFW](https://www.glfw.org/) 3.4 (zlib/libpng-Lizenz)
- [GLM](https://github.com/g-truc/glm) 1.0.1 (MIT)
- GLAD 2, erzeugt für OpenGL 3.3 Core, liegt in `third_party/glad` (SPDX laut Header: WTFPL OR CC0-1.0, AND Apache-2.0)
