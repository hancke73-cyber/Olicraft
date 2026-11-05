# OliCraft

Ein Minecraft-Klon in C++17 mit OpenGL 3.3, GLFW und GLM. Alle Texturen werden beim Start prozedural erzeugt, es gibt keine externen Assets.

## Funktionen

- Unendliche, chunkbasierte Welt (16 x 128 x 16), Gelände aus Perlin-Rauschen: Ozeane, Strände, Hügel, Berge mit Schneekappen, Wüsten, Wälder, Höhlen
- Bäume (über Chunkgrenzen hinweg konsistent), Meer mit halbtransparentem Wasser, Schwimmen, an Uferkanten mit der Sprungtaste aus dem Wasser klettern
- Fließendes Wasser: Quellen breiten sich bis 7 Blöcke aus, fließen bergab, bilden Wasserfälle, zwei Quellen ergeben eine neue (unendliche Quelle), Wasser zieht sich zurück, wenn die Quelle fehlt
- Lava: Lava-Seen in tiefen Höhlen (bis Höhe 10), fließt langsamer (1,5 s je Schritt) und nur 3 Blöcke weit, verursacht Schaden und Brennen (Wasser löscht), leuchtet, vernichtet Gegenstände und verletzt Mobs. Trifft Wasser auf eine Lavaquelle, entsteht Obsidian, auf fließende Lava Bruchstein
- Abbauen und Setzen von Blöcken per Raycast
- Survival- und Kreativmodus (Umschalten mit G): Abbauzeit je Block, Drops als Gegenstände am Boden, Gesundheit mit Herzen, Fallschaden, Ertrinken (Luftblasen), Regeneration, Tod und Wiedergeburt
- Inventar (E): 27 Rucksack-Slots plus 9 Hotbar-Slots, Stapel bis 64, Mausbedienung wie in Minecraft (links ganzen Stapel, rechts Hälfte oder eines, Umschalt-Klick verschiebt). Im Kreativmodus zusätzlich die Auswahl aller Gegenstände in Reitern nach Themen (Gelände, Baumaterial, Dekoration, Pflanzen, Stationen, Werkzeuge, Rohstoffe, Essen) mit 9 x 5 Feldern, Mausrad und Leiste scrollen; der Reiter mit der Lupe ist eine Suche (Name oder Teil davon, ohne Groß/Kleinschreibung, Umlaute als ae, oe, ue; Esc schließt, E ist im Suchfeld ein Buchstabe). Die Inventarfenster sind größer skaliert als die übrige Oberfläche (etwa 1,5-fach, soweit sie ins Fenster passen)
- Erze: Kohle (Höhe 5 bis 100), Eisen (bis Höhe 64), Gold (bis Höhe 30, selten) und Diamant (tief unten, am seltensten), in Adern im Stein
- Ofen (8 Bruchstein im Ring): Eingabe, Brennstoff (Kohle 80 s, Holz und Bretter 15 s, Stöcke 5 s), Flamme und Fortschrittspfeil, 10 s je Gegenstand, leuchtet beim Brennen und arbeitet auch bei geschlossenem Fenster weiter. Schmelzen: Tonklumpen → Ziegel, Kaktus → grüne Farbe, Holz → Kohle, Eisenerz → Eisenbarren, Golderz → Goldbarren, Sand → Glas, Bruchstein → Stein, rohes → gebratenes Fleisch (heilt 8 statt 4). Der Inhalt bleibt im Spielstand und fällt beim Abbauen heraus
- Bausteine: Ziegel, Steinziegel, Sandstein, Kies, Ton, Wolle in 6 Farben, Bücherregal, Eisen-, Gold-, Diamant- und Kohleblöcke, Lampe (Licht 15), Kürbis, Kaktus (verletzt bei Berührung), Birken- und Fichtenholz mit eigenem Laub. Die Blöcke sind im Kreativ-Inventar nach Gruppen sortiert und im Textur-Editor änderbar
- Pflanzen (kreuzförmig, begehbar, brechen sofort): hohes Gras, Farn, vier Blumen (Mohn, Löwenzahn, Kornblume, Gänseblümchen), rote und braune Pilze, Toter Busch, Zuckerrohr (am Wasser), Beerensträucher (Beeren heilen 2). Sie fallen ab, wenn der Boden fehlt. Gras, Farn und Toter Busch werden beim Setzen von Blöcken einfach ersetzt. Das Gelände hat mehr Vielfalt: Birkenwälder, Fichtenwälder in der Höhe und Kälte, Blumenwiesen, Pilze und Farn im Wald, Kakteen und Tote Büsche in der Wüste, vereinzelt Kürbisse
- Stationen: Truhe (27 Plätze, Rechtsklick, Umschalt-Klick verschiebt, Inhalt bleibt im Spielstand und fällt beim Abbauen heraus) und Bett (Rechtsklick setzt den Wiedereinstiegspunkt, nachts überspringt es die Nacht, wenn keine Monster in 10 Blöcken sind)
- Bauteile mit Teilformen: Treppen und Platten in 5 Materialien (Bretter, Bruchstein, Steinziegel, Ziegel, Sandstein), Zäune, Mauern (Bruchstein, Ziegel), Glasscheiben, Eisengitter (verbinden sich automatisch mit Nachbarn), Laternen (stehend oder hängend, Licht 15), Holz- und Eisentüren (zwei Blöcke hoch, Rechtsklick öffnet), Holz- und Eisentore. Treppen zeigen mit der hohen Seite in Blickrichtung, Platten und Treppen sitzen unten oder oben (an der Unterseite eines Blocks oder in der oberen Hälfte einer Wand gesetzt steht die Treppe umgedreht), zwei Platten ergeben einen Vollblock. Dachkeile sind glatte Schrägen über die ganze Zelle (5 Materialien, 4 Richtungen, hohe Seite in Blickrichtung; Rezept: 3 Blöcke in L-Form ergeben 4 Keile). Die Kollision besteht aus vier Stufen zu je 0,25, man läuft sie ohne Sprung hinauf. Treppenecken entstehen wie in Minecraft von selbst: steht vor einer Treppe eine gleichartige quer dazu, wird es eine Außenecke, steht eine hinter ihr, eine Innenecke (aus den Nachbarn berechnet, nichts davon wird im Block gespeichert, Kollision und Auswahl folgen der Ecke). Kollision, Strahl und Auswahl folgen der echten Form: man steht auf Platten, steigt Stufen ohne Sprung hinauf (Kanten bis 0,6), Zäune und Tore sind 1,5 hoch (der Sprung reicht nicht darüber)
- Große Tore: Tore sind einzelne Blöcke, nebeneinander und übereinander gesetzte Tore öffnen und schließen gemeinsam. So lassen sich beliebig große Tore bauen (z. B. 3 x 3). Auch nebeneinander stehende Türen (Doppeltür) öffnen gemeinsam
- Landwirtschaft: Hacken (Holz bis Diamant) machen aus Gras oder Erde Acker, Samen (aus hohem Gras) und Karotten werden darauf gepflanzt, Weizen und Karotten wachsen in 3 Stufen (mit Wasser im Umkreis von 4 Blöcken doppelt so schnell), reif geerntet geben sie Weizen und mehr Samen bzw. Karotten. Brot (3 Weizen) heilt 5, Karotten 3. Ohne Acker brechen die Pflanzen ab
- Dörfer: je etwa 160-Block-Zelle gibt es mit gewisser Wahrscheinlichkeit ein Dorf auf ebenem, trockenem Gelände (nicht im Schnee). Brunnen in der Mitte, bis zu 10 Gebäude im Kreis: Wohnhäuser (Bett, Werkbank), Bibliothek, Schmiede, Felder mit Zaun, Tor und Wasserrinne (Weizen und Karotten), Scheune mit großem Tor und Tieren, Marktstand mit Markise, Wachturm mit Eisentür und Gittern, in zwei von drei Dörfern eine Kirche (11 x 7, Steinziegelwände mit hohen Fenstern, roter Mittelgang, Bänke aus Treppen, Altar mit Truhe und Kreuzfenster, Walmdach und vorn ein Glockenturm mit offener Glockenstube und Spitze; sie wird nach allen anderen Gebäuden gesetzt, falls Platz bleibt), am Rand ein Dorftor mit großem Tor und Laternen. Häuser haben Fundamente, Holztüren, Glasscheiben, Dächer aus Treppen und hängende Laternen, Kieswege führen zu jeder Tür, Laternenpfähle säumen die Wege. Truhen im Haus enthalten beim ersten Öffnen Beute (Kohle, Eisen, Gold, Essen, Bücher, selten Diamant). Um das Dorf wachsen keine Bäume. Mit `--village` springt man beim Start zum nächsten Dorf
- Dorfbewohner: pro Haus einer, bleiben im Dorf, haben einen Beruf (Bauer, Bibliothekar, Schmied, Steinmetz, Schäfer) mit eigenem Aussehen und Arbeitsplatz (Feld, Bibliothek, Schmiede, Marktstand, Scheune). Tagesablauf: tagsüber am Arbeitsplatz, abends nach Hause, nachts im Bett (liegend), morgens wieder auf. Sie finden ihren Weg per Pfadsuche, öffnen Türen und Tore und schwimmen aus dem Wasser ans Ufer. Rechtsklick öffnet das Handelsfenster mit 5 Angeboten, Währung sind Goldbarren: sie kaufen Rohstoffe (z. B. 15 Kohle → 1 Gold) und verkaufen Waren (z. B. 3 Gold → Eisenspitzhacke, 9 Gold → Diamant). Im Kreativmodus ist Handeln kostenlos. Sie zählen nicht zum Tierlimit
- Schilder (6 Bretter und 1 Stock ergeben 3): von oben auf einen Block gesetzt steht ein Stehschild mit der Vorderseite zum Spieler, an eine Seitenfläche gesetzt hängt ein Wandschild. Beim Setzen öffnet sich das Schreibfenster (4 Zeilen zu 15 Zeichen, Enter oder Pfeile wechseln die Zeile, Umlaute werden zu ae, oe, ue, Esc übernimmt den Text), Rechtsklick auf das Schild öffnet es wieder. Der Text steht im Spielstand (Version 7) und wird eingeblendet, wenn man das Schild ansieht. Auf dem Brett in der Welt selbst wird er nicht gezeichnet. Schilder haben keine Kollision, fallen ohne Boden bzw. Wand ab und nehmen ihren Text beim Abbauen mit
- Eimer (3 Eisenbarren in V-Form): Rechtsklick auf eine Wasser- oder Lavaquelle nimmt sie auf (fließendes Wasser nicht), ein gefüllter Eimer setzt an der angeklickten Stelle eine Quelle, die danach wie gewohnt fließt. Leere Eimer stapeln bis 16, gefüllte einzeln
- Dampfmaschinen (Steampunk): Dampfkessel (Ring aus 8 Eisenbarren um einen Ofen), Rohr (3 Eisenbarren in einer Reihe ergeben 6), Welle (3 Eisenbarren übereinander ergeben 6), Motor, Destillationsapparat, Bohrmaschine, Förderband und Dampflift. Zwei getrennte Netze, beide aus den Nachbarblöcken berechnet: Der **Dampf** fließt vom Kessel durch Rohre und Maschinen (Kessel, Rohr, Motor, Destille berühren sich) zu den Verbrauchern, der **Antrieb** läuft vom Motor über Wellen zu Bohrmaschine, Förderband und Lift (Motor, Welle, Bohrer, Band, Lift berühren sich). Ohne laufenden Motor steht alles still, bei zu wenig Dampf oder Antrieb arbeiten die Maschinen entsprechend langsamer. Der Kessel zündet nur, wenn jemand Dampf braucht, und braucht dazu Wasser (Wassereimer ins obere Feld, 1000 ml je Eimer, Tank 2000 ml, destilliertes Wasser hält doppelt so lange) und Brennstoff (Kohle und Holz 1,5-fach, Teer 160 s, Heizöl 360 s). Das Fenster zeigt Tank und Last. Die Bohrmaschine (Motor, Eisenspitzhacke) steht in Blickrichtung, bei steilem Blick nach oben oder unten, bricht den nächsten abbaubaren Block bis 16 Blöcke vor sich (Truhe, Ofen, Maschinen, Obsidian und Grundgestein bleiben stehen) und legt die Beute in eine angrenzende Truhe, sonst fällt sie heraus. Das Förderband (3 Eisenbarren über 3 Stöcken ergeben 6) trägt Spieler, Gegenstände und Mobs in Blickrichtung, der Dampflift (Eisen und Glas) hebt sie in senkrechter Säule nach oben, Umschalt fährt nach unten, kein Fallschaden. Die Destille braucht Dampf und Eingabe: Kohle oder Holz → Teer (6 s), Teer → Heizöl (8 s), Wassereimer → destillierter Eimer plus Salz (10 s). Ausgaben nur entnehmen. Kessel und Destille behalten ihren Inhalt im Spielstand und lassen ihn beim Abbauen fallen. Maschinen in nicht geladenen Chunks ruhen. Dampf aus Kessel, Motor und Destille ist als Partikel zu sehen, neue Töne gibt es dafür nicht
- Steampunk-Erweiterung ein- und ausschaltbar (Einstellungen, "Steampunk: An/Aus", gespeichert als `steampunk=` in `settings.cfg`). Aus: alle Steampunk-Blöcke, Gegenstände und Rezepte verschwinden aus Kreativ-Inventar, Suche und Rezeptbuch, die Maschinen ruhen (kein Dampf, Antrieb, Band, Lift, keine Pumpe), Fahrzeuge stehen still und lassen sich nicht besteigen, Golems stehen nur herum, in Dörfern erscheinen keine neuen. Schon gesetzte Teile bleiben in der Welt und laufen nach dem Einschalten weiter
- Wasserleitung: Dampfpumpe (Rezept: 7 Eisenbarren, Wasserrohr, Eimer), Wasserrohre (6 Eisenbarren in zwei Spalten ergeben 6; eigener Blocktyp, verbindet sich nur mit Pumpe, Auslass und Kesseln, nicht mit Dampfrohren) und Wasserauslass (3 Eisenbarren über einem Wasserrohr). Die Pumpe hängt am Antriebsnetz (Welle zum Motor, 1 Antrieb) und saugt aus einer Wasserquelle direkt neben sich. Ein Meer versiegt nicht (Quelle mit zwei waagerechten Quellnachbarn), eine einzelne Quelle wird nach 1000 ml abgepumpt. Fördermenge 60 ml/s bei voller Versorgung. Wasser geht zuerst in Dampfkessel, die an der Leitung hängen (Tank bis 2000 ml), danach setzt jeder Auslass bei 250 ml eine Wasserquelle in den Block unter sich. Die Pumpe arbeitet nur, wenn jemand Wasser braucht
- Dampfboot (Rezept: Dampfkessel, 2 Eisenbarren, 5 Bretter): auf eine Wasserfläche setzen, Rechtsklick steigt ein, W und S Gas, A und D lenken, F aussteigen, E öffnet den Bordkessel (Brennstoff, Wassereimer, gleiche Regeln wie beim Kessel), Umschalt plus Rechtsklick öffnet ihn von außen. Der Bordkessel liefert nur Dampf, wenn jemand fährt. Das Boot bleibt im Wasser und stoppt am Ufer. Linksklick beschädigt es (20 Lebenspunkte), es fällt in Teilen heraus
- Luftschiff: Gondel (Bretter, Glas, Eisenbarren) auf festen Boden setzen, dann mit dem Bauteil in der Hand Rechtsklick: Ballon (klein: 6 Wolle und 2 Stöcke; mittel: kleiner Ballon plus 8 Wolle; groß: mittlerer plus 8 Wolle), Dampfpropeller (Kessel, Motor, Eisenbarren), weitere Gondeln (hintereinander gereiht, höchstens drei). Der kleine Ballon trägt eine Gondel, der mittlere zwei, der große drei. Es fliegt nur mit Ballon, Propeller und passender Ballongröße: W und S Gas, A und D lenken, Leertaste steigt, Umschalt sinkt, F aussteigt (dann hält ein Seil mit Anker es fest). Steigen und Gas brauchen Dampf, ohne Dampf sinkt es langsam. Kollision nur mit einer schmalen Säule, die Enden des Ballons gehen durch Bäume
- Dampfgolem: aus einem Eisenblock (Beine), einem Dampfkessel darauf (Rumpf), Eisenblöcken links und rechts vom Kessel (Arme) und einer Lampe oben (Kopf), in einer Richtung (Arme entlang x oder z); mit dem zuletzt gesetzten Teil erwacht er und die fünf Blöcke verschwinden. Er greift Zombies in 18 Blöcken an (auch solche, die den Spieler bedrängen), der selbstgebaute Golem folgt dem Spieler, Dorfwächter (einer je Dorf) bleiben im Dorf. 60 Lebenspunkte, Beute Eisenbarren und Kohle, kein Brennstoffbedarf. Selbstgebaute Golems stehen im Spielstand, die Dorfwächter entstehen mit dem Dorf neu
- Rezeptbuch neben dem Inventar und der Werkbank: alle Rezepte, herstellbare zuerst und grün umrandet, Hinweis mit den Zutaten, Klick füllt das Raster aus dem Rucksack, Filter "Nur machbare", Mausrad scrollt. Rezepte für 3x3 sind im Inventar abgeblendet
- Crafting: 2x2-Raster im Inventar, 3x3 an der Werkbank (Rechtsklick auf den Block, mit Umschalt wird er stattdessen gesetzt). Rezepte: Holz → 4 Bretter, 2 Bretter → 4 Stöcke, 4 Bretter → Werkbank, 8 Bruchstein → Ofen, 8 Bretter → Truhe, Wolle und Bretter → Bett, Bücherregal, Lampe, Ziegel, Steinziegel, Sandstein, Materialblöcke (und zurück zu 9 Barren), Papier und Bücher, Farben aus Blumen und Kohle, gefärbte Wolle (formlos: Wolle plus Farbe), Spitzhacke/Axt/Schaufel/Schwert aus Brettern, Bruchstein, Eisenbarren, Goldbarren oder Diamanten mit Stöcken, 1 Kohle über 1 Stock → 4 Fackeln
- Werkzeuge aus Holz, Stein, Eisen, Gold und Diamant mit Haltbarkeit (60, 132, 250, 33, 1560 Nutzungen): passende Werkzeuge bauen schneller ab, Gold am schnellsten, bricht aber schnell. Mindeststufe zum Ernten: Stein und Kohle Holz, Eisenerz Stein, Gold- und Diamanterz Eisen, Obsidian Diamant (Goldwerkzeuge zählen als Holzstufe). Ohne passendes Werkzeug dauert der Abbau länger und es fällt nichts. Schwerter und Äxte machen mehr Schaden gegen Mobs
- Mobs: Schweine, Kühe und Schafe laufen herum und fliehen bei Treffern, lassen rohes Fleisch fallen (essen heilt). Schafe lassen auch Wolle fallen. Zombies erscheinen nur nachts (draußen und in dunklen Höhlen), verfolgen und greifen an und verschwinden am Tag (im Freien verbrennen sie, in Höhlen lösen sie sich nach 10 s auf)
- Textur-Editor (Esc, Einstellungen, Textur-Editor): zeigt die aufgeklappte Textur (bei Blöcken das Kreuznetz aus Oberseite, vier Seiten und Unterseite, bei Figuren das komplette Skin-Netzblatt mit beschrifteten Flächen: V vorn, H hinten, R rechts, L links, O oben, U unten) und daneben eine drehbare 3D-Vorschau mit der echten Textur. Bemalen mit Stift (Pinselgröße 1 bis 3), Radierer, Füllen (ähnliche Farben), Pipette (auch Rechtsklick), 32 Farbfelder plus RGBA-Schieber, Rückgängig/Wiederholen, Raster und Netz einblendbar. Texturen von Blöcken, Gegenständen (Werkzeuge, Erze usw.) und Figuren (Spieler, Schwein, Kuh, Schaf, Zombie, Dorfbewohner) lassen sich ändern
- Eigene Texturen als PNG: "Speichern" schreibt nach `textures/`, beim nächsten Start lädt das Spiel diese Dateien statt der prozeduralen Texturen. "Exportieren" schreibt das aufgeklappte Netz nach `textures/export/`, das lässt sich in einem Malprogramm bearbeiten und mit "Importieren" wieder einlesen (danach noch speichern). "Zurücksetzen" stellt die Standardtextur wieder her und löscht die gespeicherte Datei
- Ansichten (V): Ego, von hinten, von vorn. In den Außenansichten sieht man das eigene Spielermodell (Quader mit Laufanimation und Kopfneigung). Die Kamera rückt heran, wenn Blöcke im Weg sind, und steckt nie im Gelände
- Spieler-Skins: 8 Plätze mit eigenen Standardfarben (Standard, Rot, Gruen, Violett, Orange, Hell, Dunkel, Rosa). Im Einstellungsmenü wechselt der Knopf "Spieler-Skin" den Platz, daneben dreht sich das Modell. Der Textur-Editor bearbeitet den gewählten Platz (Titel zeigt den Platz). Dateien: `textures/skin_player.png` (Platz 1), `skin_player_2.png` bis `skin_player_8.png`. Eigene Skins lassen sich auch einfach dort hineinkopieren (64x32 oder 64x64)
- Feindliche Mobs (Zombies) treiben nicht im Wasser: sie sinken zum Grund und gehen dort, Tiere schwimmen oben
- Figuren-Texturen als aufgeklappte Netze (Skins, 64x32 bzw. 64x64): Spieler im Minecraft-Layout, dazu Schwein, Kuh, Schaf und Zombie
- Leitern (4 Ausrichtungen, hängen an einer festen Wand, fallen ab ohne Träger): Hochklettern mit Leertaste oder Vorwärts, Umschalt hält an, sonst langsames Abrutschen, kein Fallschaden an der Leiter
- Betten sind zweiteilig (Fuß und Kopf), Rechtsklick wie bisher. Dorfhäuser haben je zwei Betten
- Rüstung in vier Materialien (Leder von Kühen, Eisen, Gold, Diamant) und vier Teilen (Helm, Brustpanzer, Hose, Stiefel). Eigene Rüstungsplätze im Survival-Inventar neben einer Spielerfigur, an der Werkbank links vom 3x3-Raster und im Kreativ-Inventar als Reihe unter dem Auswahlfeld (mit Anzeige der Schutzpunkte), Rechtsklick mit dem Teil in der Hand oder Umschalt-Klick im Inventar legt es an, Schaden wird reduziert (volle Diamantrüstung 80 %), Teile nutzen sich ab, getragene Rüstung ist am Spielermodell sichtbar. Der Schmied im Dorf verkauft Eisenrüstung
- Ego-Perspektive mit AABB-Kollision, Springen, Sprinten, Fliegen (Kreativ)
- Tag-Nacht-Zyklus (10 Minuten): Himmelsverlauf mit Abendrot und Glanz um die Sonne, Sonne mit Strahlen (tief am Horizont größer und orange), Mond mit 8 Phasen und Kratern (wechselt jede Nacht), funkelnde Sterne in drei Größen und leichten Farbtönen, Nebel, Unterwasser-Effekt
- Ambient Occlusion und einfaches Himmelslicht (Höhlen und Überhänge sind dunkler)
- Blocklicht (0 bis 15, Ausbreitung wie in Minecraft, warmes Licht): Fackeln (Stufe 14), Lava (15) und brennende Öfen (13) beleuchten ihre Umgebung, Licht läuft über Chunkgrenzen und wird beim Entfernen korrekt nachgefüllt. Fackeln stehen auf dem Boden oder hängen an Wänden (nie an der Decke), fallen ab, wenn der Träger fehlt, brechen sofort und halten Zombies fern (sie erscheinen nur bei Licht 6 oder weniger)
- Hauptmenü beim Start: Titel "OliCraft" in großer Schrift, dahinter dreht sich langsam eine Beispielwelt. Darunter die Weltliste (neueste zuerst, Mausrad, Pfeiltasten, Doppelklick oder Enter startet) mit den Knöpfen Spielen, Neue Welt, Einstellungen, Beenden. Jede Welt ist eine eigene Datei `saves/<Name>.dat`, die alte `saves/world.dat` erscheint als Welt "world". Neue Welt: Name (nur Buchstaben, Ziffern, Leerzeichen, - und _, höchstens 24 Zeichen, doppelte Namen bekommen " 2"), Seed (leer ist zufällig) und Spielmodus. Das Pause-Menü hat "Hauptmenü" (speichert die Welt). Mit `--no-menu` startet das Spiel direkt
- Grafik (Einstellungen, Grafik): Schatten als Shadow-Map (aus, mittel, hoch), Wolken (zwei Schichten, ziehen mit dem Wind), animiertes Wasser (leichte Wellen und fließende Textur), Partikel (Block-Bruchstücke beim Abbauen, Spritzer, Rauch und Flammen an Fackeln und Öfen, Funken über Lava, Blut und Staub bei Mobs), Texturfilter (Pixel scharf oder weich) und anisotrope Filterung (1x bis 16x, nur wenn die Grafikkarte sie bietet)
- Pause-Menü (Esc) und Einstellungsmenü: Sichtweite, Mausempfindlichkeit, Sichtfeld, drei Lautstärkeregler, GUI-Größe (Auto oder 1x bis 4x für HUD, Menüs und Inventar; wird bei kleinen Fenstern begrenzt), VSync, Vollbild, Spielmodus, Textur-Editor. Die Werte stehen in `settings.cfg` und gelten beim nächsten Start
- Speichern und Laden je Welt: Seed, Position, Hotbar, Tageszeit und alle vom Spieler veränderten Chunks
- Screenshots als BMP

