#include "Worlds.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#include "Save.h"

namespace oc {

namespace fs = std::filesystem;

bool worldNameChar(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
}

static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string sanitizeWorldName(const std::string& in) {
    std::string s;
    for (char c : in)
        if (worldNameChar(static_cast<unsigned char>(c))) s += c;
    const size_t b = s.find_first_not_of(' ');
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(' ');
    s = s.substr(b, e - b + 1);
    if (s.size() > MAX_WORLD_NAME) {
        s.resize(MAX_WORLD_NAME);
        const size_t e2 = s.find_last_not_of(' ');
        s.resize(e2 + 1);
    }
    // reservierte Geraetenamen unter Windows (auch mit Endung gesperrt)
    static const char* RESERVED[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5",
                                     "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5",
                                     "lpt6", "lpt7", "lpt8", "lpt9"};
    const std::string l = lower(s);
    for (const char* r : RESERVED)
        if (l == r) return s + "_";
    return s;
}

std::string worldPathFor(const std::string& dir, const std::string& name) {
    return (fs::path(dir) / (name + ".dat")).generic_string();
}

std::string uniqueWorldName(const std::string& dir, const std::string& base) {
    std::string b = sanitizeWorldName(base);
    if (b.empty()) b = "Neue Welt";
    // vorhandene Namen einlesen, damit "welt" und "Welt" nicht kollidieren (Windows ist nicht gross/klein-empfindlich)
    std::vector<std::string> existing;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file(ec) && e.path().extension() == ".dat") existing.push_back(lower(e.path().stem().string()));
    auto taken = [&](const std::string& n) { return std::find(existing.begin(), existing.end(), lower(n)) != existing.end(); };
    if (!taken(b)) return b;
    for (int i = 2; i < 10000; ++i) {
        const std::string suffix = " " + std::to_string(i);
        std::string stem = b;
        if (stem.size() + suffix.size() > MAX_WORLD_NAME) stem.resize(MAX_WORLD_NAME - suffix.size());
        while (!stem.empty() && stem.back() == ' ') stem.pop_back();
        const std::string cand = stem + suffix;
        if (!taken(cand)) return cand;
    }
    return b;
}

std::vector<WorldEntry> listWorlds(const std::string& dir) {
    std::vector<WorldEntry> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec) || e.path().extension() != ".dat") continue;
        SaveInfo info;
        if (!readSaveInfo(e.path().string(), info)) continue;
        WorldEntry w;
        w.name = e.path().stem().string();
        w.path = e.path().generic_string();
        w.seed = info.seed;
        w.creative = info.creative;
        w.modified = static_cast<int64_t>(e.last_write_time(ec).time_since_epoch().count());
        out.push_back(w);
    }
    std::sort(out.begin(), out.end(), [](const WorldEntry& a, const WorldEntry& b) {
        if (a.modified != b.modified) return a.modified > b.modified;
        return a.name < b.name;
    });
    return out;
}

}  // namespace oc
