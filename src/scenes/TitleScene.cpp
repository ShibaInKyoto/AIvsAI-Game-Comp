#include "scenes/TitleScene.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "core/BuildInfo.h"
#include "core/Format.h"
#include "game/Profile.h"
#include "scenes/GameScene.h"
#include "scenes/HangarScene.h"
#include "view/Palette.h"

#include <algorithm>

namespace gb {

namespace {

enum TitleItem { ItemPlay, ItemDaily, ItemHangar, ItemRecords, ItemCodex, ItemSettings, ItemQuit };

} // namespace

TitleScene::TitleScene(App& app) : Scene(app) {
    menu_.SetItems({{"PLAY"}, {"DAILY RUN"}, {"HANGAR"}, {"RECORDS"}, {"CODEX"}, {"SETTINGS"}, {"QUIT"}});
    ResetDemo();
    // Debug entry points for screenshots.
    showRecords_ = app.Options().startScene == "records";
    if (app.Options().startScene == "codex") {
        codex_.Open(app);
    }
}

void TitleScene::OnEnter() {
    app_.Audio().Music().SetMood(MusicMood::Menu, 0);
    ShowCursor();
}

void TitleScene::ResetDemo() {
    RunConfig config;
    config.difficulty = Difficulty::Relaxed;
    config.ship = static_cast<ShipId>(demoCounter_ % static_cast<int>(ShipId::Count));
    config.seed = 0xC0FFEEu + static_cast<std::uint64_t>(demoCounter_) * 977u;
    config.meta.bonusHp = 6;  // the demo should survive long enough to look good
    ++demoCounter_;
    demo_ = std::make_unique<GameView>(app_, config);
    demo_->Effects().SetMuted(true);
    // Drop the demo into the thick of a sector with a matching loadout so the
    // backdrop is always busy.
    World& world = demo_->GetWorld();
    const int sector = demoCounter_ % 3;
    world.GetDirector().JumpToWaves(sector, 35.0f);
    for (int i = 0; i < 8 + sector * 6; ++i) {
        const auto offers = world.RollOffers(3);
        if (!offers.empty()) {
            world.ApplyUpgrade(Bot::ChooseUpgrade(world, offers));
        }
    }
    demoDeathTimer_ = 0.0f;
}

void TitleScene::StartDaily() {
    RunConfig config;
    config.mode = GameMode::Daily;
    config.difficulty = Difficulty::Standard;
    const int stamp = TodayStamp();
    config.seed = DailySeed(stamp);
    // The daily ship rotates through the full roster, unlocked or not.
    config.ship = static_cast<ShipId>(config.seed % static_cast<std::uint64_t>(ShipId::Count));
    config.meta = app_.Progress().Bonuses();
    app_.ChangeScene(std::make_unique<GameScene>(app_, config), 0.5f);
}

void TitleScene::Update(float dt) {
    time_ += dt;

    // Attract mode: the bot plays on, quietly (its sounds would compete with the UI).
    World& world = demo_->GetWorld();
    while (world.PendingLevelUps() > 0 || world.PendingCaches() > 0) {
        const auto offers = world.RollOffers(3);
        if (!offers.empty()) {
            world.ApplyUpgrade(Bot::ChooseUpgrade(world, offers));
        }
        if (world.PendingLevelUps() > 0) {
            world.ConsumeLevelUp();
        } else {
            world.ConsumeCache();
        }
    }
    if (world.GetDirector().Phase() == SectorPhase::Cleared) {
        world.ChooseSectorModifier(SectorModifier::Respite);
    }
    demo_->Update(dt, bot_.Think(world, dt), true);
    if (!world.PlayerAlive() || world.GetDirector().Phase() == SectorPhase::Victory) {
        demoDeathTimer_ += dt;
        if (demoDeathTimer_ > 3.0f) {
            ResetDemo();
        }
    }

    if (settings_.Active()) {
        settings_.Update(app_);
        return;
    }
    if (codex_.Active()) {
        codex_.Update(app_, dt);
        return;
    }
    if (showRecords_) {
        if (app_.In().Pressed(Action::Back) || app_.In().Pressed(Action::Confirm) || app_.In().MouseClicked()) {
            showRecords_ = false;
            app_.Audio().Play(SfxId::UiBack, 0.7f);
            app_.In().ConsumeAll();
        }
        return;
    }

    const int activated = menu_.Update(app_);
    switch (activated) {
    case ItemPlay:
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        app_.ChangeScene(std::make_unique<HangarScene>(app_, false));
        break;
    case ItemDaily:
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        StartDaily();
        break;
    case ItemHangar:
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        app_.ChangeScene(std::make_unique<HangarScene>(app_, true));
        break;
    case ItemRecords:
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        showRecords_ = true;
        app_.In().ConsumeAll();
        break;
    case ItemCodex:
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        codex_.Open(app_);
        break;
    case ItemSettings:
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        settings_.Open(app_);
        break;
    case ItemQuit: app_.Quit(); break;
    default: break;
    }
}

void TitleScene::DrawWorld() { demo_->DrawWorld({}, false); }

void TitleScene::ConfigurePostFx(PostFxParams& params) const {
    params.saturation = 0.85f;
    params.exposure = 1.25f;
    params.vignette = 0.75f;
}

void TitleScene::DrawLogo(Vector2 center, float scale) {
    TextRenderer& text = app_.Gfx().Text();
    const float size = 150.0f * scale;
    // Occasional glitch: split the logo into offset colour channels.
    const float glitch = std::fmod(time_, 4.3f) < 0.12f ? 1.0f : 0.0f;
    if (glitch > 0.0f) {
        const float offset = 10.0f * scale;
        text.Draw(FontId::Display, "GRIDBREAKER", {center.x - offset, center.y}, size, WithAlpha({255, 40, 120, 255}, 0.7f),
                  Align::Center, 0.06f);
        text.Draw(FontId::Display, "GRIDBREAKER", {center.x + offset, center.y}, size, WithAlpha({40, 220, 255, 255}, 0.7f),
                  Align::Center, 0.06f);
    }
    text.DrawGlow(FontId::Display, "GRIDBREAKER", center, size, WHITE, palette::kUiAccent, Align::Center, 0.06f);
    const float lineY = center.y + size * 1.02f;
    const float halfWidth = 560.0f * scale;
    NeonRenderer& neon = app_.Gfx().Neon();
    neon.Begin();
    neon.Line({center.x - halfWidth, lineY}, {center.x + halfWidth, lineY}, 3.0f, palette::kUiAccent);
    const float sweep = std::fmod(time_ * 0.35f, 1.0f);
    neon.Glow({center.x - halfWidth + 2.0f * halfWidth * sweep, lineY}, 40.0f, WithAlpha(WHITE, 0.7f));
    neon.End();
    text.Draw(FontId::Heading, "BREAK THE GRID  //  ESCAPE THE CORE", {center.x, lineY + 18.0f}, 22.0f * scale,
              palette::kUiDim, Align::Center, 0.35f);
}

void TitleScene::DrawRecords() {
    const Vector2 ui = app_.Gfx().UiSize();
    const Profile& p = app_.Progress();
    ui::Dim(app_, 0.6f);
    const float width = std::min(1500.0f, ui.x - 80.0f);
    const Rectangle panel{ui.x * 0.5f - width * 0.5f, ui.y * 0.5f - 400.0f, width, 800.0f};
    ui::Panel(app_, panel, palette::kUiAccent);
    TextRenderer& text = app_.Gfx().Text();
    NeonRenderer& neon = app_.Gfx().Neon();
    text.Draw(FontId::Heading, "RECORDS", {panel.x + 40.0f, panel.y + 30.0f}, 34.0f, WHITE, Align::Left, 0.15f);
    const int minutes = static_cast<int>(p.PlayTime() / 60.0f);
    struct Row {
        const char* label;
        std::string value;
    };
    const std::array<Row, 9> rows{{
        {"RUNS", std::to_string(p.Runs())},
        {"VICTORIES", std::to_string(p.Wins())},
        {"FURTHEST SECTOR", p.BestSector() > 0 ? std::to_string(p.BestSector()) : "-"},
        {"ENEMIES DESTROYED", FormatThousands(p.TotalKills())},
        {"GUARDIANS DESTROYED", std::to_string(p.BossKills())},
        {"TIME PLAYED", std::to_string(minutes / 60) + "h " + std::to_string(minutes % 60) + "m"},
        {"BEST - RELAXED", FormatThousands(p.BestScore(Difficulty::Relaxed))},
        {"BEST - STANDARD", FormatThousands(p.BestScore(Difficulty::Standard))},
        {"BEST - OVERCLOCKED", FormatThousands(p.BestScore(Difficulty::Overclocked))},
    }};
    const float leftWidth = width * 0.38f;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = panel.y + 100.0f + static_cast<float>(i) * 50.0f;
        text.Draw(FontId::BodyBold, rows[i].label, {panel.x + 40.0f, y}, 26.0f, palette::kUiDim);
        text.Draw(FontId::BodyBold, rows[i].value, {panel.x + leftWidth, y - 2.0f}, 30.0f, palette::kUiText, Align::Right);
    }
    if (p.DailyDate() == TodayStamp() && p.DailyBest() > 0) {
        text.Draw(FontId::BodyBold, TextFormat("TODAY'S DAILY BEST: %s", FormatThousands(p.DailyBest()).c_str()),
                  {panel.x + 40.0f, panel.y + 570.0f}, 24.0f, palette::kUiWarn);
    }

