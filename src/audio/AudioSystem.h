#pragma once

#include "audio/MusicEngine.h"
#include "audio/SoundBank.h"

#include <raylib.h>

#include <array>
#include <memory>
#include <vector>

namespace gb {

struct Settings;

// Owns the audio device, the procedurally synthesised sound bank and the
// adaptive music engine. Every sound is generated at startup: the game ships
// with no audio files at all.
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool Init(bool enabled);
    void Shutdown();
    void Update(float dt);
    void ApplyVolumes(const Settings& settings);

    // pan: -1 (left) .. 1 (right). Rate-limited per sound so dense combat
    // never turns into a wall of identical clicks.
    void Play(SfxId id, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f);

    MusicEngine& Music() { return *music_; }
    bool Enabled() const { return enabled_; }

private:
    struct Voice {
        std::vector<Sound> aliases;
        Sound source{};
        int next = 0;
        float cooldown = 0.0f;
        float minInterval = 0.0f;
    };

    bool enabled_ = false;
    float sfxVolume_ = 1.0f;
    std::array<Voice, static_cast<std::size_t>(SfxId::Count)> voices_{};
    std::unique_ptr<MusicEngine> music_;
    AudioStream stream_{};
};

} // namespace gb
