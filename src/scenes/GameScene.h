#pragma once

#include "core/Scene.h"
#include "game/Bot.h"
#include "game/World.h"
#include "ui/SettingsPanel.h"
#include "ui/UpgradeDraft.h"
#include "ui/Widgets.h"
#include "view/GameView.h"
#include "view/Hud.h"

#include <array>
#include <cstdint>

namespace gb {

// A single run: gameplay, HUD and the in-run overlays (upgrade drafts, pause,
// sector modifiers, victory). Hands off to the results screen when the run ends.
class GameScene final : public Scene {
public:
    GameScene(App& app, const RunConfig& config);
    ~GameScene() override;

    void OnEnter() override;
    void Update(float dt) override;
    void DrawWorld() override;
    void DrawUi() override;
    void ConfigurePostFx(PostFxParams& params) const override;

private:
    enum class Overlay { None, Draft, Pause, Settings, SectorChoice, Victory };

    void ApplyDebugLaunchOptions();
    PlayerInput ReadPlayerInput();
    void UpdateOverlays(float dt);
    void UpdateAutopilotOverlays();
    void OpenPause();
    void OpenSectorChoice();
    void UpdateMusic(float dt);
    void FinishRun(bool victory);
    void CheckChallenges();
    Vector2 MouseWorld() const;

    void DrawPause();
    void DrawSectorChoice();
    void DrawVictory();
    void DrawTutorial();
    void DrawDebugOverlay();

    RunConfig config_;
    GameView view_;
    Hud hud_;
    UpgradeDraft draft_;
    SettingsPanel settings_;
    ui::Menu pauseMenu_;
    ui::Menu victoryMenu_;
    Overlay overlay_ = Overlay::None;
    Bot bot_;
    bool autopilot_ = false;

    std::array<SectorModifier, 2> modifierChoices_{};
    int modifierSelected_ = 0;
    float overlayTime_ = 0.0f;
    float draftDelay_ = 0.0f;
    float deathTimer_ = 0.0f;
    float musicIntensity_ = 0.3f;
    float tutorialTime_ = 0.0f;
    float challengeTimer_ = 0.0f;
    std::uint32_t announcedEnemies_ = 0;  // EnemyKind bits already toasted this run
    bool showTutorial_ = false;
    bool finished_ = false;
    bool victoryShown_ = false;
    bool wasFocused_ = false;
    bool debugNovaPending_ = false;
    Vector2 lastAim_{1.0f, 0.0f};
};

} // namespace gb
