#include "Audio.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>

// miniaudio (gemeinfrei), siehe third_party/miniaudio
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace oc {

namespace fs = std::filesystem;

struct AudioEngine::Impl {
    ma_context context;  // nur beim Null-Geraet benutzt; muss so lange leben wie die Engine
    bool contextInit = false;
    ma_engine engine;
    bool engineInit = false;
    ma_sound_group effects, music;
    bool groupsInit = false;

    struct Template {
        std::string path;
        std::unique_ptr<ma_sound> sound;
    };
    std::vector<Template> templates[SOUND_COUNT];

    struct Voice {
        std::unique_ptr<ma_sound> sound;
    };
    std::vector<Voice> voices;
    static constexpr size_t MAX_VOICES = 40;

    std::vector<std::string> musicTracks;
    std::unique_ptr<ma_sound> musicSound;
    bool musicEnabled = true;
    float musicTimer = 25.0f;  // erste Musik kurz nach dem Start
    int lastTrack = -1;

    std::mt19937 rng{std::random_device{}()};
    float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
};

AudioEngine::AudioEngine() : impl_(new Impl) {}
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::active() const { return impl_ && impl_->engineInit; }

bool AudioEngine::init(bool useNullDevice) {
    if (active()) return true;
    ma_engine_config cfg = ma_engine_config_init();
    if (useNullDevice) {
        ma_backend backends[] = {ma_backend_null};
        if (ma_context_init(backends, 1, nullptr, &impl_->context) != MA_SUCCESS) return false;
        impl_->contextInit = true;
        cfg.pContext = &impl_->context;
    }
    cfg.listenerCount = 1;
    if (ma_engine_init(&cfg, &impl_->engine) != MA_SUCCESS) {
        if (impl_->contextInit) {
            ma_context_uninit(&impl_->context);
            impl_->contextInit = false;
        }
        return false;
    }
    impl_->engineInit = true;
    if (ma_sound_group_init(&impl_->engine, 0, nullptr, &impl_->effects) != MA_SUCCESS ||
        ma_sound_group_init(&impl_->engine, 0, nullptr, &impl_->music) != MA_SUCCESS) {
        shutdown();
        return false;
    }
    impl_->groupsInit = true;
    return true;
}

void AudioEngine::stopAll() {
    if (!active()) return;
    for (auto& v : impl_->voices) ma_sound_uninit(v.sound.get());
    impl_->voices.clear();
    if (impl_->musicSound) {
        ma_sound_uninit(impl_->musicSound.get());
        impl_->musicSound.reset();
    }
}

void AudioEngine::shutdown() {
    if (!impl_ || !impl_->engineInit) return;
    stopAll();
    for (auto& list : impl_->templates)
        for (auto& t : list) ma_sound_uninit(t.sound.get());
    for (auto& list : impl_->templates) list.clear();
    if (impl_->groupsInit) {
        ma_sound_group_uninit(&impl_->effects);
        ma_sound_group_uninit(&impl_->music);
        impl_->groupsInit = false;
    }
    ma_engine_uninit(&impl_->engine);
    impl_->engineInit = false;
    if (impl_->contextInit) {
        ma_context_uninit(&impl_->context);
        impl_->contextInit = false;
    }
}

int AudioEngine::loadSounds(const std::string& dir) {
    if (!active()) return 0;
    static const char* EXT[] = {".wav", ".flac", ".mp3"};
    int found = 0;
    std::error_code ec;
    for (int i = 0; i < SOUND_COUNT; ++i) {
        auto& list = impl_->templates[i];
        for (auto& t : list) ma_sound_uninit(t.sound.get());
        list.clear();
        const std::string base = dir + "/" + soundDef(static_cast<SoundId>(i)).file;
        std::vector<std::string> files;
        for (const char* ext : EXT) {
            if (fs::exists(base + ext, ec)) files.push_back(base + ext);
            for (int v = 1; v <= 8; ++v) {
                const std::string p = base + "_" + std::to_string(v) + ext;
                if (fs::exists(p, ec)) files.push_back(p);
            }
        }
        for (const std::string& path : files) {
            auto snd = std::make_unique<ma_sound>();
            // Effekte werden vollstaendig dekodiert und im Speicher gehalten
            if (ma_sound_init_from_file(&impl_->engine, path.c_str(), MA_SOUND_FLAG_DECODE, &impl_->effects, nullptr,
                                        snd.get()) != MA_SUCCESS) {
                std::fprintf(stderr, "Sound nicht lesbar: %s\n", path.c_str());
                continue;
            }
            list.push_back({path, std::move(snd)});
        }
        if (!list.empty()) ++found;
    }
    // Musik
    impl_->musicTracks.clear();
    const std::string mdir = dir + "/music";
    if (fs::is_directory(mdir, ec)) {
        for (const auto& e : fs::directory_iterator(mdir, ec)) {
            if (!e.is_regular_file()) continue;
            std::string ext = e.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".wav" || ext == ".flac" || ext == ".mp3") impl_->musicTracks.push_back(e.path().string());
        }
        std::sort(impl_->musicTracks.begin(), impl_->musicTracks.end());
    }
    return found;
}

