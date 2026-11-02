#include "TextureEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

#include "Faces.h"
#include "Models.h"

namespace oc {

namespace {

constexpr int NET_W = 4 * TILE_PX, NET_H = 3 * TILE_PX;

// Zellen des Kreuznetzes (Spalte, Zeile): oben (1,0), Seiten (0..3,1), unten (1,2)
struct Cell {
    int cx, cy, which;  // which: 0 oben, 1 unten, 2 Seite
    char label;
};
const Cell NET_CELLS[] = {{1, 0, 0, 'O'}, {0, 1, 2, 'S'}, {1, 1, 2, 'S'}, {2, 1, 2, 'S'}, {3, 1, 2, 'S'}, {1, 2, 1, 'U'}};

std::vector<EditorEntry> buildEntries() {
    std::vector<EditorEntry> v;
    auto block = [&](BlockId b, const char* suffix = "") {
        const BlockInfo& bi = blockInfo(b);
        EditorEntry e;
        e.kind = EditorEntry::BlockNet;
        e.name = std::string(bi.name) + suffix;
        e.group = "Bloecke";
        e.tiles[0] = bi.top;
        e.tiles[1] = bi.bottom;
        e.tiles[2] = bi.side;
        v.push_back(e);
    };
    for (BlockId b : {GRASS, DIRT, STONE, SAND, LOG, LEAVES, PLANKS, COBBLE, BEDROCK, GLASS, SNOW, WATER, LAVA, OBSIDIAN,
                      CRAFTING_TABLE, COAL_ORE, IRON_ORE, GOLD_ORE, DIAMOND_ORE, FURNACE, BRICKS, STONE_BRICKS, SANDSTONE,
                      GRAVEL, CLAY, WOOL_WHITE, WOOL_RED, WOOL_BLUE, WOOL_GREEN, WOOL_YELLOW, WOOL_BLACK, BOOKSHELF,
                      IRON_BLOCK, GOLD_BLOCK, DIAMOND_BLOCK, COAL_BLOCK, LAMP, PUMPKIN, CACTUS, BIRCH_LOG, BIRCH_LEAVES,
                      SPRUCE_LOG, SPRUCE_LEAVES, CHEST, BED})
        block(b);
    block(FURNACE_LIT, " (an)");
    for (int t : {TILE_IRON_BARS, TILE_LANTERN, TILE_DOOR_WOOD_LOWER, TILE_DOOR_WOOD_UPPER, TILE_DOOR_IRON_LOWER, TILE_DOOR_IRON_UPPER, TILE_GATE_WOOD,
                  TILE_GATE_IRON, TILE_FARMLAND, TILE_WHEAT_0, TILE_WHEAT_1, TILE_WHEAT_2, TILE_WHEAT_3, TILE_CARROT_0, TILE_CARROT_1, TILE_CARROT_2,
                  TILE_CARROT_3, TILE_SEEDS, TILE_WHEAT, TILE_BREAD, TILE_CARROT, TILE_HOE_WOOD, TILE_HOE_STONE, TILE_HOE_IRON, TILE_HOE_GOLD,
                  TILE_HOE_DIAMOND, TILE_DOOR_WOOD_ITEM, TILE_DOOR_IRON_ITEM, TILE_TALL_GRASS, TILE_FERN, TILE_FLOWER_RED, TILE_FLOWER_YELLOW, TILE_FLOWER_BLUE, TILE_FLOWER_WHITE, TILE_MUSHROOM_RED,
                  TILE_MUSHROOM_BROWN, TILE_DEAD_BUSH, TILE_SUGAR_CANE, TILE_BERRY_BUSH, TILE_BRICK, TILE_CLAY_BALL, TILE_PAPER, TILE_BOOK,
                  TILE_DYE_RED, TILE_DYE_YELLOW, TILE_DYE_BLUE, TILE_DYE_GREEN, TILE_DYE_BLACK, TILE_BERRIES, TILE_TORCH, TILE_STICK, TILE_COAL, TILE_IRON_INGOT, TILE_GOLD_INGOT, TILE_DIAMOND, TILE_MEAT, TILE_COOKED_MEAT, TILE_PICKAXE_WOOD,
                  TILE_AXE_WOOD, TILE_SHOVEL_WOOD, TILE_SWORD_WOOD, TILE_PICKAXE_STONE, TILE_AXE_STONE, TILE_SHOVEL_STONE,
                  TILE_SWORD_STONE, TILE_PICKAXE_IRON, TILE_AXE_IRON, TILE_SHOVEL_IRON, TILE_SWORD_IRON, TILE_PICKAXE_DIAMOND,
                  TILE_AXE_DIAMOND, TILE_SHOVEL_DIAMOND, TILE_SWORD_DIAMOND, TILE_PICKAXE_GOLD, TILE_AXE_GOLD, TILE_SHOVEL_GOLD,
                  TILE_SWORD_GOLD}) {
        EditorEntry e;
        e.kind = EditorEntry::FlatTile;
        e.name = tileName(t).name;
        e.group = "Gegenstaende";
        e.tiles[0] = e.tiles[1] = e.tiles[2] = t;
        v.push_back(e);
    }
    for (int s = 0; s < SKIN_COUNT; ++s) {
        EditorEntry e;
        e.kind = EditorEntry::Skin;
        e.name = skinInfo(static_cast<SkinId>(s)).name;
        e.group = "Figuren";
        e.skin = static_cast<SkinId>(s);
        v.push_back(e);
    }
    return v;
}

}  // namespace

const std::vector<EditorEntry>& editorEntries() {
    static const std::vector<EditorEntry> e = buildEntries();
    return e;
}

TextureEditor::TextureEditor(TextureStore& store) : store_(store), entries_(buildEntries()) { rebuildView(); }

void TextureEditor::select(int index) {
    if (index < 0 || index >= static_cast<int>(entries_.size())) return;
    current_ = index;
    undo_.clear();
    redo_.clear();
    stroking_ = false;
    rebuildView();
}

int TextureEditor::uniqueTiles(int out[3]) const {
    const EditorEntry& e = entry();
    if (e.kind == EditorEntry::Skin) return 0;
    int n = 0;
    const int count = e.kind == EditorEntry::BlockNet ? 3 : 1;
    for (int i = 0; i < count; ++i) {
        bool dup = false;
        for (int j = 0; j < n; ++j) dup = dup || out[j] == e.tiles[i];
        if (!dup) out[n++] = e.tiles[i];
    }
    return n;
}

void TextureEditor::rebuildView() {
    const EditorEntry& e = entry();
    if (e.kind == EditorEntry::Skin) {
        view_ = store_.skin(e.skin);
    } else if (e.kind == EditorEntry::FlatTile) {
        view_ = store_.tile(e.tiles[0]);
    } else {
        view_ = Image(NET_W, NET_H);
        for (const Cell& c : NET_CELLS) {
            const Image t = store_.tile(e.tiles[c.which]);
            for (int y = 0; y < TILE_PX; ++y)
                for (int x = 0; x < TILE_PX; ++x) std::memcpy(view_.at(c.cx * TILE_PX + x, c.cy * TILE_PX + y), t.at(x, y), 4);
        }
    }
}

bool TextureEditor::mapPixel(int vx, int vy, int& tile, int& tx, int& ty) const {
    const EditorEntry& e = entry();
    if (vx < 0 || vy < 0 || vx >= view_.w || vy >= view_.h) return false;
    if (e.kind == EditorEntry::FlatTile) {
        tile = e.tiles[0];
        tx = vx;
        ty = vy;
        return true;
    }
    if (e.kind == EditorEntry::BlockNet) {
        const int cx = vx / TILE_PX, cy = vy / TILE_PX;
        for (const Cell& c : NET_CELLS)
            if (c.cx == cx && c.cy == cy) {
                tile = e.tiles[c.which];
                tx = vx % TILE_PX;
                ty = vy % TILE_PX;
                return true;
            }
    }
    return false;
}

bool TextureEditor::editable(int vx, int vy) const {
    if (vx < 0 || vy < 0 || vx >= view_.w || vy >= view_.h) return false;
    if (entry().kind == EditorEntry::Skin) return true;
    int t, x, y;
    return mapPixel(vx, vy, t, x, y);
}

std::vector<NetRegion> TextureEditor::regions() const {
    std::vector<NetRegion> r;
    const EditorEntry& e = entry();
    if (e.kind == EditorEntry::BlockNet) {
        for (const Cell& c : NET_CELLS) r.push_back({c.cx * TILE_PX, c.cy * TILE_PX, TILE_PX, TILE_PX, c.label});
    } else if (e.kind == EditorEntry::Skin) {
        const char labels[6] = {'V', 'H', 'O', 'U', 'R', 'L'};
        for (const ModelPart& p : modelParts(e.skin)) {
            if (p.mirror) continue;
            for (int f = 0; f < 6; ++f) {
                const NetRect nr = skinNetRect(p, f);
                r.push_back({nr.x, nr.y, nr.w, nr.h, labels[f]});
            }
        }
    }
    return r;
}

TextureEditor::Snapshot TextureEditor::snapshot() const {
    Snapshot s;
    const EditorEntry& e = entry();
    if (e.kind == EditorEntry::Skin) {
        s.skin = store_.skin(e.skin);
    } else {
        s.n = uniqueTiles(s.tiles);
        for (int i = 0; i < s.n; ++i) s.images[i] = store_.tile(s.tiles[i]);
    }
    return s;
}

void TextureEditor::restore(const Snapshot& s) {
    const EditorEntry& e = entry();
    if (e.kind == EditorEntry::Skin) store_.setSkin(e.skin, s.skin);
    else
        for (int i = 0; i < s.n; ++i) store_.setTile(s.tiles[i], s.images[i]);
    rebuildView();
}

void TextureEditor::beginStroke() {
    if (stroking_) return;
    stroking_ = true;
    undo_.push_back(snapshot());
    if (undo_.size() > 64) undo_.erase(undo_.begin());
    redo_.clear();
}

void TextureEditor::endStroke() { stroking_ = false; }

bool TextureEditor::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(snapshot());
    restore(undo_.back());
    undo_.pop_back();
    modified_ = true;
    return true;
}

