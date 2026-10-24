#pragma once

#include "core/Scene.h"
#include "game/Profile.h"
#include "ui/Widgets.h"
#include "view/MenuBackdrop.h"

namespace gb {

// End-of-run summary: stats tally, score, records and credits earned.
class ResultsScene final : public Scene {
public:
    ResultsScene(App& app, const RunSummary& summary, int creditsEarned);

    void OnEnter() override;
    void Update(float dt) override;
    void DrawWorld() override;
    void DrawUi() override;

private:
    void DrawLoadout(Rectangle panel, Color accent, float alpha);

    RunSummary summary_;
    int creditsEarned_ = 0;
    bool newRecord_ = false;
    bool unlockAvailable_ = false;
    MenuBackdrop backdrop_;
    ui::Menu menu_;
    float time_ = 0.0f;
    float displayedScore_ = 0.0f;
    float displayedCredits_ = 0.0f;
    int revealedRows_ = 0;
    float rowTimer_ = 0.0f;
};

} // namespace gb