    // Challenges column.
    const float cx = panel.x + leftWidth + 70.0f;
    const float columnWidth = panel.x + width - 40.0f - cx;
    text.Draw(FontId::Heading, TextFormat("CHALLENGES  %d / %d", p.CompletedChallengeCount(),
                                          static_cast<int>(AllChallenges().size())),
              {cx, panel.y + 34.0f}, 26.0f, palette::kCache, Align::Left, 0.1f);
    const auto challenges = AllChallenges();
    const float rowHeight = 41.0f;
    neon.Begin();
    for (std::size_t i = 0; i < challenges.size(); ++i) {
        const bool done = p.ChallengeCompleted(challenges[i].id);
        const Vector2 c{cx + 12.0f, panel.y + 100.0f + static_cast<float>(i) * rowHeight + 13.0f};
        neon.RegularPolygon(c, 4, 9.0f, kPi * 0.25f, 2.0f, done ? palette::kCache : WithAlpha(palette::kUiFaint, 0.8f));
        if (done) {
            neon.Glow(c, 10.0f, palette::kCache);
        }
    }
    neon.End();
    for (std::size_t i = 0; i < challenges.size(); ++i) {
        const ChallengeInfo& info = challenges[i];
        const bool done = p.ChallengeCompleted(info.id);
        const float y = panel.y + 100.0f + static_cast<float>(i) * rowHeight;
        text.Draw(FontId::Heading, info.name, {cx + 36.0f, y}, 19.0f, done ? WHITE : palette::kUiDim, Align::Left, 0.08f);
        text.Draw(FontId::Body, info.description, {cx + 300.0f, y - 2.0f}, 23.0f,
                  done ? palette::kUiText : palette::kUiFaint);
        text.Draw(FontId::BodyBold, TextFormat("%d CR", info.reward), {cx + columnWidth, y - 2.0f}, 22.0f,
                  done ? palette::kUiFaint : palette::kCache, Align::Right);
    }
}

