#include "TerrainGen.h"

#include <algorithm>
#include <cmath>

namespace oc {

namespace {
float smoothstep(float a, float b, float x) {
    float t = std::min(1.0f, std::max(0.0f, (x - a) / (b - a)));
    return t * t * (3.0f - 2.0f * t);
}
}  // namespace

TerrainGenerator::TerrainGenerator(int32_t seed)
    : seed_(seed),
      continental_(static_cast<uint32_t>(seed) + 1),
      detail_(static_cast<uint32_t>(seed) + 2),
      ridge_(static_cast<uint32_t>(seed) + 3),
      temperature_(static_cast<uint32_t>(seed) + 4),
      moisture_(static_cast<uint32_t>(seed) + 5),
      cave1_(static_cast<uint32_t>(seed) + 6),
      cave2_(static_cast<uint32_t>(seed) + 7),
      cavern_(static_cast<uint32_t>(seed) + 8),
      entrance_(static_cast<uint32_t>(seed) + 9) {}

TerrainGenerator::Column TerrainGenerator::column(int wx, int wz) const {
    const float x = static_cast<float>(wx), z = static_cast<float>(wz);

    float c = continental_.fbm2(x * 0.0035f, z * 0.0035f, 5);
    float d = detail_.fbm2(x * 0.02f, z * 0.02f, 3);
    float r = 1.0f - std::fabs(ridge_.fbm2(x * 0.007f, z * 0.007f, 3));
    r = r * r;

    float h = 66.0f + c * 52.0f + d * 5.0f;
    h += smoothstep(0.08f, 0.40f, c) * r * 46.0f;

    Column col;
    col.height = std::max(3, std::min(CHUNK_H - 8, static_cast<int>(std::floor(h))));
    float temp = temperature_.fbm2(x * 0.0028f + 300.0f, z * 0.0028f - 200.0f, 3);
    float moist = moisture_.fbm2(x * 0.004f - 700.0f, z * 0.004f + 900.0f, 3);
    col.desert = temp > 0.20f && col.height < 95;
    col.snow = col.height >= 100;
    col.forest = moist > 0.0f;
    return col;
}

bool TerrainGenerator::isCave(int wx, int y, int wz) const {
    const float x = static_cast<float>(wx), yy = static_cast<float>(y), z = static_cast<float>(wz);
    // Roehren: Schnitt zweier Null-Flaechen
    float a = cave1_.noise3(x * 0.026f, yy * 0.04f, z * 0.026f);
    if (std::fabs(a) < 0.055f) {
        float b = cave2_.noise3(x * 0.026f + 100.0f, yy * 0.04f - 50.0f, z * 0.026f + 70.0f);
        if (std::fabs(b) < 0.055f) return true;
    }
    // Grosse Hoehlen in der Tiefe
    if (y < 42) {
        float big = cavern_.fbm3(x * 0.018f, yy * 0.03f, z * 0.018f, 2);
        if (big > 0.30f) return true;
    }
    return false;
}

void TerrainGenerator::generate(Chunk& chunk) const {
    const int ox = chunk.cx * CHUNK_W, oz = chunk.cz * CHUNK_W;
    std::fill(chunk.blocks.begin(), chunk.blocks.end(), static_cast<BlockId>(AIR));

    for (int z = 0; z < CHUNK_W; ++z) {
        for (int x = 0; x < CHUNK_W; ++x) {
            const int wx = ox + x, wz = oz + z;
            const Column col = column(wx, wz);
            const int h = col.height;
            const bool beach = h <= SEA_LEVEL + 1 && h >= SEA_LEVEL - 10;

            for (int y = 0; y <= h; ++y) {
                BlockId b = STONE;
                const int depth = h - y;
                if (y == 0) {
                    b = BEDROCK;
                } else if (y <= 3 && (hash3(wx, y, wz, static_cast<uint32_t>(seed_)) % 4) < static_cast<uint32_t>(4 - y)) {
                    b = BEDROCK;
                } else if (depth == 0) {
                    if (col.snow) b = SNOW;
                    else if (col.desert || beach) b = SAND;
                    else if (h < SEA_LEVEL) b = DIRT;
                    else b = GRASS;
                } else if (depth <= 3) {
                    if (col.desert || beach) b = SAND;
                    else if (col.snow) b = STONE;
                    else b = DIRT;
                }
                chunk.set(x, y, z, b);
            }

            // Hoehlen (nicht bis zur Oberflaeche, ausser an gelegentlichen Eingaengen)
            int limit = h - 4;
            if (h > SEA_LEVEL + 3 && entrance_.noise2(wx * 0.02f, wz * 0.02f) > 0.35f) limit = h;
            for (int y = 5; y <= limit; ++y)
                if (chunk.get(x, y, z) != BEDROCK && isCave(wx, y, wz)) chunk.set(x, y, z, AIR);

            // Lava-Seen: Hoehlen tief unten sind mit Lava gefuellt
            for (int y = 5; y <= std::min(limit, LAVA_LEVEL); ++y)
                if (chunk.get(x, y, z) == AIR) chunk.set(x, y, z, LAVA);

            // Meer
            for (int y = h + 1; y <= SEA_LEVEL; ++y) chunk.set(x, y, z, WATER);
        }
    }

    placeOres(chunk);
    placeTrees(chunk);
}

// Erzadern: kleine Zufallsklumpen im Stein, je Chunk aus dem Seed abgeleitet (ueber Chunkgrenzen abgeschnitten)
void TerrainGenerator::placeOres(Chunk& chunk) const {
    struct OreType {
        BlockId block;
        int tries, minY, maxY, size;
    };
    static const OreType ORES[] = {
        {COAL_ORE, 16, 5, 100, 8},
        {IRON_ORE, 10, 5, 64, 6},
        {GOLD_ORE, 3, 5, 30, 5},
        {DIAMOND_ORE, 2, 5, 15, 4},
    };
    uint32_t s = hash3(chunk.cx, 0x0E5E, chunk.cz, static_cast<uint32_t>(seed_) ^ 0x0BEEFu);
    if (s == 0) s = 1;
    auto next = [&s]() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    };
    for (const OreType& o : ORES) {
        for (int t = 0; t < o.tries; ++t) {
            int x = static_cast<int>(next() % CHUNK_W), z = static_cast<int>(next() % CHUNK_W);
            int y = o.minY + static_cast<int>(next() % static_cast<uint32_t>(o.maxY - o.minY + 1));
            const int n = 1 + static_cast<int>(next() % static_cast<uint32_t>(o.size));
            for (int i = 0; i < n; ++i) {
                if (x >= 0 && x < CHUNK_W && z >= 0 && z < CHUNK_W && y >= 1 && y < CHUNK_H && chunk.get(x, y, z) == STONE)
                    chunk.set(x, y, z, o.block);
                switch (next() % 6) {
                    case 0: ++x; break;
                    case 1: --x; break;
                    case 2: ++y; break;
                    case 3: --y; break;
                    case 4: ++z; break;
                    default: --z; break;
                }
            }
        }
    }
}

