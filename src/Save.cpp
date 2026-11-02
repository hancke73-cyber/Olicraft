#include "Save.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace oc {

namespace {

constexpr uint32_t MAGIC = 0x52434C4Fu;  // "OLCR" (little endian)
constexpr uint32_t VERSION = 4;

class Writer {
public:
    explicit Writer(std::ofstream& f) : f_(f) {}
    void u8(uint8_t v) { f_.put(static_cast<char>(v)); }
    void u16(uint16_t v) {
        u8(static_cast<uint8_t>(v & 0xFF));
        u8(static_cast<uint8_t>(v >> 8));
    }
    void u32(uint32_t v) {
        for (int i = 0; i < 4; ++i) u8(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void f32(float v) {
        uint32_t u;
        std::memcpy(&u, &v, 4);
        u32(u);
    }
    void bytes(const uint8_t* p, size_t n) { f_.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n)); }

private:
    std::ofstream& f_;
};

class Reader {
public:
    explicit Reader(std::ifstream& f) : f_(f) {}
    bool ok() const { return ok_; }
    uint8_t u8() {
        char c = 0;
        if (!f_.get(c)) ok_ = false;
        return static_cast<uint8_t>(c);
    }
    uint16_t u16() {
        uint16_t lo = u8();
        uint16_t hi = u8();
        return static_cast<uint16_t>(lo | (hi << 8));
    }
    uint32_t u32() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(u8()) << (8 * i);
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    float f32() {
        uint32_t u = u32();
        float v;
        std::memcpy(&v, &u, 4);
        return v;
    }
    void bytes(uint8_t* p, size_t n) {
        f_.read(reinterpret_cast<char*>(p), static_cast<std::streamsize>(n));
        if (static_cast<size_t>(f_.gcount()) != n) ok_ = false;
    }

private:
    std::ifstream& f_;
    bool ok_ = true;
};

}  // namespace

std::vector<uint8_t> rleEncode(const std::vector<BlockId>& blocks) {
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < blocks.size()) {
        BlockId b = blocks[i];
        size_t run = 1;
        while (i + run < blocks.size() && blocks[i + run] == b && run < 65535) ++run;
        out.push_back(b);
        out.push_back(static_cast<uint8_t>(run & 0xFF));
        out.push_back(static_cast<uint8_t>(run >> 8));
        i += run;
    }
    return out;
}

bool rleDecode(const std::vector<uint8_t>& rle, std::vector<BlockId>& out, size_t expectedSize) {
    out.clear();
    out.reserve(expectedSize);
    if (rle.size() % 3 != 0) return false;
    for (size_t i = 0; i < rle.size(); i += 3) {
        BlockId b = rle[i];
        size_t run = static_cast<size_t>(rle[i + 1]) | (static_cast<size_t>(rle[i + 2]) << 8);
        if (b >= BLOCK_COUNT) b = AIR;
        if (out.size() + run > expectedSize) return false;
        out.insert(out.end(), run, b);
    }
    return out.size() == expectedSize;
}

bool writeSaveFile(const std::string& path, const SaveFile& save, std::string* error) {
    namespace fs = std::filesystem;
    try {
        fs::path p(path);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) {
                if (error) *error = "Datei kann nicht geschrieben werden: " + tmp;
                return false;
            }
            Writer w(f);
            w.u32(MAGIC);
            w.u32(VERSION);
            w.i32(save.seed);
            w.f32(save.player.pos.x);
            w.f32(save.player.pos.y);
            w.f32(save.player.pos.z);
            w.f32(save.player.yaw);
            w.f32(save.player.pitch);
            w.f32(save.player.timeOfDay);
            w.i32(save.player.slot);
            w.u8(save.player.flying ? 1 : 0);
            // Version 2: Gesundheit, Spielmodus, Inventar
            w.i32(save.player.health);
            w.u8(save.player.creative ? 1 : 0);
            w.u8(static_cast<uint8_t>(Inventory::SIZE));
            for (const ItemStack& st : save.player.inventory.slots) {
                w.u16(st.empty() ? 0 : st.id);
                w.u8(static_cast<uint8_t>(st.empty() ? 0 : st.count));
                w.u16(static_cast<uint16_t>(st.empty() ? 0 : std::min(st.damage, 65535)));
            }
            // Version 4: Oefen
            w.u32(static_cast<uint32_t>(save.furnaces.size()));
            for (const FurnaceSave& fs : save.furnaces) {
                w.i32(fs.x);
                w.i32(fs.y);
                w.i32(fs.z);
                for (const ItemStack* st : {&fs.furnace.input, &fs.furnace.fuel, &fs.furnace.output}) {
                    w.u16(st->empty() ? 0 : st->id);
                    w.u8(static_cast<uint8_t>(st->empty() ? 0 : st->count));
                    w.u16(static_cast<uint16_t>(st->empty() ? 0 : std::min(st->damage, 65535)));
                }
                w.f32(fs.furnace.burnTime);
                w.f32(fs.furnace.burnTotal);
                w.f32(fs.furnace.cookTime);
            }
            w.u32(static_cast<uint32_t>(save.chunks.size()));
            for (const SavedChunk& c : save.chunks) {
                w.i32(c.cx);
                w.i32(c.cz);
                w.u32(static_cast<uint32_t>(c.rle.size()));
                w.bytes(c.rle.data(), c.rle.size());
            }
            f.flush();
            if (!f) {
                if (error) *error = "Schreibfehler";
                return false;
            }
        }
        fs::rename(tmp, path);  // ersetzt auf allen Plattformen (C++17: std::filesystem)
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

