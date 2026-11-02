# OliCraft

Ein Minecraft-Klon in C++17 mit OpenGL 3.3, GLFW und GLM. Alle Texturen werden beim Start prozedural erzeugt, es gibt keine externen Assets.

## Funktionen

- Unendliche, chunkbasierte Welt (16 x 128 x 16), Gelände aus Perlin-Rauschen: Ozeane, Strände, Hügel, Berge mit Schneekappen, Wüsten, Wälder, Höhlen
- Bäume (über Chunkgrenzen hinweg konsistent), Meer mit halbtransparentem Wasser, Schwimmen, an Uferkanten mit der Sprungtaste aus dem Wasser klettern
- Fließendes Wasser: Quellen breiten sich bis 7 Blöcke aus, fließen bergab, bilden Wasserfälle, zwei Quellen ergeben eine neue (unendliche Quelle), Wasser zieht sich zurück, wenn die Quelle fehlt
- Lava: Lava-Seen in tiefen Höhlen (bis Höhe 10), fließt langsamer (1,5 s je Schritt) und nur 3 Blöcke weit, verursacht Schaden und Brennen (Wasser löscht), leuchtet, vernichtet Gegenstände und verletzt Mobs. Trifft Wasser auf eine Lavaquelle, entsteht Obsidian, auf fließende Lava Bruchstein
- Abbauen und Setzen von Blöcken per Raycast
- Survival- und Kreativmodus (Umschalten mit G): Abbauzeit je Block, Drops als Gegenstände am Boden, Gesundheit mit Herzen, Fallschaden, Ertrinken (Luftblasen), Regeneration, Tod und Wiedergeburt
- Inventar (E): 27 Rucksack-Slots plus 9 Hotbar-Slots, Stapel bis 64, Mausbedienung wie in Minecraft (links ganzen Stapel, rechts Hälfte oder eines, Umschalt-Klick verschiebt). Im Kreativmodus zusätzlich eine Auswahl aller Blöcke
- Erze: Kohle (Höhe 5 bis 100), Eisen (bis Höhe 64) und Diamant (tief unten, selten), in Adern im Stein
- Ofen (8 Bruchstein im Ring): Eingabe, Brennstoff (Kohle 80 s, Holz und Bretter 15 s, Stöcke 5 s), Flamme und Fortschrittspfeil, 10 s je Gegenstand, leuchtet beim Brennen und arbeitet auch bei geschlossenem Fenster weiter. Schmelzen: Eisenerz → Eisenbarren, Sand → Glas, Bruchstein → Stein, rohes → gebratenes Fleisch (heilt 8 statt 4). Der Inhalt bleibt im Spielstand und fällt beim Abbauen heraus
- Crafting: 2x2-Raster im Inventar, 3x3 an der Werkbank (Rechtsklick auf den Block, mit Umschalt wird er stattdessen gesetzt). Rezepte: Holz → 4 Bretter, 2 Bretter → 4 Stöcke, 4 Bretter → Werkbank, 8 Bruchstein → Ofen, Spitzhacke/Axt/Schaufel/Schwert aus Brettern, Bruchstein, Eisenbarren oder Diamanten mit Stöcken
- Werkzeuge aus Holz, Stein, Eisen und Diamant mit Haltbarkeit (60, 132, 250, 1560 Nutzungen): passende Werkzeuge bauen schneller ab. Mindeststufe zum Ernten: Stein und Kohle Holz, Eisenerz Stein, Diamanterz Eisen, Obsidian Diamant. Ohne passendes Werkzeug dauert der Abbau länger und es fällt nichts. Schwerter und Äxte machen mehr Schaden gegen Mobs
- Mobs: Schweine, Kühe und Schafe laufen herum und fliehen bei Treffern, lassen rohes Fleisch fallen (essen heilt). Zombies erscheinen nur nachts (draußen und in dunklen Höhlen), verfolgen und greifen an und verschwinden am Tag (im Freien verbrennen sie, in Höhlen lösen sie sich nach 10 s auf)
- Ansichten (V): Ego, von hinten, von vorn. In den Außenansichten sieht man das eigene Spielermodell (Quader mit Laufanimation und Kopfneigung). Die Kamera rückt heran, wenn Blöcke im Weg sind, und steckt nie im Gelände
- Figuren-Texturen als aufgeklappte Netze (Skins, 64x32 bzw. 64x64): Spieler im Minecraft-Layout, dazu Schwein, Kuh, Schaf und Zombie
- Ego-Perspektive mit AABB-Kollision, Springen, Sprinten, Fliegen (Kreativ)
- Tag-Nacht-Zyklus (10 Minuten) mit Sonne, Mond und Sternen, Nebel, Unterwasser-Effekt
- Ambient Occlusion und einfaches Himmelslicht (Höhlen und Überhänge sind dunkler)
- Pause-Menü (Esc) und Einstellungsmenü: Sichtweite, Mausempfindlichkeit, Sichtfeld, VSync, Vollbild, Spielmodus. Die Werte stehen in `settings.cfg` und gelten beim nächsten Start
- Speichern und Laden: Seed, Position, Hotbar, Tageszeit und alle vom Spieler veränderten Chunks
- Screenshots als BMP