void TerrainGenerator::placeTrees(Chunk& chunk) const {
    const int ox = chunk.cx * CHUNK_W, oz = chunk.cz * CHUNK_W;
    auto put = [&](int wx, int y, int wz, BlockId b, bool onlyAir) {
        int lx = wx - ox, lz = wz - oz;
        if (lx < 0 || lx >= CHUNK_W || lz < 0 || lz >= CHUNK_W || y < 0 || y >= CHUNK_H) return;
        if (onlyAir && chunk.get(lx, y, lz) != AIR) return;
        chunk.set(lx, y, lz, b);
    };

    // Wurzeln aus dem erweiterten Bereich betrachten, damit Kronen ueber Chunkgrenzen passen.
    for (int wz = oz - 3; wz < oz + CHUNK_W + 3; ++wz) {
        for (int wx = ox - 3; wx < ox + CHUNK_W + 3; ++wx) {
            uint32_t hsh = hash3(wx, 7, wz, static_cast<uint32_t>(seed_) ^ 0x7EE5u);
            // grobe Vorpruefung, bevor das teure Column() aufgerufen wird
            if ((hsh & 0xFFu) >= 64) continue;
            Column col = column(wx, wz);
            if (col.desert || col.snow || col.height <= SEA_LEVEL + 1 || col.height > 92) continue;
            const uint32_t chance = col.forest ? 40 : 6;  // von 256
            if (((hsh >> 8) & 0xFFu) >= chance) continue;

            const int top = col.height;
            const int trunk = 4 + static_cast<int>((hsh >> 16) % 3);
            const int topY = top + trunk;

            for (int y = top + 1; y <= topY; ++y) put(wx, y, wz, LOG, false);

            for (int dy = -2; dy <= 1; ++dy) {
                const int y = topY + dy;
                const int rad = dy <= -1 ? 2 : 1;
                for (int dz = -rad; dz <= rad; ++dz) {
                    for (int dx = -rad; dx <= rad; ++dx) {
                        if (dx == 0 && dz == 0 && dy <= 0) continue;  // Stamm
                        const bool corner = std::abs(dx) == rad && std::abs(dz) == rad;
                        if (corner) {
                            if (dy == 1) continue;
                            if (rad == 2 && (hash3(wx + dx, y, wz + dz, static_cast<uint32_t>(seed_)) & 1u)) continue;
                        }
                        put(wx + dx, y, wz + dz, LEAVES, true);
                    }
                }
            }
        }
    }
}

}  // namespace oc
