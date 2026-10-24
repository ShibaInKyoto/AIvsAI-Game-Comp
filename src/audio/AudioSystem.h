#pragma once

namespace gb {

struct Settings;

// Placeholder; replaced by the full synthesiser-backed implementation.
class AudioSystem {
public:
    bool Init(bool enabled);
    void Shutdown();
    void Update(float dt);
    void ApplyVolumes(const Settings& settings);
};

} // namespace gb
