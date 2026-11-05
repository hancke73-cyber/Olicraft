#include "Save.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace oc {

namespace {

constexpr uint32_t MAGIC = 0x52434C4Fu;  // "OLCR" (little endian)
constexpr uint32_t VERSION = 8;  // 7: Gegenstands-IDs ab 256 (vorher ab 200), 8: Fahrzeuge

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
            // Version 5: Truhen und Wiedereinstiegspunkt
            w.u32(static_cast<uint32_t>(save.chests.size()));
            for (const ChestSave& cs : save.chests) {
                w.i32(cs.x);
                w.i32(cs.y);
                w.i32(cs.z);
                for (const ItemStack& st : cs.chest.slots) {
                    w.u16(st.empty() ? 0 : st.id);
                    w.u8(static_cast<uint8_t>(st.empty() ? 0 : st.count));
                    w.u16(static_cast<uint16_t>(st.empty() ? 0 : std::min(st.damage, 65535)));
                }
            }
            w.u8(save.player.hasSpawn ? 1 : 0);
            w.f32(save.player.spawn.x);
            w.f32(save.player.spawn.y);
            w.f32(save.player.spawn.z);
            // Version 6: Ruestung
            w.u8(4);
            for (const ItemStack& st : save.player.inventory.armor) {
                w.u16(st.empty() ? 0 : st.id);
                w.u8(static_cast<uint8_t>(st.empty() ? 0 : st.count));
                w.u16(static_cast<uint16_t>(st.empty() ? 0 : std::min(st.damage, 65535)));
            }
            // Version 7: Schilder
            w.u32(static_cast<uint32_t>(save.signs.size()));
            for (const SignSave& ss : save.signs) {
                w.i32(ss.x);
                w.i32(ss.y);
                w.i32(ss.z);
                for (const std::string& line : ss.text.lines) {
                    const size_t len = std::min(line.size(), SignText::MAX_CHARS);
                    w.u8(static_cast<uint8_t>(len));
                    w.bytes(reinterpret_cast<const uint8_t*>(line.data()), len);
                }
            }
            // Version 7: Dampfmaschinen
            w.u32(static_cast<uint32_t>(save.machines.size()));
            for (const MachineSave& ms : save.machines) {
                w.i32(ms.x);
                w.i32(ms.y);
                w.i32(ms.z);
                w.u8(static_cast<uint8_t>(ms.m.kind));
                for (const ItemStack& st : ms.m.slots) {
                    w.u16(st.empty() ? 0 : st.id);
                    w.u8(static_cast<uint8_t>(st.empty() ? 0 : st.count));
                    w.u16(static_cast<uint16_t>(st.empty() ? 0 : std::min(st.damage, 65535)));
                }
                w.f32(ms.m.water);
                w.f32(ms.m.purity);
                w.f32(ms.m.burnTime);
                w.f32(ms.m.burnTotal);
                w.f32(ms.m.progress);
            }
            // Version 8: Fahrzeuge
            w.u32(static_cast<uint32_t>(save.vehicles.size()));
            for (const Vehicle& v : save.vehicles) {
                w.u8(static_cast<uint8_t>(v.type));
                w.f32(v.pos.x);
                w.f32(v.pos.y);
                w.f32(v.pos.z);
                w.f32(v.yaw);
                w.u8(static_cast<uint8_t>(v.balloon + 1));
                w.u8(v.propeller ? 1 : 0);
                w.u8(static_cast<uint8_t>(v.modules));
                w.u8(static_cast<uint8_t>(std::max(0, std::min(255, v.health))));
                w.u8(v.anchored ? 1 : 0);
                for (int k = 0; k < 2; ++k) {
                    const ItemStack& st = v.engine.slots[k];
                    w.u16(st.empty() ? 0 : st.id);
                    w.u8(static_cast<uint8_t>(st.empty() ? 0 : st.count));
                }
                w.f32(v.engine.water);
                w.f32(v.engine.purity);
                w.f32(v.engine.burnTime);
                w.f32(v.engine.burnTotal);
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

bool readSaveInfo(const std::string& path, SaveInfo& info) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    Reader r(f);
    if (r.u32() != MAGIC) return false;
    const uint32_t version = r.u32();
    if (version < 1 || version > VERSION) return false;
    info = SaveInfo{};
    info.seed = r.i32();
    for (int i = 0; i < 5; ++i) r.f32();  // Position, Blickrichtung
    r.f32();                              // Tageszeit
    r.i32();                              // Hotbar-Platz
    r.u8();                               // Flug
    if (version >= 2) {
        info.health = r.i32();
        info.creative = r.u8() != 0;
    } else {
        info.creative = true;  // Version 1 war immer Kreativ
    }
    return r.ok();
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
    // Version 6 und aelter: Gegenstaende hatten die IDs 200 bis 264, jetzt 256 bis 320
    auto migrateItem = [&](ItemId id) -> ItemId {
        return version < 7 && id >= OLD_ITEM_BASE && id <= OLD_ITEM_BASE + 64 ? static_cast<ItemId>(id - OLD_ITEM_BASE + ITEM_BASE) : id;
    };
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
            ItemId id = migrateItem(r.u16());
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
                ItemId id = migrateItem(r.u16());
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
    if (version >= 5) {
        const uint32_t ccount = r.u32();
        if (!r.ok() || ccount > 100000u) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
        for (uint32_t i = 0; i < ccount; ++i) {
            ChestSave cs;
            cs.x = r.i32();
            cs.y = r.i32();
            cs.z = r.i32();
            for (ItemStack& st : cs.chest.slots) {
                ItemId id = migrateItem(r.u16());
                int cnt = r.u8();
                int dmg = r.u16();
                if (itemValid(id) && cnt > 0) {
                    st.id = id;
                    st.count = std::min(cnt, itemInfo(id).maxStack);
                    st.damage = std::min(dmg, std::max(0, itemInfo(id).maxDurability));
                }
            }
            if (!r.ok()) {
                if (error) *error = "Beschaedigte Speicherdatei";
                return false;
            }
            save.chests.push_back(cs);
        }
        save.player.hasSpawn = r.u8() != 0;
        save.player.spawn.x = r.f32();
        save.player.spawn.y = r.f32();
        save.player.spawn.z = r.f32();
    }
    if (version >= 6) {
        const int n = r.u8();
        for (int i = 0; i < n; ++i) {
            ItemId id = migrateItem(r.u16());
            int cnt = r.u8();
            int dmg = r.u16();
            if (i < 4 && itemValid(id) && isArmorItem(id) && armorSlotOf(id) == i && cnt > 0) {
                ItemStack& st = save.player.inventory.armor[static_cast<size_t>(i)];
                st.id = id;
                st.count = 1;
                st.damage = std::min(dmg, itemInfo(id).maxDurability);
            }
        }
        if (!r.ok()) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
    }
    if (version >= 7) {
        const uint32_t scount = r.u32();
        if (!r.ok() || scount > 1000000u) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
        for (uint32_t i = 0; i < scount; ++i) {
            SignSave ss;
            ss.x = r.i32();
            ss.y = r.i32();
            ss.z = r.i32();
            for (std::string& line : ss.text.lines) {
                const int len = r.u8();
                for (int k = 0; k < len; ++k) {
                    const char ch = static_cast<char>(r.u8());
                    if (ch >= 32 && ch < 127 && line.size() < SignText::MAX_CHARS) line += ch;
                }
            }
            if (!r.ok()) {
                if (error) *error = "Beschaedigte Speicherdatei";
                return false;
            }
            if (!ss.text.empty()) save.signs.push_back(ss);
        }
    }
    if (version >= 7) {
        const uint32_t mcount = r.u32();
        if (!r.ok() || mcount > 1000000u) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
        for (uint32_t i = 0; i < mcount; ++i) {
            MachineSave ms;
            ms.x = r.i32();
            ms.y = r.i32();
            ms.z = r.i32();
            const int kind = r.u8();
            ms.m.kind = static_cast<MachineKind>(kind <= 4 ? kind : 0);
            for (ItemStack& st : ms.m.slots) {
                ItemId id = r.u16();
                int cnt = r.u8();
                int dmg = r.u16();
                if (itemValid(id) && cnt > 0) {
                    st.id = id;
                    st.count = std::min(cnt, itemInfo(id).maxStack);
                    st.damage = std::min(dmg, std::max(0, itemInfo(id).maxDurability));
                }
            }
            auto fl = [&](float lo, float hi) { return std::max(lo, std::min(hi, r.f32())); };
            ms.m.water = fl(0.0f, BOILER_WATER_MAX);
            ms.m.purity = fl(0.0f, 1.0f);
            ms.m.burnTime = fl(0.0f, 100000.0f);
            ms.m.burnTotal = fl(0.0f, 100000.0f);
            ms.m.progress = fl(0.0f, 1.0f);
            if (!r.ok()) {
                if (error) *error = "Beschaedigte Speicherdatei";
                return false;
            }
            save.machines.push_back(ms);
        }
    }
    if (version >= 8) {
        const uint32_t vcount = r.u32();
        if (!r.ok() || vcount > 100000u) {
            if (error) *error = "Beschaedigte Speicherdatei";
            return false;
        }
        for (uint32_t i = 0; i < vcount; ++i) {
            Vehicle v;
            const int type = r.u8();
            v.type = type == 1 ? VehicleType::Airship : VehicleType::Boat;
            v.pos.x = r.f32();
            v.pos.y = r.f32();
            v.pos.z = r.f32();
            v.yaw = r.f32();
            v.balloon = std::max(-1, std::min(2, static_cast<int>(r.u8()) - 1));
            v.propeller = r.u8() != 0;
            v.modules = std::max(1, std::min(AIRSHIP_MAX_MODULES, static_cast<int>(r.u8())));
            v.health = std::min(VEHICLE_HEALTH, static_cast<int>(r.u8()));
            v.anchored = r.u8() != 0;
            for (int k = 0; k < 2; ++k) {
                const ItemId id = r.u16();
                const int cnt = r.u8();
                if (itemValid(id) && cnt > 0) v.engine.slots[k] = {id, std::min(cnt, itemInfo(id).maxStack), 0};
            }
            auto fl = [&](float lo, float hi) { return std::max(lo, std::min(hi, r.f32())); };
            v.engine.water = fl(0.0f, BOILER_WATER_MAX);
            v.engine.purity = fl(0.0f, 1.0f);
            v.engine.burnTime = fl(0.0f, 100000.0f);
            v.engine.burnTotal = fl(0.0f, 100000.0f);
            if (!r.ok() || !std::isfinite(v.pos.x + v.pos.y + v.pos.z + v.yaw)) {
                if (error) *error = "Beschaedigte Speicherdatei";
                return false;
            }
            save.vehicles.push_back(v);
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
