#pragma once

#include "audio/SoundBank.h"
#include "game/World.h"

#include <raylib.h>

#include <array>
#include <vector>

namespace gb {

class ParticleSystem;
class WarpGrid;
class CameraRig;
class AudioSystem;
class Rumble;
struct Settings;

struct FloatingText {
    Vector2 position{};
    Vector2 velocity{};
    std::array<char, 24> text{};
    Color color = WHITE;
    float life = 1.0f;
    float maxLife = 1.0f;
    float size = 22.0f;
    bool big = false;
};

// Translates simulation events into everything the player sees, hears and
// feels: particles, sound, screen shake, grid ripples, flashes, hit-stop and
// floating combat text.
class EffectsDirector {
public:
    EffectsDirector(ParticleSystem& particles, WarpGrid& grid, CameraRig& camera, AudioSystem& audio,
                    Rumble& rumble, const Settings& settings);

    void Process(const World& world, const std::vector<GameEvent>& events);
    void Update(const World& world, float realDt);

    // Simulation time scale (hit-stop and slow motion).
    float TimeScale() const;
    float Aberration() const { return aberration_; }
    Color FlashColor() const { return flashColor_; }
    float FlashAmount() const { return flashAmount_; }
    const std::vector<FloatingText>& Texts() const { return texts_; }

    void AddText(Vector2 position, const char* text, Color color, float size, bool big = false);
    // Muted directors still render everything but stay silent (attract mode).
    void SetMuted(bool muted) { muted_ = muted; }
    void Flash(Color color, float amount);

private:
    float Pan(Vector2 position) const;
    void PlaySfx(SfxId id, float volume, float pitch, float pan);
    void Shake(float trauma);
    void Vibrate(float low, float high, float duration);

    ParticleSystem& particles_;
    WarpGrid& grid_;
    CameraRig& camera_;
    AudioSystem& audio_;
    Rumble& rumble_;
    const Settings& settings_;

    // Cosmetic explosions queued to go off later (boss death chains).
    struct ScheduledBurst {
        float delay;
        Vector2 position;
        Color color;
        float size;
    };
    std::vector<ScheduledBurst> scheduled_;
    std::vector<FloatingText> texts_;
    float hitStop_ = 0.0f;
    float slowMo_ = 0.0f;
    float slowMoScale_ = 1.0f;
    float aberration_ = 0.0f;
    Color flashColor_ = WHITE;
    float flashAmount_ = 0.0f;
    float pickupPitch_ = 1.0f;
    float pickupTimer_ = 0.0f;
    float exhaustTimer_ = 0.0f;
    int bulletForceCounter_ = 0;
    bool muted_ = false;
};

} // namespace gb