Bewusst nicht enthalten: Hunger, Rüstung, Gold, Sound, Mehrspieler.

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
| Rechtsklick (halten) | Block setzen, Fleisch essen, Werkbank und Ofen öffnen |
| Mittelklick | Block unter dem Fadenkreuz übernehmen (Kreativ: in den aktiven Slot, sonst Slot wählen) |
| 1 bis 9, Mausrad | Hotbar-Slot wählen |
| E | Inventar öffnen/schließen |
| Q | Einen Gegenstand aus dem aktiven Slot werfen (Strg+Q: ganzer Stapel) |
| G | Zwischen Survival und Kreativ wechseln |
| V | Ansicht wechseln: Ego, von hinten, von vorn |
| T | Zwischen Tag und Nacht springen |
| F1 | HUD ein/aus |
| F2 | Screenshot nach `screenshots/` |
| F5 | Speichern (zusätzlich alle 90 s und beim Beenden) |
| F11 | Vollbild |
| Esc | Inventar schließen, sonst Pause-Menü (dort "Weiter", "Einstellungen", "Spiel speichern", "Beenden"); in den Einstellungen geht Esc einen Schritt zurück |

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
--settings PFAD    Einstellungsdatei (Standard: settings.cfg)
```

Entwicklungsoptionen: `--screenshot DATEI --frames N` (Screenshot nach N Frames, dann beenden), `--pos X Y Z`, `--look YAW PITCH`, `--time T` (0 bis 1, 0.25 ist Mittag), `--fly`, `--view 0|1|2`, `--health N`, `--demo mobs|water|inv|craft|furnace|menu|settings` (Szenen für Screenshots), `--bench N` (automatischer Flug, gibt Frame-Zeiten aus), `--selftest` (prüft Abbauen, Setzen, Drops, Essen, Schaden, Fallschaden, Ertrinken, Tod, Inventar-Klicks, Speichern in einem echten Fenster).

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
| `src/Models.*`, `src/Skins.*`, `src/Camera.*` | Figurenmodelle und Skin-Netze, Standard-Skins, Kamera mit Geländekollision |
| `src/Player.*`, `src/Physics.*` | Bewegung und Kollision (auch für Mobs und Gegenstände) |
| `src/Entities.*` | Mobs (KI, Spawnen, Modelle aus Quadern) und Gegenstände am Boden |
| `src/Items.*`, `src/Inventory.*`, `src/Crafting.*`, `src/Furnace.h` | Gegenstände und Werkzeuge, Stapel, Klicklogik, Rezepte, Schmelzen, Ofenzustand |
| `src/Menu.*`, `src/Settings.*` | Pause- und Einstellungsmenü, `settings.cfg` |
| `src/Hud.*`, `src/GuiTexture.*` | HUD, Inventarfenster, Schrift (5x7-Bitmap) und Symbole |
| `src/Save.*` | Speicherformat Version 4 (RLE je veränderter Chunk, Inventar mit Werkzeug-Abnutzung, Öfen, Gesundheit, Modus; Versionen 1 bis 3 bleiben lesbar) |
| `tests/` | Tests |

## Geplant

Noch nicht umgesetzt, in dieser Reihenfolge der Nennung:

1. **Sound:** Effekte und Musik. Vorher wird die Audio-Bibliothek geklärt.
2. **Mehrspieler:** Host im Spiel, Beitreten per IP und Port. Vorher wird die Netzwerkbibliothek geklärt.
3. **Textur-Editor:** Im Spiel erreichbar über das Einstellungsmenü. Damit lassen sich Texturen von Blöcken und Charakteren ändern und speichern. Beim Start lädt das Spiel gespeicherte Texturen statt der prozeduralen. Das Einstellungsmenü dafür gibt es bereits.
4. **Fahrzeuge:** Flugzeuge, Schiffe, Autos, Zeppeline usw.

## Drittanbieter

- [GLFW](https://www.glfw.org/) 3.4 (zlib/libpng-Lizenz)
- [GLM](https://github.com/g-truc/glm) 1.0.1 (MIT)
- GLAD 2, erzeugt für OpenGL 3.3 Core, liegt in `third_party/glad` (SPDX laut Header: WTFPL OR CC0-1.0, AND Apache-2.0)
