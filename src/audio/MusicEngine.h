#pragma once

#include "core/Random.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace gb {

enum class MusicMood : std::uint8_t {
    Silent,
    Menu,
    Game,
    Boss,
    Victory,
    GameOver
};

// Real-time procedural synthwave. A step sequencer drives bass, pads, arps,
// lead and drums; layers fade in and out with gameplay intensity, and the
// chord progression, motifs and key change per sector.
//
// Threading: Set*() are called from the game thread and only touch atomics;
// Render() runs on the audio thread and owns all synthesis state.
class MusicEngine {
public:
    static constexpr int kSampleRate = 44100;

    MusicEngine();

    void SetMood(MusicMood mood, int keyOffset = 0);
    void SetIntensity(float intensity);
    void SetVolume(float volume);

    // Fills `frames` interleaved stereo float frames.
    void Render(float* out, unsigned int frames);

private:
    struct Envelope {
        float level = 0.0f;
        float attack = 0.005f;
        float decay = 0.2f;
        float sustain = 0.0f;
        float release = 0.1f;
        bool gate = false;
        bool attacking = false;
        void Trigger() {
            gate = true;
            attacking = true;
        }
        void Release() {
            gate = false;
            attacking = false;
        }
        float Next();
    };

    struct Svf {
        float ic1 = 0.0f;
        float ic2 = 0.0f;
        float Lowpass(float input, float cutoff, float resonance);
    };

    struct Voice {
        float phase = 0.0f;
        float phase2 = 0.0f;
        float frequency = 110.0f;
        float targetFrequency = 110.0f;
        Envelope amp;
        Envelope filterEnv;
        Svf filter;
    };

    struct NoteEvent {
        int step;      // 16th within the 2-bar motif
        int length;    // in 16ths
        int chordTone; // index into the current chord (0..5)
    };

    void ApplyPendingMood();
    void BuildSection();
    void OnStep();
    void RenderSample(float& left, float& right);
    int ChordNote(int chordTone, int octave) const;
    float MidiToHz(float note) const;

    // Shared with the game thread.
    std::atomic<int> requestedMood_{static_cast<int>(MusicMood::Silent)};
    std::atomic<int> requestedKey_{0};
    std::atomic<float> intensityTarget_{0.0f};
    std::atomic<float> volume_{0.7f};

    // Audio-thread state.
    MusicMood mood_ = MusicMood::Silent;
    int key_ = 0;
    float bpm_ = 110.0f;
    double samplesPerStep_ = 0.0;
    double stepAccumulator_ = 0.0;
    int step_ = 0; // 16th notes since the section began
    int bar_ = 0;
    int section_ = 0;
    float intensity_ = 0.0f;
    float moodGain_ = 0.0f; // fades during mood changes
    bool switching_ = false;
    Rng rng_{0xB16B00B5u};

    std::array<int, 4> progression_{};
    std::array<int, 16> arpPattern_{};
    std::array<int, 16> bassPattern_{}; // semitone offsets, -1 = rest
    std::array<NoteEvent, 12> motif_{};
    int motifLength_ = 0;
    bool majorMode_ = false;

    // Layer gains (smoothed toward targets derived from intensity).
    float bassGain_ = 0.0f;
    float arpGain_ = 0.0f;
    float kickGain_ = 0.0f;
    float snareGain_ = 0.0f;
    float hatGain_ = 0.0f;
    float leadGain_ = 0.0f;
    float padGain_ = 0.0f;

    Voice bass_;
    std::array<Voice, 4> pad_;
    Voice arp_;
    Voice lead_;
    Svf padFilter_;
    Svf arpFilter_;
    float padLfo_ = 0.0f;

    // Drums
    float kickPhase_ = 0.0f;
    float kickTime_ = 10.0f;
    float snareTime_ = 10.0f;
    float hatTime_ = 10.0f;
    float hatDecay_ = 0.04f;
    float noiseHp_ = 0.0f;
    float noiseHpIn_ = 0.0f;
    Rng noise_{0x1234567u};

    // Stereo ping-pong delay for arp and lead.
    static constexpr int kDelaySize = kSampleRate; // up to 1 second
    std::array<float, kDelaySize> delayLeft_{};
    std::array<float, kDelaySize> delayRight_{};
    int delayIndex_ = 0;
    int delaySamples_ = 0;
};

} // namespace gb