bool readSaveFile(const std::string& path, SaveFile& save, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "Datei nicht gefunden";
        return false;
    }
    Reader r(f);
    if (r.u32() != MAGIC) {
        if (error) *error = "Keine OliCraft-Speicherdatei";
        return false;
    }
    const uint32_t version = r.u32();
    if (version < 1 || version > VERSION) {
        if (error) *error = "Unbekannte Version der Speicherdatei";
        return false;
    }
    save = SaveFile{};
    save.seed = r.i32();
    save.player.pos.x = r.f32();
    save.player.pos.y = r.f32();
    save.player.pos.z = r.f32();
    save.player.yaw = r.f32();
    save.player.pitch = r.f32();
    save.player.timeOfDay = r.f32();
    save.player.slot = r.i32();
    save.player.flying = r.u8() != 0;
    if (version >= 2) {  // Version 2: Gesundheit, Modus, Inventar; ab Version 3 mit Abnutzung
        save.player.health = r.i32();
        save.player.creative = r.u8() != 0;
        const int invCount = r.u8();
        for (int i = 0; i < invCount; ++i) {
            ItemId id = r.u16();
            int cnt = r.u8();
            int dmg = version >= 3 ? r.u16() : 0;
            if (i < Inventory::SIZE && itemValid(id) && cnt > 0) {
                save.player.inventory.slots[i].id = id;
                save.player.inventory.slots[i].count = std::min(cnt, itemInfo(id).maxStack);
                save.player.inventory.slots[i].damage = std::min(dmg, std::max(0, itemInfo(id).maxDurability));
            }
        }
        save.player.hasInventory = true;
        save.player.health = std::max(1, std::min(20, save.player.health));
    }
    if (version >= 4) {
        const uint32_t fcount = r.u32();
        if (!r.ok() || fcount > 100000u) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
        for (uint32_t i = 0; i < fcount; ++i) {
            FurnaceSave fs;
            fs.x = r.i32();
            fs.y = r.i32();
            fs.z = r.i32();
            for (ItemStack* st : {&fs.furnace.input, &fs.furnace.fuel, &fs.furnace.output}) {
                ItemId id = r.u16();
                int cnt = r.u8();
                int dmg = r.u16();
                if (itemValid(id) && cnt > 0) {
                    st->id = id;
                    st->count = std::min(cnt, itemInfo(id).maxStack);
                    st->damage = std::min(dmg, std::max(0, itemInfo(id).maxDurability));
                }
            }
            fs.furnace.burnTime = std::max(0.0f, r.f32());
            fs.furnace.burnTotal = std::max(0.0f, r.f32());
            fs.furnace.cookTime = std::max(0.0f, r.f32());
            if (!r.ok()) {
                if (error) *error = "Beschaedigte Speicherdatei";
                return false;
            }
            save.furnaces.push_back(fs);
        }
    }
    uint32_t n = r.u32();
    if (!r.ok() || n > 10000000u) {
        if (error) *error = "Beschaedigte Speicherdatei";
        return false;
    }
    for (uint32_t i = 0; i < n; ++i) {
        SavedChunk c;
        c.cx = r.i32();
        c.cz = r.i32();
        uint32_t len = r.u32();
        if (!r.ok() || len > 4u * 1024u * 1024u) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
        c.rle.resize(len);
        r.bytes(c.rle.data(), len);
        if (!r.ok()) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
        save.chunks.push_back(std::move(c));
    }
    return true;
}

}  // namespace oc
