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

    int height() const { return static_cast<int>(rows.size()); }
    int width() const { return rows.empty() ? 0 : static_cast<int>(rows[0].size()); }
};

const std::vector<Recipe>& recipes();

// cells: gridSize * gridSize Felder, zeilenweise (gridSize 2 oder 3). Liefert das Ergebnis oder einen leeren Stapel.
ItemStack matchRecipe(const ItemStack* cells, int gridSize);

// Verbraucht je Zutat ein Stueck aus jedem belegten Feld.
void consumeIngredients(ItemStack* cells, int gridSize);

}  // namespace oc