Texturformate: Kacheln sind 16x16 (`textures/block_<name>.png`), die Skins `skin_player.png` und `skin_zombie.png` 64x32 im Minecraft-Layout (Kopf 0,0, Körper 16,16, Arm 40,16, Bein 0,16; linke Gliedmaßen werden gespiegelt; Minecraft-Skins im Format 64x64 werden akzeptiert, die obere Hälfte zählt), `skin_pig.png`, `skin_cow.png` und `skin_sheep.png` 64x64. Dateien mit falscher Größe werden beim Start ignoriert und gemeldet. Der Block-Export ist ein 64x48-Kreuznetz (4 x 3 Kacheln).

- Ton (miniaudio): Schritte je Untergrund, Abbau- und Setzgeräusche je Material, Wasser, Lava, Essen, Aufheben, Herstellen, Schaden und Tod, Rufe von Schwein, Kuh, Schaf und Zombie (Richtung und Entfernung), Musik. Lautstärkeregler im Einstellungsmenü. Klangdateien liegen nicht bei: Dateinamen und Prompts stehen in [docs/SOUNDS.md](docs/SOUNDS.md), die Dateien kommen nach `sounds/`

Bewusst nicht enthalten: Hunger, Mehrspieler.

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
| Rechtsklick (halten) | Block oder Fackel setzen, Fleisch essen, Werkbank und Ofen öffnen |
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

