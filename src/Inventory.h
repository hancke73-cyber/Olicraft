#pragma once
#include <array>

#include "Items.h"

namespace oc {

class Inventory {
public:
    static constexpr int SIZE = 36;    // 0..8 Hotbar, 9..35 Rucksack
    static constexpr int HOTBAR = 9;

    std::array<ItemStack, SIZE> slots;

    // Legt Gegenstaende ab (erst bestehende Stapel, dann leere Plaetze). Rueckgabe: Rest, der nicht passte.
    int add(ItemId id, int count, int damage = 0);
    // Entfernt n Stueck aus dem Slot; false, wenn nicht genug da ist.
    bool take(int slot, int n = 1);
    int count(ItemId id) const;
    void clear();
    bool isEmpty() const;

    // Mausklick auf einen Slot: links nimmt/legt den ganzen Stapel, rechts die Haelfte bzw. ein Stueck.
    static void click(ItemStack& slot, ItemStack& cursor, bool rightButton);
    // Umschalt-Klick: Stapel zwischen Hotbar und Rucksack verschieben.
    void quickMove(int slot);
};

}  // namespace oc
