#include "scenes/TitleScene.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "core/BuildInfo.h"
#include "game/Profile.h"
#include "scenes/GameScene.h"
#include "scenes/HangarScene.h"
#include "view/Palette.h"

#include <algorithm>

namespace gb {

namespace {

enum TitleItem {
    ItemPlay,
    ItemDaily,
    ItemHangar,
    ItemRecords,
    ItemCodex,
    ItemSettings,
    ItemQuit
};

} // namespace

TitleScene::TitleScene(App& app) : Scene(app) {
    menu_.SetItems({{"PLAY"}, {"DAILY RUN"}, {"HANGAR"}, {"RECORDS"}, {"CODEX"}, {"SETTINGS"}, {"QUIT"}});
    menu_.SetValue(ItemCodex, app.Progress().CodexHasNews() ? "NEW" : "");
    ResetDemo();
    // Debug entry points for screenshots.
    if (app.Options().startScene == "records") {
        records_.Open(app);
    }
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
    config.meta.bonusHp = 6; // the demo should survive long enough to look good
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
    if (records_.Active()) {
        records_.Update(app_);
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
        records_.Open(app_);
        app_.In().ConsumeAll();
        break;
    case ItemCodex:
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        codex_.Open(app_);
        app_.Progress().MarkCodexViewed();
        app_.Progress().Save();
        menu_.SetValue(ItemCodex, "");
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
        text.Draw(FontId::Display, "GRIDBREAKER", {center.x - offset, center.y}, size,
                  WithAlpha({255, 40, 120, 255}, 0.7f), Align::Center, 0.06f);
        text.Draw(FontId::Display, "GRIDBREAKER", {center.x + offset, center.y}, size,
                  WithAlpha({40, 220, 255, 255}, 0.7f), Align::Center, 0.06f);
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
    if (records_.Active()) {
        records_.Draw(app_);
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
                  TextFormat("BEST  %lld",
                             std::max({profile.BestScore(Difficulty::Relaxed), profile.BestScore(Difficulty::Standard),
                                       profile.BestScore(Difficulty::Overclocked)})),
                  {ui.x - 60.0f, ui.y - 76.0f}, 24.0f, palette::kUiDim, Align::Right, 0.05f);
    }
    text.Draw(FontId::Body, TextFormat("v%s  (%s)", build::kVersion, build::kGitHash), {24.0f, ui.y - 40.0f}, 20.0f,
              palette::kUiFaint);
    if (app_.In().LastDevice() == InputDevice::KeyboardMouse) {
        text.Draw(FontId::Body, "F11 / ALT+ENTER  TOGGLE FULLSCREEN", {24.0f, ui.y - 66.0f}, 20.0f, palette::kUiFaint);
    }
    ui::Prompt(app_, {ui.x * 0.5f, ui.y - 52.0f}, "ENTER", "A", "SELECT", Align::Center, 22.0f);
    if (menu_.Selected() == ItemDaily) {
        text.Draw(FontId::Body, "Same seed and ship for everyone today. One shot at glory.",
                  {600.0f, 430.0f + 70.0f + 18.0f}, 22.0f, palette::kUiDim);
    }

    settings_.Draw(app_);
}

} // namespace gb
