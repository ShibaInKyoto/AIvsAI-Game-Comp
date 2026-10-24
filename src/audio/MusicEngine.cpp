#include "audio/MusicEngine.h"

#include <algorithm>
#include <cmath>

namespace gb {

namespace {

constexpr float kSr = static_cast<float>(MusicEngine::kSampleRate);
constexpr std::array<int, 7> kMinorScale{0, 2, 3, 5, 7, 8, 10};
constexpr std::array<int, 7> kMajorScale{0, 2, 4, 5, 7, 9, 11};
constexpr int kBassRoot = 33;  // A1

// Chord progressions as scale degrees (0 = tonic).
constexpr std::array<std::array<int, 4>, 6> kMinorProgressions{{
    {0, 5, 2, 6},  // i  VI III VII
    {0, 3, 5, 4},  // i  iv VI  v
    {0, 6, 5, 6},  // i  VII VI VII
    {5, 6, 0, 0},  // VI VII i  i
    {0, 2, 6, 3},  // i  III VII iv
    {3, 5, 6, 0},  // iv VI VII i
}};
constexpr std::array<std::array<int, 4>, 2> kMajorProgressions{{
    {0, 4, 5, 3},  // I V vi IV
    {3, 4, 0, 0},  // IV V I I
}};

constexpr std::array<std::array<int, 16>, 4> kArpPatterns{{
    {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
    {0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1},
    {0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5},
    {3, 2, 1, 0, 3, 2, 1, 0, 4, 3, 2, 1, 4, 3, 2, 1},
}};

constexpr std::array<std::array<int, 16>, 3> kBassPatterns{{
    {0, -1, 0, -1, 0, -1, 12, -1, 0, -1, 0, -1, 0, -1, 12, -1},
    {0, -1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 12, -1},
    {0, 0, 12, 0, 0, 12, 0, 0, 0, 0, 12, 0, 0, 12, 0, 12},
}};

// Two-bar rhythm templates for the lead (step, length).
constexpr std::array<std::array<std::pair<int, int>, 8>, 4> kLeadRhythms{{
    {{{0, 4}, {4, 2}, {6, 2}, {8, 6}, {16, 4}, {20, 2}, {22, 2}, {24, 8}}},
    {{{0, 3}, {3, 3}, {6, 2}, {8, 8}, {16, 3}, {19, 3}, {22, 2}, {24, 8}}},
    {{{0, 2}, {2, 2}, {4, 4}, {8, 4}, {12, 4}, {16, 6}, {22, 2}, {24, 8}}},
    {{{0, 6}, {6, 2}, {8, 2}, {10, 6}, {16, 6}, {22, 2}, {24, 4}, {28, 4}}},
}};

float PolyBlep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float Saw(float phase, float dt) { return 2.0f * phase - 1.0f - PolyBlep(phase, dt); }

float Pulse(float phase, float dt, float duty) {
    float v = phase < duty ? 1.0f : -1.0f;
    v += PolyBlep(phase, dt);
    float shifted = phase + (1.0f - duty);
    shifted -= std::floor(shifted);
    v -= PolyBlep(shifted, dt);
    return v;
}

float Advance(float& phase, float frequency) {
    const float dt = frequency / kSr;
    phase += dt;
    phase -= std::floor(phase);
    return dt;
}

float Smooth(float current, float target, float rate) { return current + (target - current) * rate; }

} // namespace

float MusicEngine::Envelope::Next() {
    if (gate) {
        if (attacking) {
            level += 1.0f / std::max(attack * kSr, 1.0f);
            if (level >= 1.0f) {
                level = 1.0f;
                attacking = false;
            }
        } else {
            level += (sustain - level) * (1.0f - std::exp(-1.0f / std::max(decay * kSr, 1.0f)));
        }
    } else {
        level -= level * (1.0f - std::exp(-1.0f / std::max(release * kSr, 1.0f)));
    }
    return level;
}

float MusicEngine::Svf::Lowpass(float input, float cutoff, float resonance) {
    const float g = std::tan(kPi * std::clamp(cutoff, 20.0f, kSr * 0.45f) / kSr);
    const float k = 2.0f - 2.0f * std::clamp(resonance, 0.0f, 0.95f);
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = input - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    return v2;
}

MusicEngine::MusicEngine() {
    bass_.amp = {0.0f, 0.004f, 0.18f, 0.55f, 0.08f};
    bass_.filterEnv = {0.0f, 0.002f, 0.14f, 0.0f, 0.1f};
    arp_.amp = {0.0f, 0.002f, 0.11f, 0.0f, 0.05f};
    lead_.amp = {0.0f, 0.02f, 0.4f, 0.7f, 0.18f};
    for (Voice& v : pad_) {
        v.amp = {0.0f, 0.6f, 1.0f, 1.0f, 1.2f};
    }
    BuildSection();
    samplesPerStep_ = kSr * 60.0 / bpm_ / 4.0;
}

void MusicEngine::SetMood(MusicMood mood, int keyOffset) {
    requestedMood_.store(static_cast<int>(mood));
    requestedKey_.store(keyOffset);
}

void MusicEngine::SetIntensity(float intensity) { intensityTarget_.store(std::clamp(intensity, 0.0f, 1.0f)); }

void MusicEngine::SetVolume(float volume) { volume_.store(std::clamp(volume, 0.0f, 1.0f)); }

void MusicEngine::ApplyPendingMood() {
    mood_ = static_cast<MusicMood>(requestedMood_.load());
    key_ = requestedKey_.load();
    majorMode_ = mood_ == MusicMood::Victory;
    switch (mood_) {
    case MusicMood::Menu: bpm_ = 100.0f; break;
    case MusicMood::Game: bpm_ = 118.0f; break;
    case MusicMood::Boss: bpm_ = 134.0f; break;
    case MusicMood::Victory: bpm_ = 112.0f; break;
    case MusicMood::GameOver: bpm_ = 76.0f; break;
    case MusicMood::Silent: bpm_ = 100.0f; break;
    }
    samplesPerStep_ = kSr * 60.0 / bpm_ / 4.0;
    // Dotted-eighth echo locked to the tempo.
    delaySamples_ = std::min(kDelaySize - 1, static_cast<int>(samplesPerStep_ * 3.0));
    step_ = 0;
    bar_ = 0;
    stepAccumulator_ = 0.0;
    section_++;
    BuildSection();
}

void MusicEngine::BuildSection() {
    if (majorMode_) {
        progression_ = kMajorProgressions[static_cast<std::size_t>(rng_.RangeInt(0, 1))];
    } else if (mood_ == MusicMood::Boss) {
        progression_ = rng_.Chance(0.5f) ? std::array<int, 4>{0, 0, 5, 6} : std::array<int, 4>{0, 5, 3, 4};
    } else {
        progression_ = kMinorProgressions[static_cast<std::size_t>(rng_.RangeInt(0, 5))];
    }
    arpPattern_ = kArpPatterns[static_cast<std::size_t>(rng_.RangeInt(0, 3))];
    bassPattern_ = kBassPatterns[static_cast<std::size_t>(mood_ == MusicMood::Boss ? 2 : rng_.RangeInt(0, 1))];

    // Lead motif in chord-tone space, so it re-harmonises over every chord.
    const auto& rhythm = kLeadRhythms[static_cast<std::size_t>(rng_.RangeInt(0, 3))];
    motifLength_ = 0;
    int tone = rng_.RangeInt(1, 3);
    for (const auto& [stepIndex, length] : rhythm) {
        if (rng_.Chance(0.12f) && motifLength_ > 0) {
            continue;  // occasional rest
        }
        tone = std::clamp(tone + rng_.RangeInt(-2, 2), 0, 5);
        if (stepIndex % 8 == 0) {
            tone = (tone / 2) * 2;  // land on stronger chord tones on strong beats
        }
        motif_[static_cast<std::size_t>(motifLength_++)] = NoteEvent{stepIndex, length, tone};
    }
}

int MusicEngine::ChordNote(int chordTone, int octave) const {
    const auto& scale = majorMode_ ? kMajorScale : kMinorScale;
    const int degree = progression_[static_cast<std::size_t>(bar_ % 4)];
    // Chord tones stack thirds from the chord degree: 0=root 1=third 2=fifth 3=root+8va ...
    const int scaleIndex = degree + (chordTone % 3) * 2;
    const int octaves = scaleIndex / 7 + chordTone / 3 + octave;
    return kBassRoot + key_ + scale[static_cast<std::size_t>(scaleIndex % 7)] + 12 * octaves;
}

float MusicEngine::MidiToHz(float note) const { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }

void MusicEngine::OnStep() {
    const int stepInBar = step_ % 16;
    if (stepInBar == 0) {
        bar_ = (step_ / 16);
        if (bar_ > 0 && bar_ % 8 == 0) {
            BuildSection();
        }
        // New chord: pads glide to the new voicing.
        for (std::size_t i = 0; i < pad_.size(); ++i) {
            const int tone = static_cast<int>(i);
            pad_[i].targetFrequency = MidiToHz(static_cast<float>(ChordNote(tone, 2)));
            if (!pad_[i].amp.gate) {
                pad_[i].frequency = pad_[i].targetFrequency;
                pad_[i].amp.Trigger();
            }
        }
    }

    // Bass
    const int bassOffset = bassPattern_[static_cast<std::size_t>(stepInBar)];
    const bool sparseBass = intensity_ < 0.35f;
    if (bassOffset >= 0 && (!sparseBass || stepInBar % 4 == 0)) {
        bass_.frequency = MidiToHz(static_cast<float>(ChordNote(0, 0) + bassOffset));
        bass_.amp.Trigger();
        bass_.filterEnv.Trigger();
    } else if (bassOffset < 0) {
        bass_.amp.Release();
    }

    // Arp
    const int arpTone = arpPattern_[static_cast<std::size_t>(stepInBar)];
    arp_.frequency = MidiToHz(static_cast<float>(ChordNote(arpTone, 3)));
    arp_.amp.Trigger();

    // Lead motif (two bars long).
    const int motifStep = step_ % 32;
    lead_.amp.Release();
    for (int i = 0; i < motifLength_; ++i) {
        const NoteEvent& n = motif_[static_cast<std::size_t>(i)];
        if (motifStep >= n.step && motifStep < n.step + n.length) {
            const float freq = MidiToHz(static_cast<float>(ChordNote(n.chordTone, 3)));
            if (motifStep == n.step) {
                lead_.targetFrequency = freq;
                lead_.amp.Trigger();
            } else {
                lead_.amp.gate = true;
            }
            break;
        }
    }

    // Drums
    const bool halfTime = mood_ == MusicMood::Menu || mood_ == MusicMood::GameOver;
    const bool kick = halfTime ? (stepInBar == 0 || stepInBar == 10) : (stepInBar % 4 == 0);
    if (kick) {
        kickTime_ = 0.0f;
        kickPhase_ = 0.0f;
    }
    const bool fillBar = (bar_ % 8 == 7) && intensity_ > 0.6f;
    const bool snare = (stepInBar == 4 || stepInBar == 12) || (fillBar && stepInBar >= 12);
    if (snare && !halfTime) {
        snareTime_ = 0.0f;
    } else if (halfTime && stepInBar == 8) {
        snareTime_ = 0.0f;
    }
    const bool sixteenths = intensity_ > 0.72f;
    if ((stepInBar % 2 == 0) || sixteenths) {
        hatTime_ = 0.0f;
        hatDecay_ = (stepInBar % 4 == 2) ? 0.12f : 0.035f;
    }
}

void MusicEngine::RenderSample(float& left, float& right) {
    // --- Sequencer clock ---
    stepAccumulator_ += 1.0;
    if (stepAccumulator_ >= samplesPerStep_) {
        stepAccumulator_ -= samplesPerStep_;
        ++step_;
        OnStep();
    }

    // --- Kick (also drives sidechain ducking) ---
    kickTime_ += 1.0f / kSr;
    float kick = 0.0f;
    float duck = 1.0f;
    if (kickTime_ < 0.5f) {
        const float pitch = 45.0f + 110.0f * std::exp(-kickTime_ * 38.0f);
        kickPhase_ += pitch / kSr;
        kickPhase_ -= std::floor(kickPhase_);
        const float env = std::exp(-kickTime_ * 7.5f);
        kick = std::sin(kTau * kickPhase_) * env;
        if (kickTime_ < 0.003f) {
            kick += noise_.Range(-0.6f, 0.6f);
        }
        duck = 1.0f - 0.55f * std::exp(-kickTime_ * 9.0f) * kickGain_;
    }

    // --- Snare & hats from one noise source ---
    const float white = noise_.Range(-1.0f, 1.0f);
    snareTime_ += 1.0f / kSr;
    hatTime_ += 1.0f / kSr;
    float snare = 0.0f;
    if (snareTime_ < 0.3f) {
        snare = white * std::exp(-snareTime_ * 18.0f) * 0.8f +
                std::sin(kTau * 185.0f * snareTime_) * std::exp(-snareTime_ * 30.0f) * 0.6f;
    }
    // One-pole high-pass for crisp hats.
    const float hpA = 0.85f;
    noiseHp_ = hpA * (noiseHp_ + white - noiseHpIn_);
    noiseHpIn_ = white;
    float hat = 0.0f;
    if (hatTime_ < 0.25f) {
        hat = noiseHp_ * std::exp(-hatTime_ / hatDecay_) * 0.5f;
    }

    // --- Bass: saw + sub through a resonant low-pass with envelope ---
    const float bassDt = Advance(bass_.phase, bass_.frequency);
    Advance(bass_.phase2, bass_.frequency * 0.5f);
    float bass = Saw(bass_.phase, bassDt) * 0.6f + std::sin(kTau * bass_.phase2) * 0.7f;
    const float bassCutoff = 180.0f + 1700.0f * bass_.filterEnv.Next() + 400.0f * intensity_;
    bass = bass_.filter.Lowpass(bass, bassCutoff, 0.35f) * bass_.amp.Next();

    // --- Pads: detuned saws, gliding chords, slow filter LFO ---
    padLfo_ += 0.08f / kSr;
    padLfo_ -= std::floor(padLfo_);
    float padL = 0.0f;
    float padR = 0.0f;
    for (std::size_t i = 0; i < pad_.size(); ++i) {
        Voice& v = pad_[i];
        v.frequency += (v.targetFrequency - v.frequency) * 0.0009f;
        const float dt1 = Advance(v.phase, v.frequency * 1.0045f);
        const float dt2 = Advance(v.phase2, v.frequency * 0.9955f);
        const float env = v.amp.Next();
        padL += Saw(v.phase, dt1) * env;
        padR += Saw(v.phase2, dt2) * env;
    }
    const float padCutoff = 700.0f + 900.0f * (0.5f + 0.5f * std::sin(kTau * padLfo_)) + 600.0f * intensity_;
    const float padMono = padFilter_.Lowpass((padL + padR) * 0.5f, padCutoff, 0.2f);
    padL = padMono * 0.85f + (padL - padR) * 0.05f;
    padR = padMono * 0.85f - (padL - padR) * 0.05f;

    // --- Arp: bright pulse through a low-pass ---
    const float arpDt = Advance(arp_.phase, arp_.frequency);
    float arp = Pulse(arp_.phase, arpDt, 0.28f) * arp_.amp.Next();
    arp = arpFilter_.Lowpass(arp, 2600.0f + 1800.0f * intensity_, 0.25f);

    // --- Lead: saw/square blend with vibrato and portamento ---
    lead_.frequency += (lead_.targetFrequency - lead_.frequency) * 0.004f;
    // phase2 doubles as the vibrato LFO for the lead voice.
    const float vibrato = 1.0f + 0.006f * std::sin(lead_.phase2);
    lead_.phase2 += kTau * 5.5f / kSr;
    if (lead_.phase2 > kTau) {
        lead_.phase2 -= kTau;
    }
    const float leadDt = Advance(lead_.phase, lead_.frequency * vibrato);
    float lead = (Saw(lead_.phase, leadDt) * 0.5f + Pulse(lead_.phase, leadDt, 0.5f) * 0.5f) * lead_.amp.Next();
    lead = lead_.filter.Lowpass(lead, 3200.0f, 0.15f);

    // --- Layer gains follow intensity ---
    const float i = intensity_;
    const float rate = 1.0f / (kSr * 0.6f);
    const bool lead_on = mood_ == MusicMood::Boss || mood_ == MusicMood::Victory || i > 0.82f;
    padGain_ = Smooth(padGain_, mood_ == MusicMood::Silent ? 0.0f : (0.55f - 0.15f * i), rate);
    bassGain_ = Smooth(bassGain_, i > 0.12f ? 0.75f : 0.0f, rate);
    kickGain_ = Smooth(kickGain_, i > 0.28f ? 0.9f : 0.0f, rate);
    hatGain_ = Smooth(hatGain_, i > 0.3f ? 0.35f : 0.0f, rate);
    snareGain_ = Smooth(snareGain_, i > 0.5f ? 0.55f : 0.0f, rate);
    arpGain_ = Smooth(arpGain_, i > 0.42f ? 0.22f : 0.0f, rate);
    leadGain_ = Smooth(leadGain_, lead_on ? 0.2f : 0.0f, rate);

    // --- Echo bus (ping-pong) for arp and lead ---
    const float send = arp * arpGain_ + lead * leadGain_ * 0.7f;
    const int readIndex = (delayIndex_ - delaySamples_ + kDelaySize) % kDelaySize;
    const float echoL = delayLeft_[static_cast<std::size_t>(readIndex)];
    const float echoR = delayRight_[static_cast<std::size_t>(readIndex)];
    delayLeft_[static_cast<std::size_t>(delayIndex_)] = send + echoR * 0.38f;
    delayRight_[static_cast<std::size_t>(delayIndex_)] = echoL * 0.38f;
    delayIndex_ = (delayIndex_ + 1) % kDelaySize;

    // --- Mix ---
    const float drums = kick * kickGain_ + snare * snareGain_;
    const float dry = bass * bassGain_ * duck + arp * arpGain_ + lead * leadGain_ + drums;
    left = dry + padL * padGain_ * duck + hat * hatGain_ * 0.8f + echoL * 0.45f;
    right = dry + padR * padGain_ * duck + hat * hatGain_ + echoR * 0.45f;
}

void MusicEngine::Render(float* out, unsigned int frames) {
    const int requested = requestedMood_.load();
    const int requestedKey = requestedKey_.load();
    const float targetIntensity = [&] {
        switch (static_cast<MusicMood>(requested)) {
        case MusicMood::Menu: return 0.55f;
        case MusicMood::Boss: return 1.0f;
        case MusicMood::Victory: return 0.75f;
        case MusicMood::GameOver: return 0.15f;
        case MusicMood::Silent: return 0.0f;
        case MusicMood::Game: break;
        }
        return intensityTarget_.load();
    }();
    const float volume = volume_.load();

    for (unsigned int f = 0; f < frames; ++f) {
        // Mood changes dip the music out, swap, and fade back in.
        if (!switching_ && (requested != static_cast<int>(mood_) || requestedKey != key_)) {
            switching_ = true;
        }
        if (switching_) {
            moodGain_ -= 1.0f / (kSr * 0.35f);
            if (moodGain_ <= 0.0f) {
                moodGain_ = 0.0f;
                ApplyPendingMood();
                switching_ = false;
            }
        } else {
            moodGain_ = std::min(1.0f, moodGain_ + 1.0f / (kSr * 0.8f));
        }
        intensity_ += (targetIntensity - intensity_) * (1.0f / (kSr * 1.5f));

        float left = 0.0f;
        float right = 0.0f;
        RenderSample(left, right);
        const float gain = volume * moodGain_ * 0.55f;
        out[f * 2] = std::tanh(left * gain);
        out[f * 2 + 1] = std::tanh(right * gain);
    }
}

} // namespace gb
