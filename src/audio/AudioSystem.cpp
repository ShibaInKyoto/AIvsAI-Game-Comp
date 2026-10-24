#include "audio/AudioSystem.h"

#include "core/Settings.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace gb {

namespace {

// raylib's stream callback has no user pointer; the engine is reached through
// this atomic so it can be detached safely before destruction.
std::atomic<MusicEngine*> g_music{nullptr};

void MusicCallback(void* buffer, unsigned int frames) {
    MusicEngine* music = g_music.load();
    if (music != nullptr) {
        music->Render(static_cast<float*>(buffer), frames);
    } else {
        std::memset(buffer, 0, static_cast<std::size_t>(frames) * 2 * sizeof(float));
    }
}

Sound MakeSound(const std::vector<float>& samples) {
    std::vector<std::int16_t> pcm(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        pcm[i] = static_cast<std::int16_t>(std::clamp(samples[i], -1.0f, 1.0f) * 32767.0f);
    }
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(pcm.size());
    wave.sampleRate = kSfxSampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm.data();
    return LoadSoundFromWave(wave);  // copies the samples
}

} // namespace

AudioSystem::AudioSystem() : music_(std::make_unique<MusicEngine>()) {}

AudioSystem::~AudioSystem() { Shutdown(); }

bool AudioSystem::Init(bool enabled) {
    if (!enabled) {
        return true;
    }
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "AUDIO: no audio device, continuing silently");
        return false;
    }
    enabled_ = true;

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        const auto id = static_cast<SfxId>(i);
        Voice& voice = voices_[i];
        voice.source = MakeSound(SynthesizeSfx(id));
        voice.minInterval = SfxMinInterval(id);
        voice.aliases.push_back(voice.source);
        for (int k = 1; k < SfxPolyphony(id); ++k) {
            voice.aliases.push_back(LoadSoundAlias(voice.source));
        }
    }

    SetAudioStreamBufferSizeDefault(2048);
    stream_ = LoadAudioStream(MusicEngine::kSampleRate, 32, 2);
    g_music.store(music_.get());
    SetAudioStreamCallback(stream_, MusicCallback);
    PlayAudioStream(stream_);
    return true;
}

void AudioSystem::Shutdown() {
    if (!enabled_) {
        return;
    }
    StopAudioStream(stream_);
    UnloadAudioStream(stream_);
    g_music.store(nullptr);
    for (Voice& voice : voices_) {
        for (std::size_t k = 1; k < voice.aliases.size(); ++k) {
            UnloadSoundAlias(voice.aliases[k]);
        }
        UnloadSound(voice.source);
        voice.aliases.clear();
    }
    CloseAudioDevice();
    enabled_ = false;
}

void AudioSystem::Update(float dt) {
    for (Voice& voice : voices_) {
        voice.cooldown = std::max(0.0f, voice.cooldown - dt);
    }
}

void AudioSystem::ApplyVolumes(const Settings& settings) {
    sfxVolume_ = settings.sfxVolume;
    music_->SetVolume(settings.musicVolume);
    if (enabled_) {
        SetMasterVolume(settings.masterVolume);
    }
}

void AudioSystem::Play(SfxId id, float volume, float pitch, float pan) {
    if (!enabled_) {
        return;
    }
    Voice& voice = voices_[static_cast<std::size_t>(id)];
    if (voice.cooldown > 0.0f || voice.aliases.empty()) {
        return;
    }
    voice.cooldown = voice.minInterval;
    Sound& sound = voice.aliases[static_cast<std::size_t>(voice.next)];
    voice.next = (voice.next + 1) % static_cast<int>(voice.aliases.size());
    SetSoundVolume(sound, std::clamp(volume * sfxVolume_, 0.0f, 1.0f));
    SetSoundPitch(sound, std::clamp(pitch, 0.25f, 4.0f));
    // raylib's pan is the left-channel weight: 1 = left, 0 = right.
    SetSoundPan(sound, std::clamp(0.5f - pan * 0.5f, 0.0f, 1.0f));
    PlaySound(sound);
}

} // namespace gb
