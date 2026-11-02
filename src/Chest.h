#pragma once
#include <algorithm>
#include <array>

#include "Items.h"

namespace oc {

// Inhalt einer Truhe: 3 Reihen zu 9 Plaetzen
struct Chest {
    static constexpr int SIZE = 27;
    std::array<ItemStack, SIZE> slots;

    bool isEmpty() const {
        for (const ItemStack& s : slots)
            if (!s.empty()) return false;
        return true;
    }
    // Legt Gegenstaende ab (erst bestehende Stapel, dann leere Plaetze). Rueckgabe: Rest, der nicht passte.
    int add(ItemId id, int count, int damage = 0) {
        const int maxStack = itemInfo(id).maxStack;
        for (ItemStack& s : slots)
            if (!s.empty() && s.id == id && s.damage == damage && s.count < maxStack) {
                const int n = std::min(count, maxStack - s.count);
                s.count += n;
                count -= n;
                if (count <= 0) return 0;
            }
        for (ItemStack& s : slots)
            if (s.empty()) {
                const int n = std::min(count, maxStack);
                s = {id, n, damage};
                count -= n;
                if (count <= 0) return 0;
            }
        return count;
    }
};

}  // namespace oc
