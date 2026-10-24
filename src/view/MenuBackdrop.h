#pragma once

#include "render/CameraRig.h"
#include "render/Particles.h"
#include "render/WarpGrid.h"

#include <raylib.h>

namespace gb {

class App;

// Animated menu background: a rippling grid with drifting neon shapes and
// occasional pulses. Cheap enough to run behind every menu screen.
class MenuBackdrop {
public:
    explicit MenuBackdrop(Color accent = {80, 200, 255, 255});
    void SetAccent(Color accent) { targetAccent_ = accent; }
    void Pulse(Vector2 uiPosition, float strength);
    void Update(float dt);
    void Draw(App& app);

private:
    WarpGrid grid_;
    ParticleSystem particles_{2000};
    Color accent_;
    Color targetAccent_;
    float time_ = 0.0f;
    float pulseTimer_ = 0.0f;
};

} // namespace gb