void TitleScene::DrawUi() {
    const Vector2 ui = app_.Gfx().UiSize();
    // Darken the left side so the menu reads over the demo.
    for (int i = 0; i < 24; ++i) {
        const float t = static_cast<float>(i) / 24.0f;
        DrawRectangleRec({t * 900.0f, 0.0f, 900.0f / 24.0f + 1.0f, ui.y},
                         WithAlpha(Color{2, 4, 12, 255}, 0.78f * (1.0f - t)));
    }
    DrawLogo({ui.x * 0.5f, 120.0f}, 1.0f);
    menu_.Draw(app_, {110.0f, 430.0f}, 460.0f, 70.0f, palette::kUiAccent);
    // Full-screen panels replace the footer, which would only peek out from under them.
    if (showRecords_) {
        DrawRecords();
        return;
    }
    if (codex_.Active()) {
        codex_.Draw(app_);
        return;
    }

    TextRenderer& text = app_.Gfx().Text();
    const Profile& profile = app_.Progress();
    text.Draw(FontId::BodyBold, TextFormat("CREDITS  %d", profile.Credits()), {ui.x - 60.0f, ui.y - 112.0f}, 28.0f,
              palette::kCache, Align::Right, 0.05f);
    if (profile.Runs() > 0) {
        text.Draw(FontId::BodyBold,
                  TextFormat("BEST  %lld", std::max({profile.BestScore(Difficulty::Relaxed),
                                                     profile.BestScore(Difficulty::Standard),
                                                     profile.BestScore(Difficulty::Overclocked)})),
                  {ui.x - 60.0f, ui.y - 76.0f}, 24.0f, palette::kUiDim, Align::Right, 0.05f);
    }
    text.Draw(FontId::Body, TextFormat("v%s  (%s)", build::kVersion, build::kGitHash), {24.0f, ui.y - 40.0f}, 20.0f,
              palette::kUiFaint);
    ui::Prompt(app_, {ui.x * 0.5f, ui.y - 52.0f}, "ENTER", "A", "SELECT", Align::Center, 22.0f);
    if (menu_.Selected() == ItemDaily) {
        text.Draw(FontId::Body, "Same seed and ship for everyone today. One shot at glory.", {600.0f, 430.0f + 70.0f + 18.0f},
                  22.0f, palette::kUiDim);
    }

    settings_.Draw(app_);
}

} // namespace gb