bool TextureEditor::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(snapshot());
    restore(redo_.back());
    redo_.pop_back();
    modified_ = true;
    return true;
}

void TextureEditor::writePixel(int vx, int vy, const uint8_t rgba[4]) {
    const EditorEntry& e = entry();
    if (e.kind == EditorEntry::Skin) {
        store_.setSkinPixel(e.skin, vx, vy, rgba);
    } else {
        int t, tx, ty;
        if (!mapPixel(vx, vy, t, tx, ty)) return;
        store_.setTilePixel(t, tx, ty, rgba);
    }
    modified_ = true;
}

void TextureEditor::flood(int vx, int vy) {
    // Bereich, in dem gefuellt werden darf: Kachelzelle bzw. Netzflaeche
    int rx = 0, ry = 0, rw = view_.w, rh = view_.h;
    const EditorEntry& e = entry();
    if (e.kind == EditorEntry::BlockNet) {
        rx = (vx / TILE_PX) * TILE_PX;
        ry = (vy / TILE_PX) * TILE_PX;
        rw = rh = TILE_PX;
    } else if (e.kind == EditorEntry::Skin) {
        bool found = false;
        for (const NetRegion& r : regions())
            if (vx >= r.x && vx < r.x + r.w && vy >= r.y && vy < r.y + r.h) {
                rx = r.x;
                ry = r.y;
                rw = r.w;
                rh = r.h;
                found = true;
                break;
            }
        if (!found) return;
    }
    uint8_t target[4];
    std::memcpy(target, view_.at(vx, vy), 4);
    if (std::memcmp(target, color, 4) == 0) return;
    // Aehnliche Farben zaehlen mit (die Standardtexturen sind leicht verrauscht)
    auto similar = [&](const uint8_t* p) {
        for (int c = 0; c < 4; ++c)
            if (std::abs(static_cast<int>(p[c]) - static_cast<int>(target[c])) > fillTolerance) return false;
        return true;
    };
    std::vector<std::pair<int, int>> stack{{vx, vy}};
    std::vector<uint8_t> seen(static_cast<size_t>(view_.w) * view_.h, 0);
    while (!stack.empty()) {
        auto [x, y] = stack.back();
        stack.pop_back();
        if (x < rx || y < ry || x >= rx + rw || y >= ry + rh) continue;
        const size_t i = static_cast<size_t>(y) * view_.w + x;
        if (seen[i] || !similar(view_.at(x, y))) continue;
        seen[i] = 1;
        writePixel(x, y, color);
        stack.push_back({x + 1, y});
        stack.push_back({x - 1, y});
        stack.push_back({x, y + 1});
        stack.push_back({x, y - 1});
    }
}

