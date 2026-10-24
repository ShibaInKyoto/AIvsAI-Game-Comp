#pragma once

#include "core/Scene.h"
#include "game/Profile.h"
#include "view/MenuBackdrop.h"

namespace gb {

// Played after a victorious run (the Core destroyed): a short typed epilogue,
// then a credits roll with the run's stats. Skippable; hands off to results.
class EndingScene final : public Scene {
public:
    EndingScene(App& app, const RunSummary& summary, int creditsEarned);

    void OnEnter() override;
    void Update(float dt) override;
    void DrawWorld() override;
    void DrawUi() override;
    void ConfigurePostFx(PostFxParams& params) const override;

private:
    void Finish();
    void DrawEpilogue();
    void DrawCredits();

    RunSummary summary_;
    int creditsEarned_ = 0;
    MenuBackdrop backdrop_;
    float time_ = 0.0f;
    bool leaving_ = false;
};

} // namespace gb
