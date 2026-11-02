#pragma once
#include <cstdint>

#include "Chunk.h"
#include "Noise.h"
#include "Village.h"

namespace oc {

class TerrainGenerator {
public:
    explicit TerrainGenerator(int32_t seed);

    // Fuellt den Chunk (Gelaende, Hoehlen, Wasser, Baeume).
    void generate(Chunk& chunk) const;

    struct Column {
        int height;  // oberste feste Schicht
        bool desert;
        bool snow;
        bool forest;
        float temp = 0.0f, moist = 0.0f;  // Rauschwerte -1..1
    };
    Column column(int wx, int wz) const;

    int32_t seed() const { return seed_; }
    const VillageGenerator& villages() const { return villages_; }

private:
    bool isCave(int wx, int y, int wz) const;
    void placeTrees(Chunk& chunk) const;
    void placeOres(Chunk& chunk) const;
    void placePlants(Chunk& chunk, const Column* cols) const;

    int32_t seed_;
    VillageGenerator villages_;
    Noise continental_, detail_, ridge_, temperature_, moisture_;
    Noise cave1_, cave2_, cavern_, entrance_;
};

}  // namespace oc
