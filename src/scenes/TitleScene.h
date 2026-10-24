#pragma once

#include "core/Scene.h"
#include "game/Bot.h"
#include "ui/CodexPanel.h"
#include "ui/SettingsPanel.h"
#include "ui/Widgets.h"
#include "view/GameView.h"

#include <memory>

namespace gb {

// Title screen with an attract-mode demo (the bot playing) in the background.
class TitleScene final : public Scene {
public:
    explicit TitleScene(App& app);

    void OnEnter() override;
    void Update(float dt) override;
    void DrawWorld() override;
    void DrawUi() override;
    void ConfigurePostFx(PostFxParams& params) const override;

private:
    void ResetDemo();
    void StartDaily();
    void DrawRecords();
    void DrawChallenges(Rectangle area);
    void DrawRecentRuns(Rectangle area);
    void DrawLogo(Vector2 center, float scale);

    std::unique_ptr<GameView> demo_;
    Bot bot_;
    ui::Menu menu_;
    SettingsPanel settings_;
    CodexPanel codex_;
    bool showRecords_ = false;
    bool showRecentRuns_ = false; // records: recent runs instead of challenges
    float time_ = 0.0f;
    float demoDeathTimer_ = 0.0f;
    int demoCounter_ = 0;
};

} // namespace gb
