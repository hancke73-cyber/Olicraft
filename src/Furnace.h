#pragma once
#include "Items.h"

namespace oc {

struct Furnace {
    ItemStack input, fuel, output;
    float burnTime = 0.0f;   // verbleibende Brenndauer des aktuellen Brennstoffs
    float burnTotal = 0.0f;  // Gesamtdauer des aktuellen Brennstoffs (fuer die Flammenanzeige)
    float cookTime = 0.0f;   // Fortschritt des aktuellen Gegenstands

    bool lit() const { return burnTime > 0.0f; }
};

}  // namespace oc
