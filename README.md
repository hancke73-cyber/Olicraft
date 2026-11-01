# OliCraft

Ein Minecraft-Klon in C++17 mit OpenGL 3.3, GLFW und GLM. Alle Texturen werden beim Start prozedural erzeugt, es gibt keine externen Assets.

## Funktionen

- Unendliche, chunkbasierte Welt (16 x 128 x 16), Gelände aus Perlin-Rauschen: Ozeane, Strände, Hügel, Berge mit Schneekappen, Wüsten, Wälder, Höhlen
- Bäume (über Chunkgrenzen hinweg konsistent), Meer mit halbtransparentem Wasser, Schwimmen
- Fließendes Wasser: Quellen breiten sich bis 7 Blöcke aus, fließen bergab, bilden Wasserfälle, zwei Quellen ergeben eine neue (unendliche Quelle), Wasser zieht sich zurück, wenn die Quelle fehlt
- Abbauen und Setzen von Blöcken per Raycast
- Survival- und Kreativmodus (Umschalten mit G): Abbauzeit je Block, Drops als Gegenstände am Boden, Gesundheit mit Herzen, Fallschaden, Ertrinken (Luftblasen), Regeneration, Tod und Wiedergeburt
- Inventar (E): 27 Rucksack-Slots plus 9 Hotbar-Slots, Stapel bis 64, Mausbedienung wie in Minecraft (links ganzen Stapel, rechts Hälfte oder eines, Umschalt-Klick verschiebt). Im Kreativmodus zusätzlich eine Auswahl aller Blöcke
- Mobs: Schweine, Kühe und Schafe laufen herum und fliehen bei Treffern, lassen rohes Fleisch fallen (essen heilt). Zombies spawnen nachts und in dunklen Höhlen, verfolgen und greifen an und verbrennen im Tageslicht
- Ego-Perspektive mit AABB-Kollision, Springen, Sprinten, Fliegen (Kreativ)
- Tag-Nacht-Zyklus (10 Minuten) mit Sonne, Mond und Sternen, Nebel, Unterwasser-Effekt
- Ambient Occlusion und einfaches Himmelslicht (Höhlen und Überhänge sind dunkler)
- Speichern und Laden: Seed, Position, Hotbar, Tageszeit und alle vom Spieler veränderten Chunks
- Screenshots als BMP

Bewusst nicht enthalten: Crafting, Werkzeuge, Hunger, Sound, Mehrspieler.

## Steuerung

| Taste | Aktion |
|---|---|
| W A S D | Laufen |
| Maus | Umsehen |
| Leertaste | Springen, im Wasser schwimmen, beim Fliegen steigen |
| Strg | Sprinten |
| Umschalt | Beim Fliegen sinken |
| F oder Leertaste doppelt | Fliegen an/aus (nur Kreativ) |
| Linksklick (halten) | Block abbauen, Mob angreifen |
| Rechtsklick (halten) | Block setzen, mit rohem Fleisch in der Hand essen |
| Mittelklick | Block unter dem Fadenkreuz übernehmen (Kreativ: in den aktiven Slot, sonst Slot wählen) |
| 1 bis 9, Mausrad | Hotbar-Slot wählen |
| E | Inventar öffnen/schließen |
| Q | Einen Gegenstand aus dem aktiven Slot werfen (Strg+Q: ganzer Stapel) |
| G | Zwischen Survival und Kreativ wechseln |
| T | Zwischen Tag und Nacht springen |
| F1 | HUD ein/aus |
| F2 | Screenshot nach `screenshots/` |
| F5 | Speichern (zusätzlich alle 90 s und beim Beenden) |
| F11 | Vollbild |
| Esc | Inventar schließen, sonst Pause. Klick setzt fort, Q beendet das Spiel |

Im Inventar: Linksklick nimmt oder legt einen ganzen Stapel, Rechtsklick nimmt die Hälfte bzw. legt eines ab, Umschalt+Linksklick verschiebt zwischen Hotbar und Rucksack, ein Klick außerhalb des Fensters wirft den gehaltenen Stapel weg. Nach dem Tod lässt dich die Leertaste am Startpunkt wiederkehren (Inventar bleibt erhalten).

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

### Fertige Windows-Datei

