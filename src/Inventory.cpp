#include "Inventory.h"

#include <algorithm>

namespace oc {

int Inventory::add(ItemId id, int count) {
    if (!itemValid(id) || count <= 0) return count;
    const int maxStack = itemInfo(id).maxStack;
    // Reihenfolge: Hotbar zuerst, dann Rucksack
    for (int pass = 0; pass < 2 && count > 0; ++pass) {
        for (int i = 0; i < SIZE && count > 0; ++i) {
            ItemStack& s = slots[i];
            if (pass == 0) {
                if (s.id != id || s.count >= maxStack) continue;
                int n = std::min(count, maxStack - s.count);
                s.count += n;
                count -= n;
            } else {
                if (!s.empty()) continue;
                int n = std::min(count, maxStack);
                s.id = id;
                s.count = n;
                count -= n;
            }
        }
    }
    return count;
}

bool Inventory::take(int slot, int n) {
    if (slot < 0 || slot >= SIZE) return false;
    ItemStack& s = slots[slot];
    if (s.empty() || s.count < n) return false;
    s.count -= n;
    if (s.count <= 0) s.clear();
    return true;
}

int Inventory::count(ItemId id) const {
    int n = 0;
    for (const ItemStack& s : slots)
        if (s.id == id) n += s.count;
    return n;
}

void Inventory::clear() {
    for (ItemStack& s : slots) s.clear();
}

bool Inventory::isEmpty() const {
    for (const ItemStack& s : slots)
        if (!s.empty()) return false;
    return true;
}

void Inventory::click(ItemStack& slot, ItemStack& cursor, bool right) {
    if (slot.count <= 0) slot.clear();
    if (cursor.count <= 0) cursor.clear();

    if (!right) {
        if (cursor.empty()) {
            std::swap(slot, cursor);
        } else if (slot.empty()) {
            slot = cursor;
            cursor.clear();
        } else if (slot.id == cursor.id) {
            int space = itemInfo(slot.id).maxStack - slot.count;
            int n = std::min(space, cursor.count);
            slot.count += n;
            cursor.count -= n;
            if (cursor.count <= 0) cursor.clear();
        } else {
            std::swap(slot, cursor);
        }
    } else {
        if (cursor.empty()) {
            if (slot.empty()) return;
            int take = (slot.count + 1) / 2;
            cursor.id = slot.id;
            cursor.count = take;
            slot.count -= take;
            if (slot.count <= 0) slot.clear();
        } else if (slot.empty() || (slot.id == cursor.id && slot.count < itemInfo(slot.id).maxStack)) {
            if (slot.empty()) slot.id = cursor.id;
            slot.count += 1;
            cursor.count -= 1;
            if (cursor.count <= 0) cursor.clear();
        } else {
            std::swap(slot, cursor);
        }
    }
}

void Inventory::quickMove(int slot) {
    if (slot < 0 || slot >= SIZE || slots[slot].empty()) return;
    ItemStack moving = slots[slot];
    slots[slot].clear();
    const bool fromHotbar = slot < HOTBAR;
    const int begin = fromHotbar ? HOTBAR : 0, end = fromHotbar ? SIZE : HOTBAR;
    const int maxStack = itemInfo(moving.id).maxStack;
    for (int pass = 0; pass < 2 && moving.count > 0; ++pass) {
        for (int i = begin; i < end && moving.count > 0; ++i) {
            ItemStack& s = slots[i];
            if (pass == 0) {
                if (s.id != moving.id || s.count >= maxStack) continue;
                int n = std::min(moving.count, maxStack - s.count);
                s.count += n;
                moving.count -= n;
            } else if (s.empty()) {
                s = moving;
                moving.clear();
            }
        }
    }
    if (moving.count > 0) slots[slot] = moving;  // kein Platz: bleibt liegen
}

}  // namespace oc
