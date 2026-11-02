#pragma once
#include <string>
#include <vector>

#include "Items.h"

namespace oc {

// Formrezept: Muster aus Zeichen, ' ' = leer. Das Muster darf an beliebiger Stelle im Raster liegen und
// waagerecht gespiegelt sein.
struct Recipe {
    std::vector<std::string> rows;
    char symbols[8];       // Zeichen ...
    ItemId items[8];       // ... und zugehoerige Gegenstaende
    int symbolCount = 0;
    ItemStack result;
    // Formlose Rezepte: nur die Zutaten zaehlen (je ein Stueck), nicht ihre Lage
    bool shapeless = false;
    std::vector<ItemId> list;

    int height() const { return static_cast<int>(rows.size()); }
    int width() const { return rows.empty() ? 0 : static_cast<int>(rows[0].size()); }
    // Kleinste noetige Rastergroesse: 2 (Inventar) oder 3 (Werkbank)
    int gridNeeded() const {
        if (shapeless) return list.size() > 4 ? 3 : 2;
        return (height() > 2 || width() > 2) ? 3 : 2;
    }
};

const std::vector<Recipe>& recipes();
// Die Zutaten eines Rezepts als Liste mit Anzahl (z. B. fuer Anzeige und automatisches Befuellen)
struct Ingredient {
    ItemId item;
    int count;
};
std::vector<Ingredient> recipeIngredients(const Recipe& r);
// Belegung des Rasters (gridSize x gridSize) fuer das Rezept, Zeile fuer Zeile; formlos: von links oben nacheinander
std::vector<ItemId> recipeLayout(const Recipe& r, int gridSize);

// cells: gridSize * gridSize Felder, zeilenweise (gridSize 2 oder 3). Liefert das Ergebnis oder einen leeren Stapel.
ItemStack matchRecipe(const ItemStack* cells, int gridSize);

// Verbraucht je Zutat ein Stueck aus jedem belegten Feld.
void consumeIngredients(ItemStack* cells, int gridSize);

// Ofen
struct SmeltRecipe {
    ItemId input;
    ItemStack output;
};
const SmeltRecipe* smeltRecipe(ItemId input);  // nullptr, wenn der Gegenstand nicht geschmolzen werden kann
float fuelSeconds(ItemId item);                // Brenndauer, 0 = kein Brennstoff
constexpr float SMELT_SECONDS = 10.0f;         // je Gegenstand

}  // namespace oc