`dist/OliCraft-windows-x64.exe` ist ein mit MinGW-w64 unter Linux gebauter Release-Build (64 Bit, braucht nur Windows-Systembibliotheken und eine OpenGL-3.3-fähige Grafikkarte). Er ist nicht signiert, Windows SmartScreen kann deshalb warnen ("Weitere Informationen", "Trotzdem ausführen"). Er wird nicht bei jeder Änderung neu erzeugt, der aktuelle Stand ist der Quelltext.

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

Entwicklungsoptionen: `--screenshot DATEI --frames N` (Screenshot nach N Frames, dann beenden), `--pos X Y Z`, `--look YAW PITCH`, `--time T` (0 bis 1, 0.25 ist Mittag), `--fly`, `--health N`, `--demo mobs|water|inv` (Szenen für Screenshots), `--bench N` (automatischer Flug, gibt Frame-Zeiten aus), `--selftest` (prüft Abbauen, Setzen, Drops, Essen, Schaden, Fallschaden, Ertrinken, Tod, Inventar-Klicks, Speichern in einem echten Fenster).

## Tests

Die Tests laufen ohne OpenGL-Kontext und prüfen Rauschen, Terrain-Determinismus, Chunkgrenzen, Raycast, Spielerphysik, Mesher, Wassersimulation, Inventar, Items, Mobs (Physik, KI, Spawnen, Beute) und das Speicherformat.

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
| `src/Player.*`, `src/Physics.*` | Bewegung und Kollision (auch für Mobs und Gegenstände) |
| `src/Entities.*` | Mobs (KI, Spawnen, Modelle aus Quadern) und Gegenstände am Boden |
| `src/Items.*`, `src/Inventory.*` | Gegenstände, Stapel, Klicklogik |
| `src/Hud.*`, `src/GuiTexture.*` | HUD, Inventarfenster, Schrift (5x7-Bitmap) und Symbole |
| `src/Save.*` | Speicherformat Version 2 (RLE je veränderter Chunk, Inventar, Gesundheit, Modus; Version 1 bleibt lesbar) |
| `tests/` | Tests |

## Bekannte Probleme

Gemeldet, noch nicht behoben:

1. **Aus dem Wasser an Land kommen:** Man kommt vom Wasser nicht auf das Ufer, der Sprung im Wasser reicht nicht über die Uferkante.
2. **Zombies tagsüber:** Ein Zombie hat tagsüber angegriffen. Zombies sollen nur nachts auftreten.

## Geplant

Noch nicht umgesetzt, in dieser Reihenfolge der Nennung:

1. **Sound:** Effekte und Musik. Vorher wird die Audio-Bibliothek geklärt.
2. **Mehrspieler:** Host im Spiel, Beitreten per IP und Port. Vorher wird die Netzwerkbibliothek geklärt.
3. **Lava:** Lava-Block auf Basis der Wassersimulation, mit langsamerem Fluss, Schaden und Brennen sowie Lava-Seen in Höhlen. Dazu die Interaktion mit Wasser (Obsidian oder Bruchstein).
4. **Third-Person-Ansicht:** Kamera hinter oder vor dem Charakter (z. B. umschaltbar), mit eigenem Spielermodell aus Quadern wie bei den Mobs. Die Kamera darf vom Gelände nicht blockiert werden.
5. **Textur-Editor:** Im Spiel erreichbar über ein Einstellungsmenü. Damit lassen sich Texturen von Blöcken und Charakteren ändern und speichern. Beim Start lädt das Spiel gespeicherte Texturen statt der prozeduralen. Voraussetzung ist ein Einstellungsmenü, das es noch nicht gibt (bisher nur die Pause mit Esc).
6. **Crafting und Werkzeuge.**
7. **Fahrzeuge:** Flugzeuge, Schiffe, Autos, Zeppeline usw.

## Drittanbieter

- [GLFW](https://www.glfw.org/) 3.4 (zlib/libpng-Lizenz)
- [GLM](https://github.com/g-truc/glm) 1.0.1 (MIT)
- GLAD 2, erzeugt für OpenGL 3.3 Core, liegt in `third_party/glad` (SPDX laut Header: WTFPL OR CC0-1.0, AND Apache-2.0)
