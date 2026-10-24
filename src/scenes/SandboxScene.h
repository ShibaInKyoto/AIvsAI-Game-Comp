#pragma once

#include "core/Scene.h"
#include "render/CameraRig.h"
#include "render/Particles.h"
#include "render/WarpGrid.h"

namespace gb {

// Temporary rendering test bed.
class SandboxScene final : public Scene {
public:
    explicit SandboxScene(App& app);
    void Update(float dt) override;
    void DrawWorld() override;
    void DrawUi() override;

private:
    WarpGrid grid_;
    ParticleSystem particles_;
    CameraRig camera_;
    float time_ = 0.0f;
    float burstTimer_ = 0.0f;
};

} // namespace gb
