#include "audio/SoundBank.h"

#include "core/Random.h"

#include <algorithm>
#include <cmath>

namespace gb {

namespace {

enum class Osc {
    Sine,
    Square,
    Saw,
    Triangle,
    Noise
};

// One oscillator voice with pitch/amplitude/filter envelopes (sfxr-style).
struct Layer {
    Osc osc = Osc::Square;
    float freqStart = 440.0f;
    float freqEnd = 440.0f;
    float slideCurve = 1.0f; // >1: pitch moves late, <1: early
    float attack = 0.002f;
    float hold = 0.0f;
    float decay = 0.1f;
    float decayCurve = 2.0f;
    float volume = 1.0f;
    float duty = 0.5f;
    float vibratoDepth = 0.0f; // semitones
    float vibratoRate = 0.0f;
    float lowpassStart = 20000.0f;
    float lowpassEnd = 20000.0f;
    float highpass = 0.0f;
    float delay = 0.0f;
    std::vector<float> arpeggio; // semitone offsets cycled every arpStep
    float arpStep = 0.06f;
    float tremoloRate = 0.0f;
    float tremoloDepth = 0.0f;
};

struct Recipe {
    std::vector<Layer> layers;
    float drive = 0.0f;
    float echo = 0.0f;
    float echoDelay = 0.09f;
    float gain = 0.9f;
};

constexpr float kSr = static_cast<float>(kSfxSampleRate);

float LayerLength(const Layer& l) { return l.delay + l.attack + l.hold + l.decay; }

void RenderLayer(const Layer& l, std::vector<float>& out, Rng& rng) {
    const float body = l.attack + l.hold + l.decay;
    const auto start = static_cast<std::size_t>(l.delay * kSr);
    const auto count = static_cast<std::size_t>(body * kSr);
    float phase = 0.0f;
    float noiseValue = rng.Range(-1.0f, 1.0f);
    float lp = 0.0f;
    float hpOut = 0.0f;
    float hpIn = 0.0f;
    const float ratio = l.freqEnd / std::max(l.freqStart, 1.0f);

    for (std::size_t i = 0; i < count && start + i < out.size(); ++i) {
        const float t = static_cast<float>(i) / kSr;
        const float u = t / std::max(body, 1e-4f);

        float env;
        if (t < l.attack) {
            env = t / std::max(l.attack, 1e-5f);
        } else if (t < l.attack + l.hold) {
            env = 1.0f;
        } else {
            const float d = (t - l.attack - l.hold) / std::max(l.decay, 1e-5f);
            env = std::pow(std::max(0.0f, 1.0f - d), l.decayCurve);
        }
        if (l.tremoloDepth > 0.0f) {
            env *= 1.0f - l.tremoloDepth * (0.5f + 0.5f * std::sin(kTau * l.tremoloRate * t));
        }

        float freq = l.freqStart * std::pow(ratio, std::pow(u, l.slideCurve));
        if (!l.arpeggio.empty()) {
            const auto step = static_cast<std::size_t>(t / l.arpStep) % l.arpeggio.size();
            freq *= std::exp2(l.arpeggio[step] / 12.0f);
        }
        if (l.vibratoDepth > 0.0f) {
            freq *= std::exp2(l.vibratoDepth * std::sin(kTau * l.vibratoRate * t) / 12.0f);
        }

        phase += freq / kSr;
        const bool wrapped = phase >= 1.0f;
        phase -= std::floor(phase);

        float v = 0.0f;
        switch (l.osc) {
        case Osc::Sine: v = std::sin(kTau * phase); break;
        case Osc::Square: v = phase < l.duty ? 1.0f : -1.0f; break;
        case Osc::Saw: v = 2.0f * phase - 1.0f; break;
        case Osc::Triangle: v = 4.0f * std::abs(phase - 0.5f) - 1.0f; break;
        case Osc::Noise:
            if (wrapped) {
                noiseValue = rng.Range(-1.0f, 1.0f);
            }
            v = noiseValue;
            break;
        }

        const float cutoff = l.lowpassStart * std::pow(l.lowpassEnd / l.lowpassStart, u);
        const float alpha = 1.0f - std::exp(-kTau * std::min(cutoff, kSr * 0.45f) / kSr);
        lp += (v - lp) * alpha;
        v = lp;
        if (l.highpass > 0.0f) {
            const float rc = 1.0f / (kTau * l.highpass);
            const float a = rc / (rc + 1.0f / kSr);
            hpOut = a * (hpOut + v - hpIn);
            hpIn = v;
            v = hpOut;
        }
        out[start + i] += v * env * l.volume;
    }
}

std::vector<float> Render(const Recipe& recipe, std::uint64_t seed) {
    float length = 0.0f;
    for (const Layer& l : recipe.layers) {
        length = std::max(length, LayerLength(l));
    }
    if (recipe.echo > 0.0f) {
        length += recipe.echoDelay * 6.0f;
    }
    std::vector<float> out(static_cast<std::size_t>(length * kSr) + 1, 0.0f);
    Rng rng(seed);
    for (const Layer& l : recipe.layers) {
        RenderLayer(l, out, rng);
    }
    if (recipe.drive > 0.0f) {
        const float k = 1.0f + recipe.drive;
        const float norm = std::tanh(k);
        for (float& s : out) {
            s = std::tanh(s * k) / norm;
        }
    }
    if (recipe.echo > 0.0f) {
        const auto d = static_cast<std::size_t>(recipe.echoDelay * kSr);
        for (std::size_t i = d; i < out.size(); ++i) {
            out[i] += out[i - d] * recipe.echo;
        }
    }
    float peak = 0.0f;
    for (float s : out) {
        peak = std::max(peak, std::abs(s));
    }
    if (peak > 1e-5f) {
        const float scale = recipe.gain / peak;
        for (float& s : out) {
            s *= scale;
        }
    }
    // Short fade at both ends avoids clicks.
    const std::size_t fade = std::min<std::size_t>(64, out.size() / 2);
    for (std::size_t i = 0; i < fade; ++i) {
        const float f = static_cast<float>(i) / static_cast<float>(fade);
        out[i] *= f;
        out[out.size() - 1 - i] *= f;
    }
    return out;
}

Layer Noise(float lowStart, float lowEnd, float decay, float volume = 1.0f) {
    Layer l;
    l.osc = Osc::Noise;
    l.freqStart = l.freqEnd = kSr;
    l.lowpassStart = lowStart;
    l.lowpassEnd = lowEnd;
    l.decay = decay;
    l.volume = volume;
    return l;
}

Layer Tone(Osc osc, float f0, float f1, float decay, float volume = 1.0f) {
    Layer l;
    l.osc = osc;
    l.freqStart = f0;
    l.freqEnd = f1;
    l.decay = decay;
    l.volume = volume;
    return l;
}

Recipe MakeRecipe(SfxId id) {
    Recipe r;
    switch (id) {
    case SfxId::Shoot: {
        Layer a = Tone(Osc::Square, 1250.0f, 380.0f, 0.075f, 0.6f);
        a.duty = 0.3f;
        a.slideCurve = 0.5f;
        a.lowpassStart = a.lowpassEnd = 5200.0f;
        Layer b = Noise(9000.0f, 2000.0f, 0.03f, 0.35f);
        b.highpass = 1500.0f;
        r.layers = {a, b};
        r.gain = 0.5f;
        break;
    }
    case SfxId::ShootHeavy: {
        Layer a = Tone(Osc::Saw, 520.0f, 70.0f, 0.28f, 0.8f);
        a.slideCurve = 0.4f;
        a.lowpassStart = 6000.0f;
        a.lowpassEnd = 600.0f;
        Layer b = Noise(12000.0f, 800.0f, 0.18f, 0.6f);
        Layer c = Tone(Osc::Sine, 160.0f, 50.0f, 0.2f, 0.7f);
        r.layers = {a, b, c};
        r.drive = 1.5f;
        r.gain = 0.65f;
        break;
    }
    case SfxId::ShootScatter: {
        Layer a = Noise(7000.0f, 600.0f, 0.16f, 1.0f);
        Layer b = Tone(Osc::Square, 300.0f, 90.0f, 0.12f, 0.5f);
        b.lowpassStart = b.lowpassEnd = 2500.0f;
        r.layers = {a, b};
        r.drive = 1.0f;
        r.gain = 0.6f;
        break;
    }
    case SfxId::EnemyShoot: {
        Layer a = Tone(Osc::Triangle, 820.0f, 360.0f, 0.12f, 1.0f);
        a.slideCurve = 0.6f;
        r.layers = {a};
        r.gain = 0.45f;
        break;
    }
    case SfxId::Hit: {
        Layer a = Noise(8000.0f, 3000.0f, 0.035f, 0.8f);
        a.highpass = 2000.0f;
        Layer b = Tone(Osc::Sine, 620.0f, 380.0f, 0.04f, 0.6f);
        r.layers = {a, b};
        r.gain = 0.4f;
        break;
    }
    case SfxId::HitCrit: {
        Layer a = Noise(12000.0f, 4000.0f, 0.05f, 0.7f);
        a.highpass = 2500.0f;
        Layer b = Tone(Osc::Square, 1500.0f, 900.0f, 0.07f, 0.5f);
        b.duty = 0.25f;
        r.layers = {a, b};
        r.gain = 0.5f;
        break;
    }
    case SfxId::ShieldPing: {
        Layer a = Tone(Osc::Triangle, 2400.0f, 2100.0f, 0.06f, 1.0f);
        Layer b = Tone(Osc::Sine, 3300.0f, 3000.0f, 0.05f, 0.5f);
        r.layers = {a, b};
        r.gain = 0.3f;
        break;
    }
    case SfxId::EnemyDie: {
        Layer a = Noise(6000.0f, 300.0f, 0.32f, 1.0f);
        a.decayCurve = 2.5f;
        Layer b = Tone(Osc::Sine, 220.0f, 45.0f, 0.22f, 0.9f);
        b.slideCurve = 0.5f;
        Layer c = Tone(Osc::Square, 700.0f, 120.0f, 0.1f, 0.25f);
        c.lowpassStart = c.lowpassEnd = 3000.0f;
        r.layers = {a, b, c};
        r.drive = 0.8f;
        r.gain = 0.7f;
        break;
    }
    case SfxId::EnemyDieBig:
    case SfxId::Explosion: {
        Layer a = Noise(5000.0f, 150.0f, id == SfxId::Explosion ? 0.6f : 0.75f, 1.0f);
        a.decayCurve = 2.0f;
        Layer b = Tone(Osc::Sine, 150.0f, 32.0f, 0.55f, 1.0f);
        b.slideCurve = 0.4f;
        Layer c = Noise(1200.0f, 100.0f, 0.9f, 0.5f);
        c.freqStart = c.freqEnd = 3000.0f; // grainy rumble
        r.layers = {a, b, c};
        r.drive = 2.0f;
        r.gain = 0.85f;
        break;
    }
    case SfxId::PlayerHurt: {
        Layer a = Tone(Osc::Saw, 480.0f, 70.0f, 0.42f, 0.9f);
        a.vibratoDepth = 1.5f;
        a.vibratoRate = 30.0f;
        a.lowpassStart = 4000.0f;
        a.lowpassEnd = 500.0f;
        Layer b = Noise(9000.0f, 400.0f, 0.3f, 0.8f);
        Layer c = Tone(Osc::Sine, 110.0f, 40.0f, 0.35f, 0.9f);
        r.layers = {a, b, c};
        r.drive = 2.5f;
        r.gain = 0.85f;
        break;
    }
    case SfxId::PlayerDeath: {
        Layer a = Noise(8000.0f, 80.0f, 1.8f, 1.0f);
        a.decayCurve = 1.5f;
        Layer b = Tone(Osc::Saw, 300.0f, 25.0f, 1.6f, 0.8f);
        b.lowpassStart = 3000.0f;
        b.lowpassEnd = 200.0f;
        Layer c = Tone(Osc::Sine, 90.0f, 20.0f, 1.4f, 1.0f);
        r.layers = {a, b, c};
        r.drive = 2.0f;
        r.echo = 0.35f;
        r.echoDelay = 0.16f;
        r.gain = 0.9f;
        break;
    }
    case SfxId::ShieldBlock: {
        Layer a = Tone(Osc::Square, 1900.0f, 1700.0f, 0.35f, 0.6f);
        a.duty = 0.15f;
        a.tremoloRate = 40.0f;
        a.tremoloDepth = 0.6f;
        Layer b = Tone(Osc::Sine, 2850.0f, 2600.0f, 0.4f, 0.6f);
        Layer c = Noise(9000.0f, 3000.0f, 0.08f, 0.5f);
        r.layers = {a, b, c};
        r.echo = 0.3f;
        r.gain = 0.6f;
        break;
    }
    case SfxId::Dash: {
        Layer a = Noise(700.0f, 5000.0f, 0.2f, 1.0f);
        a.attack = 0.03f;
        a.decayCurve = 1.5f;
        a.highpass = 300.0f;
        r.layers = {a};
        r.gain = 0.5f;
        break;
    }
    case SfxId::Nova: {
        Layer a = Tone(Osc::Sine, 110.0f, 28.0f, 1.2f, 1.0f);
        a.slideCurve = 0.5f;
        Layer b = Noise(4000.0f, 120.0f, 1.0f, 0.8f);
        Layer c = Tone(Osc::Saw, 900.0f, 60.0f, 0.6f, 0.4f);
        c.lowpassStart = 5000.0f;
        c.lowpassEnd = 300.0f;
        r.layers = {a, b, c};
        r.drive = 1.8f;
        r.echo = 0.3f;
        r.echoDelay = 0.12f;
        r.gain = 0.95f;
        break;
    }
    case SfxId::LevelUp: {
        Layer a = Tone(Osc::Square, 523.25f, 523.25f, 0.5f, 0.6f);
        a.duty = 0.25f;
        a.arpeggio = {0.0f, 4.0f, 7.0f, 12.0f, 16.0f, 19.0f, 24.0f};
        a.arpStep = 0.055f;
        a.hold = 0.2f;
        a.lowpassStart = a.lowpassEnd = 7000.0f;
        Layer b = Tone(Osc::Triangle, 1046.5f, 1046.5f, 0.6f, 0.4f);
        b.arpeggio = {0.0f, 7.0f, 12.0f};
        b.arpStep = 0.09f;
        b.delay = 0.1f;
        r.layers = {a, b};
        r.echo = 0.35f;
        r.echoDelay = 0.11f;
        r.gain = 0.55f;
        break;
    }
    case SfxId::PickupXp: {
        Layer a = Tone(Osc::Triangle, 1300.0f, 1900.0f, 0.06f, 1.0f);
        Layer b = Tone(Osc::Sine, 2600.0f, 3400.0f, 0.04f, 0.4f);
        r.layers = {a, b};
        r.gain = 0.32f;
        break;
    }
    case SfxId::PickupHealth: {
        Layer a = Tone(Osc::Triangle, 660.0f, 660.0f, 0.35f, 1.0f);
        a.arpeggio = {0.0f, 5.0f, 12.0f};
        a.arpStep = 0.08f;
        a.hold = 0.1f;
        r.layers = {a};
        r.echo = 0.25f;
        r.gain = 0.55f;
        break;
    }
    case SfxId::PickupCache: {
        Layer a = Tone(Osc::Square, 880.0f, 880.0f, 0.7f, 0.5f);
        a.duty = 0.2f;
        a.arpeggio = {0.0f, 3.0f, 7.0f, 10.0f, 12.0f, 15.0f, 19.0f, 24.0f};
        a.arpStep = 0.045f;
        a.hold = 0.2f;
        a.vibratoDepth = 0.2f;
        a.vibratoRate = 8.0f;
        Layer b = Tone(Osc::Sine, 1760.0f, 1760.0f, 0.8f, 0.4f);
        b.delay = 0.15f;
        b.tremoloRate = 18.0f;
        b.tremoloDepth = 0.7f;
        r.layers = {a, b};
        r.echo = 0.35f;
        r.gain = 0.55f;
        break;
    }
    case SfxId::Spawn: {
        Layer a = Tone(Osc::Sine, 260.0f, 900.0f, 0.18f, 1.0f);
        a.attack = 0.08f;
        a.slideCurve = 1.5f;
        r.layers = {a};
        r.gain = 0.22f;
        break;
    }
    case SfxId::LaserCharge: {
        Layer a = Tone(Osc::Saw, 180.0f, 1300.0f, 0.15f, 0.7f);
        a.attack = 0.85f;
        a.slideCurve = 1.3f;
        a.tremoloRate = 22.0f;
        a.tremoloDepth = 0.5f;
        a.lowpassStart = 800.0f;
        a.lowpassEnd = 5000.0f;
        r.layers = {a};
        r.gain = 0.35f;
        break;
    }
    case SfxId::LaserFire: {
        Layer a = Tone(Osc::Saw, 1600.0f, 180.0f, 0.45f, 0.8f);
        a.slideCurve = 0.6f;
        a.vibratoDepth = 0.6f;
        a.vibratoRate = 45.0f;
        Layer b = Noise(10000.0f, 900.0f, 0.4f, 0.7f);
        Layer c = Tone(Osc::Sine, 120.0f, 60.0f, 0.4f, 0.8f);
        r.layers = {a, b, c};
        r.drive = 1.5f;
        r.gain = 0.7f;
        break;
    }
    case SfxId::Zap: {
        Layer a = Noise(14000.0f, 6000.0f, 0.22f, 1.0f);
        a.freqStart = a.freqEnd = 9000.0f;
        a.highpass = 1500.0f;
        a.tremoloRate = 60.0f;
        a.tremoloDepth = 0.9f;
        Layer b = Tone(Osc::Square, 2200.0f, 600.0f, 0.15f, 0.4f);
        b.duty = 0.1f;
        r.layers = {a, b};
        r.drive = 1.0f;
        r.gain = 0.45f;
        break;
    }
    case SfxId::Lance: {
        Layer a = Tone(Osc::Saw, 2400.0f, 400.0f, 0.35f, 0.7f);
        a.slideCurve = 0.4f;
        a.lowpassStart = 12000.0f;
        a.lowpassEnd = 1500.0f;
        Layer b = Tone(Osc::Sine, 600.0f, 150.0f, 0.3f, 0.6f);
        Layer c = Noise(12000.0f, 2000.0f, 0.15f, 0.5f);
        r.layers = {a, b, c};
        r.echo = 0.25f;
        r.gain = 0.55f;
        break;
    }
    case SfxId::Missile: {
        Layer a = Noise(1500.0f, 6000.0f, 0.28f, 1.0f);
        a.attack = 0.02f;
        a.highpass = 400.0f;
        Layer b = Tone(Osc::Triangle, 300.0f, 700.0f, 0.2f, 0.3f);
        r.layers = {a, b};
        r.gain = 0.35f;
        break;
    }
    case SfxId::Pulse: {
        Layer a = Tone(Osc::Sine, 170.0f, 55.0f, 0.32f, 1.0f);
        a.slideCurve = 0.5f;
        Layer b = Noise(3000.0f, 300.0f, 0.25f, 0.5f);
        r.layers = {a, b};
        r.drive = 1.2f;
        r.gain = 0.55f;
        break;
    }
    case SfxId::MineArm: {
        Layer a = Tone(Osc::Square, 1800.0f, 1800.0f, 0.05f, 0.7f);
        a.duty = 0.2f;
        Layer b = a;
        b.delay = 0.09f;
        r.layers = {a, b};
        r.gain = 0.35f;
        break;
    }
    case SfxId::BossWarning: {
        Layer a = Tone(Osc::Square, 620.0f, 620.0f, 0.15f, 0.7f);
        a.hold = 1.5f;
        a.arpeggio = {0.0f, -5.0f};
        a.arpStep = 0.4f;
        a.duty = 0.35f;
        a.lowpassStart = a.lowpassEnd = 2400.0f;
        a.vibratoDepth = 0.3f;
        a.vibratoRate = 6.0f;
        Layer b = Tone(Osc::Saw, 155.0f, 155.0f, 0.3f, 0.5f);
        b.hold = 1.4f;
        b.arpeggio = {0.0f, -5.0f};
        b.arpStep = 0.4f;
        b.lowpassStart = b.lowpassEnd = 900.0f;
        r.layers = {a, b};
        r.echo = 0.3f;
        r.echoDelay = 0.2f;
        r.gain = 0.6f;
        break;
    }
    case SfxId::BossRoar: {
        Layer a = Tone(Osc::Saw, 95.0f, 42.0f, 1.0f, 1.0f);
        a.attack = 0.08f;
        a.vibratoDepth = 1.2f;
        a.vibratoRate = 11.0f;
        a.lowpassStart = 1800.0f;
        a.lowpassEnd = 300.0f;
        Layer b = Noise(2500.0f, 200.0f, 1.0f, 0.6f);
        b.attack = 0.05f;
        Layer c = Tone(Osc::Square, 190.0f, 84.0f, 0.9f, 0.4f);
        c.lowpassStart = c.lowpassEnd = 1200.0f;
        r.layers = {a, b, c};
        r.drive = 3.0f;
        r.gain = 0.8f;
        break;
    }
    case SfxId::BossDeath: {
        Layer a = Noise(7000.0f, 60.0f, 2.6f, 1.0f);
        a.decayCurve = 1.3f;
        Layer b = Tone(Osc::Sine, 130.0f, 18.0f, 2.4f, 1.0f);
        Layer c = Tone(Osc::Saw, 400.0f, 30.0f, 2.0f, 0.5f);
        c.lowpassStart = 3000.0f;
        c.lowpassEnd = 100.0f;
        r.layers = {a, b, c};
        r.drive = 2.5f;
        r.echo = 0.4f;
        r.echoDelay = 0.21f;
        r.gain = 0.95f;
        break;
    }
    case SfxId::SectorClear: {
        Layer a = Tone(Osc::Square, 392.0f, 392.0f, 0.9f, 0.5f);
        a.duty = 0.25f;
        a.arpeggio = {0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 12.0f, 16.0f, 19.0f};
        a.arpStep = 0.09f;
        a.hold = 0.4f;
        Layer b = Tone(Osc::Saw, 196.0f, 196.0f, 1.2f, 0.35f);
        b.attack = 0.1f;
        b.hold = 0.3f;
        b.lowpassStart = b.lowpassEnd = 1500.0f;
        r.layers = {a, b};
        r.echo = 0.35f;
        r.echoDelay = 0.135f;
        r.gain = 0.6f;
        break;
    }
    case SfxId::Combo: {
        Layer a = Tone(Osc::Triangle, 1568.0f, 1568.0f, 0.3f, 1.0f);
        a.arpeggio = {0.0f, 7.0f, 12.0f};
        a.arpStep = 0.05f;
        r.layers = {a};
        r.echo = 0.3f;
        r.gain = 0.4f;
        break;
    }
    case SfxId::UiMove: {
        Layer a = Tone(Osc::Sine, 1500.0f, 1300.0f, 0.03f, 1.0f);
        r.layers = {a};
        r.gain = 0.25f;
        break;
    }
    case SfxId::UiConfirm: {
        Layer a = Tone(Osc::Square, 880.0f, 880.0f, 0.12f, 0.6f);
        a.duty = 0.25f;
        a.arpeggio = {0.0f, 7.0f};
        a.arpStep = 0.05f;
        a.lowpassStart = a.lowpassEnd = 6000.0f;
        r.layers = {a};
        r.gain = 0.35f;
        break;
    }
    case SfxId::UiBack: {
        Layer a = Tone(Osc::Square, 660.0f, 660.0f, 0.12f, 0.6f);
        a.duty = 0.25f;
        a.arpeggio = {0.0f, -5.0f};
        a.arpStep = 0.05f;
        a.lowpassStart = a.lowpassEnd = 5000.0f;
        r.layers = {a};
        r.gain = 0.3f;
        break;
    }
    case SfxId::UiDeny: {
        Layer a = Tone(Osc::Square, 140.0f, 120.0f, 0.16f, 1.0f);
        a.lowpassStart = a.lowpassEnd = 1500.0f;
        r.layers = {a};
        r.gain = 0.35f;
        break;
    }
    case SfxId::CardFlip: {
        Layer a = Noise(3000.0f, 9000.0f, 0.09f, 1.0f);
        a.attack = 0.02f;
        a.highpass = 800.0f;
        r.layers = {a};
        r.gain = 0.25f;
        break;
    }
    case SfxId::Reroll: {
        Layer a = Tone(Osc::Square, 600.0f, 1200.0f, 0.25f, 0.6f);
        a.arpeggio = {0.0f, 3.0f, 0.0f, 5.0f, 0.0f, 7.0f};
        a.arpStep = 0.035f;
        a.duty = 0.2f;
        r.layers = {a};
        r.gain = 0.35f;
        break;
    }
    case SfxId::Count: break;
    }
    return r;
}

} // namespace

std::vector<float> SynthesizeSfx(SfxId id) {
    return Render(MakeRecipe(id), 0x9e3779b97f4a7c15ULL ^ static_cast<std::uint64_t>(id));
}

float SfxMinInterval(SfxId id) {
    switch (id) {
    case SfxId::Hit:
    case SfxId::HitCrit:
    case SfxId::ShieldPing: return 0.03f;
    case SfxId::EnemyDie: return 0.035f;
    case SfxId::PickupXp: return 0.025f;
    case SfxId::EnemyShoot: return 0.06f;
    case SfxId::Spawn: return 0.08f;
    case SfxId::Explosion:
    case SfxId::EnemyDieBig: return 0.07f;
    case SfxId::Shoot: return 0.03f;
    default: return 0.0f;
    }
}

int SfxPolyphony(SfxId id) {
    switch (id) {
    case SfxId::Shoot:
    case SfxId::Hit:
    case SfxId::EnemyDie:
    case SfxId::PickupXp: return 8;
    case SfxId::HitCrit:
    case SfxId::Explosion:
    case SfxId::EnemyDieBig:
    case SfxId::EnemyShoot:
    case SfxId::ShieldPing:
    case SfxId::Spawn: return 5;
    default: return 3;
    }
}

} // namespace gb