void TextureEditor::paintAt(int vx, int vy) {
    if (!editable(vx, vy)) return;
    if (tool == Tool::Picker) {
        std::memcpy(color, view_.at(vx, vy), 4);
        return;
    }
    const bool own = !stroking_;
    if (own) beginStroke();
    if (tool == Tool::Fill) {
        flood(vx, vy);
    } else {
        const uint8_t clear[4] = {0, 0, 0, 0};
        const uint8_t* c = tool == Tool::Eraser ? clear : color;
        const int n = std::max(1, std::min(3, brush));
        const int off = (n - 1) / 2;
        for (int dy = 0; dy < n; ++dy)
            for (int dx = 0; dx < n; ++dx) {
                const int x = vx - off + dx, y = vy - off + dy;
                if (editable(x, y)) writePixel(x, y, c);
            }
    }
    rebuildView();
    if (own) endStroke();
}

void TextureEditor::paintLine(int x0, int y0, int x1, int y1) {
    if (tool == Tool::Fill || tool == Tool::Picker) {
        paintAt(x1, y1);
        return;
    }
    const bool own = !stroking_;
    if (own) beginStroke();
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int guard = 0; guard < 4096; ++guard) {
        paintAt(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    if (own) endStroke();
}

std::string TextureEditor::exportPath() const {
    const EditorEntry& e = entry();
    std::string base;
    if (e.kind == EditorEntry::Skin) base = std::filesystem::path(store_.skinPath(e.skin)).filename().string();
    else if (e.kind == EditorEntry::FlatTile) base = std::string("block_") + tileName(e.tiles[0]).file + ".png";
    else base = std::string("net_") + tileName(e.tiles[2]).file + (e.tiles[0] == e.tiles[2] ? "" : std::string("_") + tileName(e.tiles[0]).file) + ".png";
    return store_.directory() + "/export/" + base;
}

bool TextureEditor::save(std::string* message) {
    const EditorEntry& e = entry();
    std::string err, paths;
    if (e.kind == EditorEntry::Skin) {
        if (!store_.saveSkin(e.skin, &err)) {
            if (message) *message = err;
            return false;
        }
        paths = store_.skinPath(e.skin);
    } else {
        int tiles[3];
        const int n = uniqueTiles(tiles);
        for (int i = 0; i < n; ++i) {
            if (!store_.saveTile(tiles[i], &err)) {
                if (message) *message = err;
                return false;
            }
            paths += (i ? ", " : "") + store_.tilePath(tiles[i]);
        }
    }
    modified_ = false;
    if (message) *message = "Gespeichert: " + paths;
    return true;
}

bool TextureEditor::exportNet(std::string* message) {
    std::string err;
    const std::string path = exportPath();
    if (!savePng(path, view_, &err)) {
        if (message) *message = err;
        return false;
    }
    if (message) *message = "Exportiert: " + path;
    return true;
}

bool TextureEditor::importNet(std::string* message) {
    std::string err;
    const std::string path = exportPath();
    Image img;
    if (!loadImage(path, img, &err)) {
        if (message) *message = "Nicht gefunden: " + path;
        return false;
    }
    const EditorEntry& e = entry();
    beginStroke();
    bool ok = true;
    if (e.kind == EditorEntry::Skin) {
        ok = TextureStore::conformSkin(e.skin, img, &err);
        if (ok) store_.setSkin(e.skin, img);
    } else if (e.kind == EditorEntry::FlatTile) {
        ok = TextureStore::conformTile(img, &err);
        if (ok) store_.setTile(e.tiles[0], img);
    } else {
        if (img.w != NET_W || img.h != NET_H) {
            ok = false;
            err = "Falsche Groesse " + std::to_string(img.w) + "x" + std::to_string(img.h) + ", erwartet " +
                  std::to_string(NET_W) + "x" + std::to_string(NET_H);
        } else {
            int applied[3] = {-1, -1, -1};
            int na = 0;
            for (const Cell& c : NET_CELLS) {
                const int tile = e.tiles[c.which];
                bool done = false;
                for (int i = 0; i < na; ++i) done = done || applied[i] == tile;
                if (done) continue;
                Image cell(TILE_PX, TILE_PX);
                for (int y = 0; y < TILE_PX; ++y)
                    for (int x = 0; x < TILE_PX; ++x) std::memcpy(cell.at(x, y), img.at(c.cx * TILE_PX + x, c.cy * TILE_PX + y), 4);
                if (cell.rgba != store_.tile(tile).rgba) {  // diese Zelle wurde veraendert
                    store_.setTile(tile, cell);
                    applied[na++] = tile;
                }
            }
        }
    }
    endStroke();
    if (!ok) {
        undo_.pop_back();  // der Schnappschuss war unnoetig
        if (message) *message = err;
        return false;
    }
    modified_ = true;
    rebuildView();
    if (message) *message = "Importiert: " + path + " (zum Behalten noch speichern)";
    return true;
}

void TextureEditor::resetToDefault(std::string* message) {
    const EditorEntry& e = entry();
    beginStroke();
    if (e.kind == EditorEntry::Skin) {
        store_.resetSkin(e.skin);
        store_.removeSkinFile(e.skin);
    } else {
        int tiles[3];
        const int n = uniqueTiles(tiles);
        for (int i = 0; i < n; ++i) {
            store_.resetTile(tiles[i]);
            store_.removeTileFile(tiles[i]);
        }
    }
    endStroke();
    modified_ = false;
    rebuildView();
    if (message) *message = "Standardtextur wiederhergestellt";
}

// ---------------------------------------------------------------------------------------------------------------
// Vorschau
// ---------------------------------------------------------------------------------------------------------------

std::vector<PreviewQuad> TextureEditor::preview(float yaw, float pitch) const {
    struct Face {
        glm::vec3 c[4];
        glm::vec3 n;
        glm::vec2 uv[4];
        float shade;
        bool skin;
    };
    std::vector<Face> faces;
    const EditorEntry& e = entry();

    if (e.kind == EditorEntry::Skin) {
        const SkinInfo& si = skinInfo(e.skin);
        for (const ModelPart& p : modelParts(e.skin)) {
            const glm::vec3 c = p.center / 16.0f, sz = p.size / 16.0f;
            for (int f = 0; f < 6; ++f) {
                const FaceDef& fd = faceDef(f);
                Face face;
                for (int k = 0; k < 4; ++k) {
                    const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
                    glm::vec3 local = c;
                    for (int a = 0; a < 3; ++a) {
                        const float s = static_cast<float>(fd.n[a]) + (i ? 1.0f : -1.0f) * static_cast<float>(fd.t1[a]) +
                                        (j ? 1.0f : -1.0f) * static_cast<float>(fd.t2[a]);
                        local[a] += 0.5f * sz[a] * s;
                    }
                    face.c[k] = local;
                    face.uv[k] = skinCornerUV(p, f, i, j, si.width, si.height);
                }
                face.n = glm::vec3(static_cast<float>(fd.n[0]), static_cast<float>(fd.n[1]), static_cast<float>(fd.n[2]));
                face.shade = fd.shade;
                face.skin = true;
                faces.push_back(face);
            }
        }
    } else {
        // Wuerfel um den Ursprung (Kantenlaenge 1)
        for (int f = 0; f < 6; ++f) {
            const FaceDef& fd = faceDef(f);
            const int tile = f == 2 ? e.tiles[0] : (f == 3 ? e.tiles[1] : e.tiles[2]);
            const UVRect uv = tileUV(tile);
            Face face;
            for (int k = 0; k < 4; ++k) {
                const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
                glm::vec3 local(0.0f);
                for (int a = 0; a < 3; ++a) {
                    const float s = static_cast<float>(fd.n[a]) + (i ? 1.0f : -1.0f) * static_cast<float>(fd.t1[a]) +
                                    (j ? 1.0f : -1.0f) * static_cast<float>(fd.t2[a]);
                    local[a] = 0.5f * s;
                }
                face.c[k] = local;
                float uu, vv;
                faceUV(f, i, j, uu, vv);
                face.uv[k] = glm::vec2(uv.u0 + (uv.u1 - uv.u0) * uu, uv.v0 + (uv.v1 - uv.v0) * vv);
            }
            face.n = glm::vec3(static_cast<float>(fd.n[0]), static_cast<float>(fd.n[1]), static_cast<float>(fd.n[2]));
            face.shade = fd.shade;
            face.skin = false;
            faces.push_back(face);
        }
    }

    // Mitte und Radius: danach liegt jede Drehung in der Einheitskugel
    glm::vec3 mn(1e9f), mx(-1e9f);
    for (const Face& f : faces)
        for (const glm::vec3& c : f.c) {
            mn = glm::min(mn, c);
            mx = glm::max(mx, c);
        }
    const glm::vec3 center = (mn + mx) * 0.5f;
    float radius = 1e-3f;
    for (const Face& f : faces)
        for (const glm::vec3& c : f.c) radius = std::max(radius, glm::length(c - center));

    const float base = -1.57079633f;  // Modell blickt +x: Vorderseite zum Betrachter drehen
    const float cy = std::cos(yaw + base), sy = std::sin(yaw + base);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    auto rot = [&](glm::vec3 v) {
        float x = v.x * cy + v.z * sy, z = -v.x * sy + v.z * cy;  // um Y
        float y = v.y * cp - z * sp, z2 = v.y * sp + z * cp;      // um X (Kippen)
        return glm::vec3(x, y, z2);
    };

    struct Sorted {
        float depth;
        PreviewQuad q;
    };
    std::vector<Sorted> out;
    for (const Face& f : faces) {
        const glm::vec3 n = rot(f.n);
        if (n.z <= 0.0f) continue;  // Rueckseite
        Sorted s;
        s.q.shade = f.shade;
        s.q.skin = f.skin;
        float depth = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const glm::vec3 r = rot((f.c[k] - center) / radius);
            s.q.p[k] = glm::vec2(r.x, -r.y);
            s.q.uv[k] = f.uv[k];
            depth += r.z;
        }
        s.depth = depth / 4.0f;
        out.push_back(s);
    }
    std::sort(out.begin(), out.end(), [](const Sorted& a, const Sorted& b) { return a.depth < b.depth; });
    std::vector<PreviewQuad> result;
    result.reserve(out.size());
    for (const Sorted& s : out) result.push_back(s.q);
    return result;
}

}  // namespace oc
