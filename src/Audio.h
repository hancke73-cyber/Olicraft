#pragma once
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Sounds.h"

namespace oc {

// Tonausgabe mit miniaudio. Effekte liegen als Dateien in <ordner>/ (WAV, MP3 oder FLAC), Musik in <ordner>/music/.
// Fehlen Dateien oder ein Audiogeraet, bleibt das Spiel einfach still.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // useNullDevice: kein echtes Geraet (fuer Tests); die Verarbeitung laeuft trotzdem
    bool init(bool useNullDevice = false);
    void shutdown();
    bool active() const;

    // Laedt alle gefundenen Effekte und die Musikliste. Rueckgabe: Anzahl Ereignisse mit mindestens einer Datei.
    int loadSounds(const std::string& dir);
    int variantCount(SoundId id) const;
    int musicTrackCount() const;

    void setVolumes(float master, float effects, float music);
    void setListener(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up);

    // Spielt ein Ereignis (zufaellige Variante). spatial = false: ohne Richtung (Oberflaeche)
    bool play(SoundId id, const glm::vec3& pos, float volume = 1.0f, float pitch = 1.0f, bool spatial = true);
    bool playUi(SoundId id, float volume = 1.0f) { return play(id, glm::vec3(0.0f), volume, 1.0f, false); }

    // Aufraeumen beendeter Stimmen, Musikablauf
    void update(float dt);
    void setMusicEnabled(bool on);
    void stopAll();

    int voiceCount() const;
    bool musicPlaying() const;
    uint64_t playedCount() const { return played_; }  // wie viele Effekte seit dem Start abgespielt wurden

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    uint64_t played_ = 0;
};

}  // namespace oc
