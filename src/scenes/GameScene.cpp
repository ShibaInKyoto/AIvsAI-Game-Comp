#include "scenes/GameScene.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "game/Profile.h"
#include "scenes/ResultsScene.h"
#include "scenes/TitleScene.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>

namespace gb {

namespace {

constexpr float kDraftDelay = 0.45f;   // let the level-up burst play before pausing
constexpr float kDeathDelay = 2.6f;
constexpr float kTutorialDuration = 16.0f;

enum PauseItem { PauseResume, PauseSettings, PauseRestart, PauseQuit };
enum VictoryItem { VictoryEndless, VictoryFinish };

} // namespace

GameScene::GameScene(App& app, const RunConfig& config)
    : Scene(app), config_(config), view_(app, config), autopilot_(app.Options().autotestSeconds > 0.0f) {
    pauseMenu_.SetItems({{"RESUME"}, {"SETTINGS"}, {"RESTART RUN"}, {"ABANDON RUN"}});
    victoryMenu_.SetItems({{"CONTINUE IN ENDLESS MODE"}, {"FINISH RUN"}});
    showTutorial_ = !app.Progress().TutorialSeen() && !autopilot_;
}

GameScene::~GameScene() { ShowCursor(); }

void GameScene::OnEnter() {
    if (!autopilot_) {
        HideCursor();
    }
    app_.Audio().Music().SetMood(MusicMood::Game, 0);

    ApplyDebugLaunchOptions();
}

// Developer entry points ("--sector", "--boss", "--scene draft|pause") used
// for testing encounters and capturing UI screenshots.
void GameScene::ApplyDebugLaunchOptions() {
    const LaunchOptions& options = app_.Options();
    if (options.startSector > 0 || options.startAtBoss) {
        World& world = view_.GetWorld();
        world.GetDirector().DebugJump(options.startSector, options.startAtBoss);
        // Give the debug run a build roughly matching that point of a real run.
        const int levels = 6 + options.startSector * 7;
        for (int i = 0; i < levels; ++i) {
            const auto offers = world.RollOffers(3);
            if (!offers.empty()) {
                world.ApplyUpgrade(Bot::ChooseUpgrade(world, offers));
            }
        }
    }

    // Debug entry points used for UI screenshots ("--scene draft|pause").
    const std::string& debugScene = options.startScene;
    if (debugScene == "draft" || debugScene == "evolution") {
        World& world = view_.GetWorld();
        const int blades = debugScene == "evolution" ? 5 : 1;
        for (int i = 0; i < blades; ++i) {
            world.ApplyUpgrade(UpgradeId::OrbitBlades);
        }
        world.ApplyUpgrade(debugScene == "evolution" ? UpgradeId::Thrusters : UpgradeId::Overclock);
        world.ApplyUpgrade(UpgradeId::Drones);
        draft_.Open(app_, world, false);
        overlay_ = Overlay::Draft;
    } else if (debugScene == "pause") {
        World& world = view_.GetWorld();
        for (UpgradeId id : {UpgradeId::SplitShot, UpgradeId::ArcCoil, UpgradeId::ArcCoil, UpgradeId::Magnet,
                             UpgradeId::HighVoltage, UpgradeId::Deflector}) {
            world.ApplyUpgrade(id);
        }
        OpenPause();
    }
}

Vector2 GameScene::MouseWorld() const { return view_.ScreenToWorld(app_.In().MousePosition()); }

PlayerInput GameScene::ReadPlayerInput() {
    World& world = view_.GetWorld();
    if (autopilot_) {
        return bot_.Think(world, GetFrameTime());
    }
    const Input& in = app_.In();
    const Settings& prefs = app_.Prefs();
    const Player& player = world.GetPlayer();

    PlayerInput input;
    input.move = in.Move();
    input.dash = in.Pressed(Action::Dash);
    input.nova = in.Pressed(Action::Special);

    if (in.LastDevice() == InputDevice::Gamepad) {
        if (in.AimStickActive()) {
            lastAim_ = Normalized(in.AimStick());
            input.fire = true;
        } else if (in.Down(Action::Fire) || prefs.autoFire) {
            input.fire = true;
            if (prefs.aimAssist) {
                if (const Enemy* target = world.NearestEnemy(player.position, 750.0f)) {
                    lastAim_ = Normalized(target->position - player.position);
                }
            }
        }
        input.aim = lastAim_;
    } else {
        const Vector2 toMouse = MouseWorld() - player.position;
        if (LengthSq(toMouse) > 25.0f) {
            lastAim_ = Normalized(toMouse);
        }
        input.aim = lastAim_;
        input.fire = in.Down(Action::Fire) || prefs.autoFire;
    }
    return input;
}

void GameScene::Update(float dt) {
    World& world = view_.GetWorld();
    hud_.Update(view_, dt);

    if (overlay_ != Overlay::None) {
        UpdateOverlays(dt);
        // Keep the backdrop alive (particles settle, camera eases) while paused.
        view_.Update(dt, PlayerInput{}, false);
        UpdateMusic(dt);
        return;
    }

    // Pause on request, and automatically when the window loses focus
    // (only on the transition, so a window that never had focus still plays).
    const bool focused = IsWindowFocused();
    const bool focusLost = wasFocused_ && !focused && !autopilot_;
    wasFocused_ = focused;
    if (!autopilot_ && (app_.In().Pressed(Action::Pause) || focusLost) && world.PlayerAlive()) {
        OpenPause();
        return;
    }

    view_.Update(dt, ReadPlayerInput(), true);
    UpdateMusic(dt);
    challengeTimer_ -= dt;
    if (challengeTimer_ <= 0.0f) {
        challengeTimer_ = 0.5f;
        CheckChallenges();
    }
    if (showTutorial_) {
        tutorialTime_ += dt;
        if (tutorialTime_ > kTutorialDuration) {
            showTutorial_ = false;
        }
    }

    if (!world.PlayerAlive()) {
        deathTimer_ += dt;
        if (deathTimer_ >= kDeathDelay) {
            FinishRun(false);
        }
        return;
    }

    // Pending upgrade picks pause the action after a short beat.
    if (world.PendingLevelUps() > 0 || world.PendingCaches() > 0) {
        draftDelay_ += dt;
        if (draftDelay_ >= kDraftDelay) {
            draftDelay_ = 0.0f;
            if (autopilot_) {
                UpdateAutopilotOverlays();
            } else {
                draft_.Open(app_, world, world.PendingLevelUps() == 0);
                overlay_ = Overlay::Draft;
                ShowCursor();
            }
            return;
        }
    } else {
        draftDelay_ = 0.0f;
    }

    const Director& director = world.GetDirector();
    if (director.Phase() == SectorPhase::Cleared && director.PhaseTime() > 2.6f &&
        world.PendingLevelUps() == 0 && world.PendingCaches() == 0) {
        const bool campaignEnd = config_.mode != GameMode::Endless && director.Sector() + 1 >= Director::kCampaignSectors &&
                                 director.Loop() == 0;
        if (!campaignEnd) {
            if (autopilot_) {
                UpdateAutopilotOverlays();
            } else {
                OpenSectorChoice();
            }
        }
    }
    if (director.Phase() == SectorPhase::Victory && !victoryShown_) {
        victoryShown_ = true;
        if (autopilot_) {
            world.ContinueEndless();
        } else {
            overlay_ = Overlay::Victory;
            overlayTime_ = 0.0f;
            victoryMenu_.SetSelected(0);
            app_.In().ConsumeAll();
            ShowCursor();
            app_.Audio().Music().SetMood(MusicMood::Victory, 0);
        }
    }
}

void GameScene::UpdateAutopilotOverlays() {
    World& world = view_.GetWorld();
    while (world.PendingLevelUps() > 0 || world.PendingCaches() > 0) {
        const bool cache = world.PendingLevelUps() == 0;
        const auto offers = world.RollOffers(3);
        if (!offers.empty()) {
            world.ApplyUpgrade(Bot::ChooseUpgrade(world, offers));
        }
        if (cache) {
            world.ConsumeCache();
        } else {
            world.ConsumeLevelUp();
        }
    }
    if (world.GetDirector().Phase() == SectorPhase::Cleared) {
        world.ChooseSectorModifier(Bot::ChooseModifier(world, SectorModifier::Swarm, SectorModifier::Respite));
    }
}

void GameScene::OpenPause() {
    overlay_ = Overlay::Pause;
    overlayTime_ = 0.0f;
    pauseMenu_.SetSelected(0);
    app_.In().ConsumeAll();
    app_.Audio().Play(SfxId::UiBack, 0.7f);
    ShowCursor();
}

void GameScene::OpenSectorChoice() {
    World& world = view_.GetWorld();
    Rng& rng = world.Random();
    const int count = static_cast<int>(SectorModifier::Count);
    const int a = rng.RangeInt(0, count - 1);
    int b = rng.RangeInt(0, count - 2);
    if (b >= a) {
        ++b;
    }
    modifierChoices_ = {static_cast<SectorModifier>(a), static_cast<SectorModifier>(b)};
    modifierSelected_ = 0;
    overlay_ = Overlay::SectorChoice;
    overlayTime_ = 0.0f;
    app_.In().ConsumeAll();
    ShowCursor();
}

void GameScene::UpdateOverlays(float dt) {
    overlayTime_ += dt;
    World& world = view_.GetWorld();
    Input& in = app_.In();
    auto resume = [&] {
        overlay_ = Overlay::None;
        in.ConsumeAll();
        if (!autopilot_) {
            HideCursor();
        }
    };

    switch (overlay_) {
    case Overlay::Draft:
        if (draft_.Update(app_, world, dt)) {
            if (world.PendingLevelUps() > 0 || world.PendingCaches() > 0) {
                draft_.Open(app_, world, world.PendingLevelUps() == 0);
            } else {
                resume();
            }
        }
        break;
    case Overlay::Pause: {
        const int activated = pauseMenu_.Update(app_);
        if (in.Pressed(Action::Pause) || in.Pressed(Action::Back) || activated == PauseResume) {
            app_.Audio().Play(SfxId::UiConfirm, 0.6f);
            resume();
        } else if (activated == PauseSettings) {
            settings_.Open(app_);
            overlay_ = Overlay::Settings;
        } else if (activated == PauseRestart) {
            app_.Audio().Play(SfxId::UiConfirm, 0.7f);
            RunConfig config = config_;
            if (config.mode != GameMode::Daily) {
                config.seed = static_cast<std::uint64_t>(GetRandomValue(1, 0x7fffffff));
            }
            app_.ChangeScene(std::make_unique<GameScene>(app_, config));
            overlay_ = Overlay::None;
        } else if (activated == PauseQuit) {
            FinishRun(false);
        }
        break;
    }
    case Overlay::Settings:
        if (settings_.Update(app_)) {
            overlay_ = Overlay::Pause;
        }
        break;
    case Overlay::SectorChoice: {
        if (overlayTime_ < 0.5f) {
            break;
        }
        if (in.Repeated(Action::Left) || in.Repeated(Action::Right)) {
            modifierSelected_ = 1 - modifierSelected_;
            app_.Audio().Play(SfxId::UiMove, 0.6f);
        }
        const Vector2 ui = app_.Gfx().UiSize();
        int clicked = -1;
        for (int i = 0; i < 2; ++i) {
            const Rectangle r{ui.x * 0.5f - 470.0f + static_cast<float>(i) * 500.0f, ui.y * 0.5f - 120.0f, 440.0f, 300.0f};
            if (ui::MouseOver(app_, r)) {
                if (in.MouseMoved()) {
                    modifierSelected_ = i;
                }
                if (in.MouseClicked()) {
                    clicked = i;
                }
            }
        }
        if (clicked >= 0 || in.Pressed(Action::Confirm)) {
            if (clicked >= 0) {
                modifierSelected_ = clicked;
            }
            world.ChooseSectorModifier(modifierChoices_[static_cast<std::size_t>(modifierSelected_)]);
            app_.Audio().Play(SfxId::UiConfirm, 0.8f);
            resume();
        }
        break;
    }
    case Overlay::Victory: {
        if (overlayTime_ < 1.0f) {
            break;
        }
        const int activated = victoryMenu_.Update(app_);
        if (activated == VictoryEndless) {
            world.ContinueEndless();
            app_.Audio().Play(SfxId::UiConfirm, 0.8f);
            resume();
        } else if (activated == VictoryFinish) {
            FinishRun(true);
        }
        break;
    }
    case Overlay::None: break;
    }
}

void GameScene::UpdateMusic(float dt) {
    if (finished_) {
        return;
    }
    const World& world = view_.GetWorld();
    MusicEngine& music = app_.Audio().Music();
    const Director& director = world.GetDirector();
    const int key = (director.Sector() % Director::kCampaignSectors) * 5 % 12;  // walk the circle of fourths
    if (!world.PlayerAlive()) {
        music.SetMood(MusicMood::GameOver, key);
        return;
    }
    if (overlay_ == Overlay::Victory) {
        return;
    }
    switch (director.Phase()) {
    case SectorPhase::BossWarning:
    case SectorPhase::Boss: music.SetMood(MusicMood::Boss, key); break;
    case SectorPhase::Victory: music.SetMood(MusicMood::Victory, key); break;
    default: music.SetMood(MusicMood::Game, key); break;
    }
    const float crowd = static_cast<float>(world.ActiveEnemyCount()) / 70.0f;
    const float bullets = static_cast<float>(world.EnemyBullets().size()) / 120.0f;
    float target = std::clamp(0.32f + crowd * 0.45f + bullets * 0.25f, 0.32f, 0.95f);
    if (director.Phase() == SectorPhase::Intro || director.Phase() == SectorPhase::Cleared) {
        target = 0.3f;
    }
    musicIntensity_ = Damp(musicIntensity_, target, 1.2f, dt);
    music.SetIntensity(overlay_ == Overlay::None ? musicIntensity_ : musicIntensity_ * 0.6f);
}

void GameScene::CheckChallenges() {
    if (autopilot_) {
        return;
    }
    Profile& profile = app_.Progress();
    for (const ChallengeInfo& info : AllChallenges()) {
        if (!profile.ChallengeCompleted(info.id) && ChallengeMet(info.id, view_.GetWorld()) &&
            profile.CompleteChallenge(info.id)) {
            hud_.Toast("CHALLENGE COMPLETE", TextFormat("%s   +%d CR", info.name, info.reward), palette::kCache);
            app_.Audio().Play(SfxId::Combo, 0.8f, 0.8f);
        }
    }
}

void GameScene::FinishRun(bool victory) {
    if (finished_) {
        return;
    }
    finished_ = true;
    CheckChallenges();
    const World& world = view_.GetWorld();
    RunSummary summary;
    summary.stats = world.Record();
    summary.stats.victory = victory || world.Record().bossesKilled >= Director::kCampaignSectors;
    summary.config = config_;
    summary.level = world.Level();
    summary.sectorReached = world.GetDirector().Sector() + 1;
    summary.victory = summary.stats.victory;
    Profile& profile = app_.Progress();
    if (!autopilot_) {
        profile.SetTutorialSeen();
    }
    const int credits = profile.RecordRun(summary);
    ShowCursor();
    app_.ChangeScene(std::make_unique<ResultsScene>(app_, summary, credits), 0.6f);
}

void GameScene::DrawWorld() {
    const bool showReticle = !autopilot_ && overlay_ == Overlay::None &&
                             app_.In().LastDevice() == InputDevice::KeyboardMouse && view_.GetWorld().PlayerAlive();
    view_.DrawWorld(MouseWorld(), showReticle);
}

void GameScene::ConfigurePostFx(PostFxParams& params) const {
    const EffectsDirector& fx = view_.Effects();
    params.aberration = fx.Aberration();
    params.flashColor = fx.FlashColor();
    params.flashAmount = fx.FlashAmount();
    if (!view_.GetWorld().PlayerAlive()) {
        params.saturation = std::max(0.25f, 1.0f - deathTimer_ * 0.35f);
    }
    if (overlay_ != Overlay::None && overlay_ != Overlay::Victory) {
        params.saturation = 0.55f;
        params.exposure = 1.1f;
    }
}

void GameScene::DrawUi() {
    hud_.Draw(app_, view_);
    if (app_.DebugOverlay()) {
        DrawDebugOverlay();
    }
    if (showTutorial_ && overlay_ == Overlay::None) {
        DrawTutorial();
    }
    switch (overlay_) {
    case Overlay::Draft: draft_.Draw(app_, view_.GetWorld()); break;
    case Overlay::Pause: DrawPause(); break;
    case Overlay::Settings: settings_.Draw(app_); break;
    case Overlay::SectorChoice: DrawSectorChoice(); break;
    case Overlay::Victory: DrawVictory(); break;
    case Overlay::None: break;
    }
}

void GameScene::DrawDebugOverlay() {
    const World& world = view_.GetWorld();
    const std::array<std::string, 8> lines{{
        TextFormat("FPS %d  (%.2f ms)", GetFPS(), static_cast<double>(GetFrameTime() * 1000.0f)),
        TextFormat("update %.2f ms   draw %.2f ms", static_cast<double>(view_.UpdateMs()),
                   static_cast<double>(view_.DrawMs())),
        TextFormat("enemies %d / %d", world.ActiveEnemyCount(), static_cast<int>(world.Enemies().size())),
        TextFormat("player bullets %d", static_cast<int>(world.Bullets().size())),
        TextFormat("enemy bullets %d", static_cast<int>(world.EnemyBullets().size())),
        TextFormat("pickups %d", static_cast<int>(world.Pickups().size())),
        TextFormat("particles %d", static_cast<int>(view_.ParticleCount())),
        TextFormat("window %dx%d  scene %dx%d", GetRenderWidth(), GetRenderHeight(),
                   static_cast<int>(app_.Gfx().SceneSize().x), static_cast<int>(app_.Gfx().SceneSize().y)),
    }};
    const Rectangle r{20.0f, 140.0f, 330.0f, 30.0f * static_cast<float>(lines.size()) + 20.0f};
    DrawRectangleRec(r, Color{0, 0, 0, 170});
    for (std::size_t i = 0; i < lines.size(); ++i) {
        app_.Gfx().Text().Draw(FontId::BodyBold, lines[i], {r.x + 14.0f, r.y + 10.0f + 30.0f * static_cast<float>(i)},
                               24.0f, palette::kUiGood);
    }
}

void GameScene::DrawTutorial() {
    const Vector2 ui = app_.Gfx().UiSize();
    const float a = Saturate(tutorialTime_ / 0.5f) * Saturate((kTutorialDuration - tutorialTime_) / 1.0f);
    const bool pad = app_.In().LastDevice() == InputDevice::Gamepad;
    struct Hint {
        const char* key;
        const char* label;
    };
    const std::array<Hint, 5> hints = pad ? std::array<Hint, 5>{{{"L-STICK", "MOVE"},
                                                                {"R-STICK", "AIM + FIRE"},
                                                                {"LT / A", "DASH"},
                                                                {"RB / B", "NOVA"},
                                                                {"START", "PAUSE"}}}
                                          : std::array<Hint, 5>{{{"WASD", "MOVE"},
                                                                {"MOUSE", "AIM"},
                                                                {"L-CLICK", "FIRE"},
                                                                {"SPACE", "DASH"},
                                                                {"Q", "NOVA"}}};
    const float width = 1040.0f;
    const Rectangle r{ui.x * 0.5f - width * 0.5f, ui.y - 190.0f, width, 74.0f};
    ui::Panel(app_, r, WithAlpha(palette::kUiAccent, a), a);
    TextRenderer& text = app_.Gfx().Text();
    for (std::size_t i = 0; i < hints.size(); ++i) {
        const float cx = r.x + width / 10.0f + static_cast<float>(i) * width / 5.0f;
        text.Draw(FontId::Heading, hints[i].key, {cx, r.y + 12.0f}, 22.0f, WithAlpha(palette::kUiAccent, a),
                  Align::Center, 0.1f);
        text.Draw(FontId::BodyBold, hints[i].label, {cx, r.y + 40.0f}, 22.0f, WithAlpha(palette::kUiDim, a),
                  Align::Center, 0.1f);
    }
    text.Draw(FontId::BodyBold, "Collect shards to level up. Survive the sector, then destroy its guardian.",
              {ui.x * 0.5f, r.y - 34.0f}, 24.0f, WithAlpha(palette::kUiText, a), Align::Center);
}

void GameScene::DrawPause() {
    const Vector2 ui = app_.Gfx().UiSize();
    const World& world = view_.GetWorld();
    TextRenderer& text = app_.Gfx().Text();
    NeonRenderer& neon = app_.Gfx().Neon();
    ui::Dim(app_, 0.6f);

    text.DrawGlow(FontId::Display, "PAUSED", {ui.x * 0.5f - 380.0f, 170.0f}, 72.0f, WHITE, palette::kUiAccent,
                  Align::Left, 0.1f);
    pauseMenu_.Draw(app_, {ui.x * 0.5f - 380.0f, 300.0f}, 420.0f, 64.0f, palette::kUiAccent);

    // Run summary and current build.
    const Rectangle panel{ui.x * 0.5f + 100.0f, 170.0f, 520.0f, 700.0f};
    ui::Panel(app_, panel, palette::kUiAccent);
    const RunStats& stats = world.Record();
    const ShipInfo& ship = GetShipInfo(config_.ship);
    text.Draw(FontId::Heading, ship.name, {panel.x + 30.0f, panel.y + 26.0f}, 30.0f, ship.color, Align::Left, 0.12f);
    text.Draw(FontId::BodyBold,
              TextFormat("%s  -  %s", DifficultyName(config_.difficulty), config_.mode == GameMode::Daily ? "DAILY RUN"
                                                                           : config_.mode == GameMode::Endless
                                                                               ? "ENDLESS"
                                                                               : "CAMPAIGN"),
              {panel.x + 30.0f, panel.y + 66.0f}, 22.0f, palette::kUiDim);
    const int seconds = static_cast<int>(stats.time);
    text.Draw(FontId::BodyBold,
              TextFormat("TIME %d:%02d     KILLS %d     LEVEL %d", seconds / 60, seconds % 60, stats.kills, world.Level()),
              {panel.x + 30.0f, panel.y + 100.0f}, 22.0f, palette::kUiText);

    text.Draw(FontId::Heading, "BUILD", {panel.x + 30.0f, panel.y + 150.0f}, 20.0f, palette::kUiAccent, Align::Left,
              0.2f);
    int row = 0;
    neon.Begin();
    std::vector<std::pair<UpgradeId, int>> owned;
    for (const UpgradeInfo& info : AllUpgrades()) {
        const int stacks = world.Inventory().Stacks(info.id);
        if (stacks > 0 && info.maxStacks != 0) {
            owned.emplace_back(info.id, stacks);
        }
    }
    const float rowHeight = 34.0f;
    const int columns = 2;
    for (const auto& [id, stacks] : owned) {
        const int col = row % columns;
        const int line = row / columns;
        const Vector2 c{panel.x + 48.0f + static_cast<float>(col) * 245.0f, panel.y + 200.0f + line * rowHeight};
        if (c.y > panel.y + panel.height - 30.0f) {
            break;
        }
        shapes::DrawUpgradeIcon(neon, id, c, 24.0f, RarityColor(GetUpgradeInfo(id).rarity), overlayTime_);
        ++row;
    }
    neon.End();
    row = 0;
    for (const auto& [id, stacks] : owned) {
        const int col = row % columns;
        const int line = row / columns;
        const Vector2 c{panel.x + 48.0f + static_cast<float>(col) * 245.0f, panel.y + 200.0f + line * rowHeight};
        if (c.y > panel.y + panel.height - 30.0f) {
            break;
        }
        const char* label = stacks > 1 ? TextFormat("%s  %d", GetUpgradeInfo(id).name, stacks) : GetUpgradeInfo(id).name;
        text.Draw(FontId::BodyBold, label, {c.x + 24.0f, c.y - 11.0f}, 20.0f, palette::kUiText);
        ++row;
    }
    if (owned.empty()) {
        text.Draw(FontId::Body, "No upgrades yet.", {panel.x + 30.0f, panel.y + 190.0f}, 22.0f, palette::kUiFaint);
    }
}

void GameScene::DrawSectorChoice() {
    const Vector2 ui = app_.Gfx().UiSize();
    TextRenderer& text = app_.Gfx().Text();
    NeonRenderer& neon = app_.Gfx().Neon();
    const float a = Saturate(overlayTime_ / 0.4f);
    ui::Dim(app_, 0.6f * a);
    text.DrawGlow(FontId::Display, "ROUTE THE NEXT SECTOR", {ui.x * 0.5f, ui.y * 0.5f - 260.0f}, 54.0f,
                  WithAlpha(WHITE, a), WithAlpha(palette::kUiGood, a), Align::Center, 0.08f);
    text.Draw(FontId::BodyBold, "Choose a protocol for the sector ahead.", {ui.x * 0.5f, ui.y * 0.5f - 190.0f}, 26.0f,
              WithAlpha(palette::kUiDim, a), Align::Center);
    for (int i = 0; i < 2; ++i) {
        const SectorModifierInfo& info = GetSectorModifierInfo(modifierChoices_[static_cast<std::size_t>(i)]);
        const bool selected = i == modifierSelected_;
        Rectangle r{ui.x * 0.5f - 470.0f + static_cast<float>(i) * 500.0f, ui.y * 0.5f - 120.0f, 440.0f, 300.0f};
        if (selected) {
            r.y -= 12.0f;
        }
        DrawRectangleRec(r, WithAlpha(palette::kUiPanel, 0.94f * a));
        DrawRectangleRec({r.x, r.y, r.width, 6.0f}, WithAlpha(info.color, a));
        neon.Begin();
        const std::array<Vector2, 4> corners{Vector2{r.x, r.y}, Vector2{r.x + r.width, r.y},
                                             Vector2{r.x + r.width, r.y + r.height}, Vector2{r.x, r.y + r.height}};
        neon.Polyline(corners, true, selected ? 3.0f : 1.5f, WithAlpha(selected ? info.color : palette::kUiBorder, a));
        neon.RegularPolygon({r.x + r.width * 0.5f, r.y + 90.0f}, 6, 40.0f, overlayTime_ * 0.5f, 2.5f,
                            WithAlpha(info.color, a));
        neon.End();
        text.Draw(FontId::Heading, info.name, {r.x + r.width * 0.5f, r.y + 150.0f}, 28.0f, WithAlpha(WHITE, a),
                  Align::Center, 0.08f);
        text.DrawWrapped(FontId::Body, info.description, {r.x + 30.0f, r.y + 196.0f}, r.width - 60.0f, 26.0f,
                         WithAlpha(palette::kUiText, a), 1.15f, Align::Center);
    }
    ui::Prompt(app_, {ui.x * 0.5f, ui.y * 0.5f + 230.0f}, "ENTER", "A", "CONFIRM", Align::Center);
}

void GameScene::DrawVictory() {
    const Vector2 ui = app_.Gfx().UiSize();
    TextRenderer& text = app_.Gfx().Text();
    const float a = Saturate(overlayTime_ / 0.8f);
    ui::Dim(app_, 0.55f * a);
    text.DrawGlow(FontId::Display, "SYSTEM LIBERATED", {ui.x * 0.5f, ui.y * 0.5f - 220.0f}, 84.0f, WithAlpha(WHITE, a),
                  WithAlpha(palette::kUiGood, a), Align::Center, 0.08f);
    text.Draw(FontId::BodyBold, "The Core is down. The grid is yours - or keep breaking it.",
              {ui.x * 0.5f, ui.y * 0.5f - 110.0f}, 28.0f, WithAlpha(palette::kUiText, a), Align::Center);
    if (overlayTime_ >= 1.0f) {
        victoryMenu_.Draw(app_, {ui.x * 0.5f - 260.0f, ui.y * 0.5f - 20.0f}, 520.0f, 66.0f, palette::kUiGood);
    }
}

} // namespace gb
