#pragma once

#include "game/World.h"
#include "render/CameraRig.h"
#include "render/Particles.h"
#include "render/WarpGrid.h"
#include "view/Effects.h"
#include "view/WorldRenderer.h"

#include <memory>

namespace gb {

class App;

// A running world plus everything needed to present it: grid, particles,
// camera, effects and renderer. Shared by the game scene and the title
// screen's attract-mode demo.
class GameView {
public:
    GameView(App& app, const RunConfig& config);

    // Advances simulation (scaled by hit-stop/slow-mo) and presentation.
    void Update(float dt, const PlayerInput& input, bool simulate);
    void DrawWorld(Vector2 reticle, bool showReticle);

    World& GetWorld() { return *world_; }
    const World& GetWorld() const { return *world_; }
    EffectsDirector& Effects() { return effects_; }
    const EffectsDirector& Effects() const { return effects_; }
    CameraRig& Camera() { return camera_; }
    Camera2D CurrentCamera() const;
    Vector2 ScreenToWorld(Vector2 screen) const;
    Vector2 WorldToUi(Vector2 world) const;
    Color AccentColor() const { return accent_; }
    std::size_t ParticleCount() const { return particles_.Count(); }
    // CPU cost of the last update/draw, for the performance overlay.
    float UpdateMs() const { return updateMs_; }
    float DrawMs() const { return drawMs_; }
    float Time() const { return time_; }

private:
    App& app_;
    std::unique_ptr<World> world_;
    WarpGrid grid_;
    ParticleSystem particles_;
    CameraRig camera_;
    EffectsDirector effects_;
    WorldRenderer renderer_;
    Color gridColor_{};
    Color accent_{};
    float time_ = 0.0f;
    float updateMs_ = 0.0f;
    float drawMs_ = 0.0f;
};

} // namespace gb
