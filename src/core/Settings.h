#pragma once

namespace gb {

// User preferences, persisted to settings.ini in the data directory.
struct Settings {
    float masterVolume = 0.8f;
    float musicVolume = 0.65f;
    float sfxVolume = 0.8f;

    bool fullscreen = true;
    bool vsync = true;
    int renderScale = 0;  // percent of native resolution for the 3D-ish scene; 0 = automatic
    bool showFps = false;

    float screenShake = 1.0f;
    bool bloom = true;
    float bloomIntensity = 1.0f;
    bool chromaticAberration = true;
    bool filmGrain = true;
    bool scanlines = false;
    bool reduceFlashing = false;

    bool damageNumbers = true;
    bool autoFire = false;
    bool aimAssist = true;

    bool Load();
    bool Save() const;
    void Sanitize();
};

} // namespace gb
