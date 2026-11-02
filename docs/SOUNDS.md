# Klangdateien für OliCraft

Das Spiel bringt keine Klangdateien mit. Es spielt, was im Ordner `sounds/` liegt (oder im Ordner aus `--sounds PFAD`). Fehlt eine Datei, bleibt dieses Ereignis still. Fehlt der ganze Ordner, läuft das Spiel ohne Ton.

## Format

- Effekte: WAV, FLAC oder MP3. OGG wird nicht gelesen.
- Mehrere Varianten pro Ereignis: `step_grass.wav`, oder `step_grass_1.wav` bis `step_grass_8.wav`. Das Spiel wählt zufällig.
- Musik: beliebige WAV-, FLAC- oder MP3-Dateien in `sounds/music/`. Das erste Stück startet etwa 25 Sekunden nach Spielstart, danach folgen Pausen von 1 bis 3 Minuten.
- Lautstärke im Spiel: Esc, Einstellungen, Regler Gesamt, Effekte, Musik (gespeichert in `settings.cfg`).
- Kurze Effekte halten sich am besten bei der angegebenen Dauer, mit wenig Stille am Anfang und Ende.

## Liste der Effekte

Die Tabelle stammt aus dem Spiel selbst und lässt sich neu erzeugen mit `OliCraft --list-sounds`. Die Prompts sind Vorschläge für ein Text-zu-Audio-Modell (Annahme: Das gewählte Modell versteht englische Prompts am besten, das ist nicht geprüft).

| Datei (ohne Endung) | Beschreibung | Dauer | Prompt (englisch) |
|---|---|---|---|
| `block_stone` | Stein abbauen/setzen | 0.5 s | sharp chip of a pickaxe hitting stone, short dry rock crack, game sound effect |
| `block_dirt` | Erde abbauen/setzen | 0.5 s | dull soft thud of digging into dirt, short earthy crunch, game sound effect |
| `block_grass` | Gras und Laub | 0.5 s | short soft rustle of grass and leaves being cut, game sound effect |
| `block_sand` | Sand abbauen/setzen | 0.5 s | short dry scrape and soft hiss of sand being shovelled, game sound effect |
| `block_wood` | Holz abbauen/setzen | 0.5 s | hollow knock of an axe hitting wood, short wooden thunk, game sound effect |
| `block_glass` | Glas abbauen | 0.7 s | short bright shatter of glass breaking into pieces, game sound effect |
| `block_snow` | Schnee abbauen/setzen | 0.5 s | very soft muffled crunch of snow, short, game sound effect |
| `step_stone` | Schritt auf Stein | 0.3 s | single footstep on hard stone, short dry tap, game sound effect |
| `step_dirt` | Schritt auf Erde | 0.3 s | single footstep on soft dirt, muffled short thud, game sound effect |
| `step_grass` | Schritt auf Gras | 0.3 s | single footstep on grass, soft short swish, game sound effect |
| `step_sand` | Schritt auf Sand | 0.3 s | single footstep on dry sand, soft short scrunch, game sound effect |
| `step_wood` | Schritt auf Holz | 0.3 s | single footstep on wooden planks, short hollow tap, game sound effect |
| `step_snow` | Schritt auf Schnee | 0.4 s | single footstep in snow, soft short crunch, game sound effect |
| `water_splash` | Platschen beim Eintauchen | 0.9 s | short splash of a person jumping into water, game sound effect |
| `water_swim` | Schwimmen | 0.6 s | short gentle water paddling swish, game sound effect |
| `lava_pop` | Lava blubbert | 0.7 s | single thick bubble popping in hot lava, short bloop with hiss, game sound effect |
| `player_hurt` | Spieler verletzt | 0.5 s | short pained grunt of a man taking damage, oof, game sound effect |
| `player_death` | Spieler stirbt | 1.2 s | short dying groan of a man falling down, game sound effect |
| `eat` | Essen | 0.9 s | short crunchy bite and chew of eating food, game sound effect |
| `item_pickup` | Gegenstand aufheben | 0.2 s | short bright pop of picking up an item, tiny blip, game sound effect |
| `click` | Menue-Klick | 0.2 s | short soft click of a button, user interface, game sound effect |
| `craft` | Herstellen | 0.5 s | short satisfying tool crafting clink with a wooden tap, game sound effect |
| `fuse` | Ofen entzuendet | 1.0 s | short whoosh of a furnace igniting with a crackle, game sound effect |
| `pig_idle` | Schwein grunzt | 1.0 s | a pig oinking and grunting, short, cartoon farm animal, game sound effect |
| `pig_hurt` | Schwein verletzt | 0.8 s | a pig squealing in pain, short, game sound effect |
| `pig_death` | Schwein stirbt | 1.0 s | a pig squealing and dying, short, game sound effect |
| `cow_idle` | Kuh muht | 1.6 s | a cow mooing, one low moo, farm animal, game sound effect |
| `cow_hurt` | Kuh verletzt | 0.9 s | a cow bellowing in pain, short, game sound effect |
| `cow_death` | Kuh stirbt | 1.2 s | a cow mooing sadly and falling, short, game sound effect |
| `sheep_idle` | Schaf blökt | 1.0 s | a sheep bleating baa, short, farm animal, game sound effect |
| `sheep_hurt` | Schaf verletzt | 0.7 s | a sheep bleating in pain, short sharp baa, game sound effect |
| `sheep_death` | Schaf stirbt | 1.0 s | a sheep bleating weakly and dying, short, game sound effect |
| `zombie_idle` | Zombie stöhnt | 1.6 s | a zombie groaning, low raspy undead moan, horror game sound effect |
| `zombie_hurt` | Zombie verletzt | 0.7 s | a zombie grunting in pain, short raspy hiss and groan, game sound effect |
| `zombie_death` | Zombie stirbt | 1.4 s | a zombie dying, long raspy groan fading out, horror game sound effect |
| `zombie_attack` | Zombie greift an | 0.8 s | a zombie snarling and lunging, short aggressive growl, horror game sound effect |

## Musik

Vorschläge für Prompts (ebenfalls ungeprüft, Länge je nach Modell begrenzt, ggf. mehrere Stücke erzeugen):

- `calm ambient piano melody, soft, slow, minimalist, relaxing sandbox game background music, instrumental`
- `gentle ambient pads with sparse piano notes, peaceful, nature, instrumental`
- `dark ambient drone with distant echoes, cave exploration, tense but quiet, instrumental`

Beim Speichern in ComfyUI das Format FLAC oder MP3 wählen, dann nach `sounds/music/` kopieren.
