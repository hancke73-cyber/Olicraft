#pragma once
#include <vector>

#include "Items.h"

namespace oc {

// Berufe der Dorfbewohner; sie handeln mit Goldbarren als Waehrung.
enum Profession : int { PROF_FARMER = 0, PROF_LIBRARIAN, PROF_SMITH, PROF_MASON, PROF_SHEPHERD, PROF_COUNT };

struct Trade {
    ItemStack cost;    // das gibt der Spieler
    ItemStack result;  // das bekommt der Spieler
};

const char* professionName(int profession);
const std::vector<Trade>& tradesFor(int profession);
// Beruf eines Dorfbewohners aus Dorf und Nummer (deterministisch)
int professionFor(int villageId, int index);

}  // namespace oc