int AudioEngine::variantCount(SoundId id) const { return static_cast<int>(impl_->templates[static_cast<int>(id)].size()); }
int AudioEngine::musicTrackCount() const { return static_cast<int>(impl_->musicTracks.size()); }

void AudioEngine::setVolumes(float master, float effects, float music) {
    if (!active()) return;
    master = std::max(0.0f, std::min(1.0f, master));
    ma_engine_set_volume(&impl_->engine, master);
    ma_sound_group_set_volume(&impl_->effects, std::max(0.0f, std::min(1.0f, effects)));
    ma_sound_group_set_volume(&impl_->music, std::max(0.0f, std::min(1.0f, music)));
}

void AudioEngine::setListener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up) {
    if (!active()) return;
    ma_engine_listener_set_position(&impl_->engine, 0, pos.x, pos.y, pos.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl_->engine, 0, up.x, up.y, up.z);
}

bool AudioEngine::play(SoundId id, const glm::vec3& pos, float volume, float pitch, bool spatial) {
    if (!active()) return false;
    auto& list = impl_->templates[static_cast<int>(id)];
    if (list.empty()) return false;
    if (impl_->voices.size() >= Impl::MAX_VOICES) return false;
    const size_t pick = list.size() == 1 ? 0 : static_cast<size_t>(impl_->rng() % list.size());
    auto snd = std::make_unique<ma_sound>();
    if (ma_sound_init_copy(&impl_->engine, list[pick].sound.get(), 0, &impl_->effects, snd.get()) != MA_SUCCESS) return false;
    ma_sound_set_volume(snd.get(), std::max(0.0f, volume) * soundDef(id).baseVolume);
    ma_sound_set_pitch(snd.get(), std::max(0.25f, pitch) * impl_->rnd(0.94f, 1.06f));
    ma_sound_set_spatialization_enabled(snd.get(), spatial ? MA_TRUE : MA_FALSE);
    if (spatial) {
        ma_sound_set_position(snd.get(), pos.x, pos.y, pos.z);
        ma_sound_set_attenuation_model(snd.get(), ma_attenuation_model_inverse);
        ma_sound_set_min_distance(snd.get(), 2.0f);
        ma_sound_set_max_distance(snd.get(), 36.0f);
        ma_sound_set_rolloff(snd.get(), 1.2f);
    }
    if (ma_sound_start(snd.get()) != MA_SUCCESS) {
        ma_sound_uninit(snd.get());
        return false;
    }
    impl_->voices.push_back({std::move(snd)});
    ++played_;
    return true;
}

void AudioEngine::setMusicEnabled(bool on) {
    impl_->musicEnabled = on;
    if (!on && impl_->musicSound) {
        ma_sound_uninit(impl_->musicSound.get());
        impl_->musicSound.reset();
    }
}

void AudioEngine::update(float dt) {
    if (!active()) return;
    for (size_t i = 0; i < impl_->voices.size();) {
        if (ma_sound_at_end(impl_->voices[i].sound.get())) {
            ma_sound_uninit(impl_->voices[i].sound.get());
            impl_->voices.erase(impl_->voices.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }
    // Musik: ein Stueck, dann eine laengere Pause (wie in Minecraft), Reihenfolge zufaellig
    if (impl_->musicSound && ma_sound_at_end(impl_->musicSound.get())) {
        ma_sound_uninit(impl_->musicSound.get());
        impl_->musicSound.reset();
        impl_->musicTimer = impl_->rnd(60.0f, 180.0f);
    }
    if (!impl_->musicSound && impl_->musicEnabled && !impl_->musicTracks.empty()) {
        impl_->musicTimer -= dt;
        if (impl_->musicTimer <= 0.0f) {
            int pick = static_cast<int>(impl_->rng() % impl_->musicTracks.size());
            if (impl_->musicTracks.size() > 1 && pick == impl_->lastTrack) pick = (pick + 1) % static_cast<int>(impl_->musicTracks.size());
            impl_->lastTrack = pick;
            auto snd = std::make_unique<ma_sound>();
            if (ma_sound_init_from_file(&impl_->engine, impl_->musicTracks[static_cast<size_t>(pick)].c_str(),
                                        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, &impl_->music, nullptr,
                                        snd.get()) == MA_SUCCESS &&
                ma_sound_start(snd.get()) == MA_SUCCESS) {
                impl_->musicSound = std::move(snd);
            } else {
                impl_->musicTimer = 30.0f;  // Datei kaputt: spaeter erneut versuchen
            }
        }
    }
}

int AudioEngine::voiceCount() const { return impl_ ? static_cast<int>(impl_->voices.size()) : 0; }
bool AudioEngine::musicPlaying() const { return impl_ && impl_->musicSound != nullptr; }

}  // namespace oc