Im Textur-Editor: Linksklick malt, Rechtsklick nimmt die Farbe auf, B Stift, E Radierer, F Füllen, P Pipette, 1 bis 3 Pinselgröße, G Raster, N Netz, R Drehen, Strg+Z rückgängig, Strg+Y oder Strg+Umschalt+Z wiederholen, Strg+S speichern, Mausrad in der Liste scrollt, Esc geht zurück.

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
--no-menu          Ohne Hauptmenü direkt ins Spiel
--save-path PFAD   Speicherdatei für --no-menu und die Entwicklungsoptionen (Standard: saves/world.dat)
--saves-dir PFAD   Ordner der Welten im Hauptmenü (Standard: saves)
--settings PFAD    Einstellungsdatei (Standard: settings.cfg)
--textures PFAD    Ordner mit eigenen Texturen (Standard: textures)
--sounds PFAD      Ordner mit Klangdateien (Standard: sounds, Musik in sounds/music)
--no-sound         Ton aus
--list-sounds      Benötigte Klangdateien als Tabelle ausgeben
```

Entwicklungsoptionen: `--screenshot DATEI --frames N` (Screenshot nach N Frames, dann beenden), `--pos X Y Z`, `--look YAW PITCH`, `--time T` (0 bis 1, 0.25 ist Mittag), `--fly`, `--view 0|1|2`, `--health N`, `--moon N` (feste Mondphase 0 bis 7), `--village` (zum nächsten Dorf springen), `--demo mobs|water|torch|inv|craft|chest|trade|village|parts|armor|armorinv|furnace|menu|mainmenu|settings|editor|editorblock|editorpig|editoritem` (Szenen für Screenshots), `--bench N` (automatischer Flug, gibt Frame-Zeiten aus), `--selftest` (prüft Abbauen, Setzen, Drops, Essen, Schaden, Fallschaden, Ertrinken, Tod, Inventar-Klicks, Speichern in einem echten Fenster).

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
| `src/Audio.*`, `src/Sounds.*` | Tonausgabe (miniaudio), Klangereignisse, Dateinamen und Prompts |
| `src/Menu.*`, `src/Settings.*` | Pause- und Einstellungsmenü, `settings.cfg` |
| `src/TextureStore.*`, `src/TextureEditor.*`, `src/EditorUi.*`, `src/Image.*` | Texturspeicher mit PNG-Dateien, Editor-Logik (Werkzeuge, Rückgängig, Netze, Vorschau), Editor-Oberfläche |
| `src/Hud.*`, `src/GuiTexture.*` | HUD, Inventarfenster, Schrift (5x7-Bitmap) und Symbole |
| `src/Worlds.*` | Weltliste (Dateien im Ordner), Namen prüfen, eindeutige Namen |
| `src/Save.*` | Speicherformat Version 9 (RLE je veränderter Chunk, Inventar mit Werkzeug-Abnutzung, Rüstung, Öfen, Truhen, Schilder, Dampfmaschinen, Fahrzeuge, gebaute Golems, Wiedereinstiegspunkt, Gesundheit, Modus; ältere Versionen bleiben lesbar) |
| `src/Machines.*`, `src/Pipeline.cpp` | Dampfmaschinen (Kessel, Destille, Motor, Bohrer, Band, Lift, Netzberechnung in festen 0,1-s-Schritten), Wasserleitung mit Pumpe |
| `src/Vehicles.*`, `src/GameVehicles.cpp`, `src/EntityBox.h` | Dampfboot und Luftschiff (Bewegung, Bordkessel, Darstellung aus Atlasquadern), Einsteigen und Bedienung im Spiel |
| `src/GameGolems.cpp` | Bau von Dampfgolems aus Blöcken (die KI steht in `Entities.cpp`) |
| `src/GameSelfTest.cpp`, `src/GameDemo.cpp` | Selbsttest (`--selftest`) und Szenen für Screenshots (`--demo`) |
| `src/BlockShape.h` | Boxen der Teilformen (Kollision, Strahl, Darstellung) |
| `src/Village.*`, `src/Trades.*` | Dörfer (Lage, Gebäude, Wege, Beute) und Handelsangebote der Dorfbewohner |
| `src/Sky.*` | Pixelgrafiken für Sonne, Mondphasen, Lichtkranz und die Sterne |
| `src/Particles.*` | Partikelsystem (Billboards) |
| `src/Chest.h` | Truheninhalt |
| `tests/` | Tests |

## Geplant

Noch nicht umgesetzt, in dieser Reihenfolge der Nennung:

1. **Steampunk, Fortsetzung:** U-Boot, Lagerplätze in den Gondeln, Töne für Maschinen und Fahrzeuge, Arbeiter-Golems.
2. **Mehrspieler:** Host im Spiel, Beitreten per IP und Port. Vorher wird die Netzwerkbibliothek geklärt.
3. **Weitere Fahrzeuge:** Autos, Flugzeuge.
4. **Dörfer ausbauen:** Zombie-Überfälle.
5. **Text auf dem Schild in der Welt zeichnen** (bisher nur Anzeige am Fadenkreuz).

## Drittanbieter

- [GLFW](https://www.glfw.org/) 3.4 (zlib/libpng-Lizenz)
- [GLM](https://github.com/g-truc/glm) 1.0.1 (MIT)
- [stb_image und stb_image_write](https://github.com/nothings/stb) (gemeinfrei oder MIT) für PNG, liegen in `third_party/stb`
- [miniaudio](https://miniaud.io/) 0.11.26 (gemeinfrei oder MIT-0) für den Ton, liegt in `third_party/miniaudio`
- GLAD 2, erzeugt für OpenGL 3.3 Core, liegt in `third_party/glad` (SPDX laut Header: WTFPL OR CC0-1.0, AND Apache-2.0)
