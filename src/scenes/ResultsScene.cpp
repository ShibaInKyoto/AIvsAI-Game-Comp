#include "scenes/ResultsScene.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "core/Format.h"
#include "game/EnemyCatalog.h"
#include "scenes/GameScene.h"
#include "scenes/HangarScene.h"
#include "scenes/TitleScene.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>
#include <string>
#include <vector>

namespace gb {

namespace {

enum ResultItem {
    ItemRetry,
    ItemHangar,
    ItemTitle
};

constexpr int kRowCount = 8;
constexpr float kRowInterval = 0.14f;

} // namespace

ResultsScene::ResultsScene(App& app, const RunSummary& summary, int creditsEarned)
    : Scene(app), summary_(summary), creditsEarned_(creditsEarned),
      backdrop_(summary.victory ? palette::kUiGood : palette::kDanger) {
    const Profile& profile = app.Progress();
    // RecordRun already folded this run in, so a tie with the best means it set it.
    newRecord_ = summary.stats.score > 0 && summary.stats.score >= profile.BestScoreFor(summary.config);
    for (const ShipInfo& ship : AllShips()) {
        if (!profile.ShipUnlocked(ship.id) && profile.Credits() >= ship.cost) {
            unlockAvailable_ = true;
        }
    }
    menu_.SetItems({{summary.config.mode == GameMode::Daily ? "RETRY DAILY" : "RETRY"}, {"HANGAR"}, {"MAIN MENU"}});
}

void ResultsScene::OnEnter() {
    app_.Audio().Music().SetMood(summary_.victory ? MusicMood::Victory : MusicMood::GameOver, 0);
    ShowCursor();
}

void ResultsScene::Update(float dt) {
    time_ += dt;
    backdrop_.Update(dt);
    rowTimer_ += dt;
    if (revealedRows_ < kRowCount && rowTimer_ >= kRowInterval && time_ > 0.6f) {
        rowTimer_ = 0.0f;
        ++revealedRows_;
        app_.Audio().Play(SfxId::UiMove, 0.6f, 0.9f + 0.05f * static_cast<float>(revealedRows_));
    }
    if (revealedRows_ >= kRowCount) {
        const float score = static_cast<float>(summary_.stats.score);
        displayedScore_ = Damp(displayedScore_, score, 0.15f, dt);
        if (score - displayedScore_ < 1.0f) {
            displayedScore_ = score;
        }
        displayedCredits_ = Damp(displayedCredits_, static_cast<float>(creditsEarned_), 0.2f, dt);
        if (static_cast<float>(creditsEarned_) - displayedCredits_ < 0.5f) {
            displayedCredits_ = static_cast<float>(creditsEarned_);
        }
    }

    // Skip the tally on any input.
    Input& in = app_.In();
    if (revealedRows_ < kRowCount && (in.Pressed(Action::Confirm) || in.MouseClicked())) {
        revealedRows_ = kRowCount;
        in.ConsumeAll();
        return;
    }
    if (time_ < 1.0f) {
        return;
    }
    const int activated = menu_.Update(app_);
    if (activated == ItemRetry) {
        app_.Audio().Play(SfxId::UiConfirm, 0.9f);
        RunConfig config = summary_.config;
        if (config.mode != GameMode::Daily) {
            config.seed = static_cast<std::uint64_t>(GetRandomValue(1, 0x7fffffff)) ^
                          (static_cast<std::uint64_t>(GetTime() * 1000.0) << 20);
        }
        config.meta = app_.Progress().Bonuses();
        app_.ChangeScene(std::make_unique<GameScene>(app_, config), 0.5f);
    } else if (activated == ItemHangar) {
        app_.Audio().Play(SfxId::UiConfirm, 0.8f);
        app_.ChangeScene(std::make_unique<HangarScene>(app_, false));
    } else if (activated == ItemTitle || in.Pressed(Action::Back)) {
        app_.Audio().Play(SfxId::UiBack, 0.8f);
        app_.ChangeScene(std::make_unique<TitleScene>(app_));
    }
}

void ResultsScene::DrawWorld() { backdrop_.Draw(app_); }

void ResultsScene::DrawUi() {
    const Vector2 ui = app_.Gfx().UiSize();
    TextRenderer& text = app_.Gfx().Text();
    const RunStats& s = summary_.stats;
    const Color accent = summary_.victory ? palette::kUiGood : palette::kDanger;
    const float cx = ui.x * 0.5f;

    const float titleIn = EaseOutCubic(Saturate(time_ / 0.6f));
    text.DrawGlow(FontId::Display, summary_.victory ? "SYSTEM LIBERATED" : "SIGNAL LOST",
                  {cx, 70.0f - (1.0f - titleIn) * 40.0f}, 86.0f, WithAlpha(WHITE, titleIn), WithAlpha(accent, titleIn),
                  Align::Center, 0.08f);
    const ShipInfo& ship = GetShipInfo(summary_.config.ship);
    const char* mode = GameModeName(summary_.config.mode);
    text.Draw(FontId::Heading,
              TextFormat("%s  //  %s  //  %s", ship.name, DifficultyName(summary_.config.difficulty), mode),
              {cx, 178.0f}, 22.0f, WithAlpha(palette::kUiDim, titleIn), Align::Center, 0.2f);

    // Stats on the left, the run's loadout on the right.
    const float totalWidth = std::min(1480.0f, ui.x - 80.0f);
    const float statsWidth = std::min(860.0f, totalWidth * 0.58f);
    const float left = cx - totalWidth * 0.5f;
    const Rectangle panel{left, 230.0f, statsWidth, 560.0f};
    const Rectangle loadout{left + statsWidth + 30.0f, 230.0f, totalWidth - statsWidth - 30.0f, 560.0f};
    ui::Panel(app_, panel, accent, titleIn);
    DrawLoadout(loadout, accent, titleIn);
    const int seconds = static_cast<int>(s.time);
    const std::array<std::pair<const char*, std::string>, kRowCount> rows{{
        {"TIME SURVIVED", TextFormat("%d:%02d", seconds / 60, seconds % 60)},
        {"SECTOR REACHED", std::to_string(summary_.sectorReached)},
        {"LEVEL", std::to_string(summary_.level)},
        {"ENEMIES DESTROYED", FormatThousands(s.kills)},
        {"ELITES / GUARDIANS", std::to_string(s.elitesKilled) + " / " + std::to_string(s.bossesKilled)},
        {"BEST COMBO", std::to_string(s.bestCombo)},
        {"DAMAGE DEALT", FormatThousands(static_cast<long long>(s.damageDealt))},
        {"HITS TAKEN", std::to_string(s.hitsTaken)},
    }};
    for (int i = 0; i < revealedRows_ && i < kRowCount; ++i) {
        const float y = panel.y + 36.0f + static_cast<float>(i) * 44.0f;
        text.Draw(FontId::BodyBold, rows[static_cast<std::size_t>(i)].first, {panel.x + 60.0f, y}, 28.0f,
                  palette::kUiDim);
        text.Draw(FontId::BodyBold, rows[static_cast<std::size_t>(i)].second, {panel.x + panel.width - 60.0f, y - 3.0f},
                  32.0f, palette::kUiText, Align::Right);
    }
    if (revealedRows_ >= kRowCount) {
        const float y = panel.y + 400.0f;
        text.Draw(FontId::BodyBold, "SCORE", {panel.x + 60.0f, y + 14.0f}, 30.0f, palette::kUiDim);
        text.DrawGlow(FontId::BodyBold, FormatThousands(static_cast<long long>(displayedScore_)),
                      {panel.x + panel.width - 60.0f, y - 8.0f}, 70.0f, WHITE, accent, Align::Right, 0.02f);
        if (newRecord_ && displayedScore_ >= static_cast<float>(s.score)) {
            const float pulse = 0.6f + 0.4f * std::sin(time_ * 6.0f);
            text.Draw(FontId::Heading, "NEW RECORD", {panel.x + panel.width * 0.5f, y + 20.0f}, 26.0f,
                      WithAlpha(palette::kUiWarn, pulse), Align::Center, 0.2f);
        }
        text.Draw(FontId::BodyBold, "CREDITS EARNED", {panel.x + 60.0f, y + 96.0f}, 28.0f, palette::kUiDim);
        text.Draw(FontId::BodyBold, TextFormat("+%d", static_cast<int>(displayedCredits_)),
                  {panel.x + panel.width - 60.0f, y + 88.0f}, 42.0f, palette::kCache, Align::Right);
    }

    if (time_ >= 1.0f) {
        menu_.Draw(app_, {cx - 230.0f, panel.y + panel.height + 30.0f}, 460.0f, 60.0f, accent);
    }
    if (unlockAvailable_ && revealedRows_ >= kRowCount) {
        const float pulse = 0.6f + 0.4f * std::sin(time_ * 4.0f);
        text.Draw(FontId::Heading, "YOU CAN AFFORD A NEW SHIP IN THE HANGAR", {cx, ui.y - 60.0f}, 22.0f,
                  WithAlpha(palette::kCache, pulse), Align::Center, 0.15f);
    }
}

void ResultsScene::DrawLoadout(Rectangle panel, Color accent, float alpha) {
    TextRenderer& text = app_.Gfx().Text();
    NeonRenderer& neon = app_.Gfx().Neon();
    ui::Panel(app_, panel, accent, alpha);

    // Unique upgrades in the order they were first taken.
    std::vector<UpgradeId> picks;
    for (UpgradeId id : summary_.build.Order()) {
        if (std::find(picks.begin(), picks.end(), id) == picks.end()) {
            picks.push_back(id);
        }
    }
    text.Draw(FontId::Heading, "LOADOUT", {panel.x + 36.0f, panel.y + 30.0f}, 24.0f, WithAlpha(WHITE, alpha),
              Align::Left, 0.15f);
    text.Draw(FontId::BodyBold, TextFormat("%d UPGRADES", static_cast<int>(picks.size())),
              {panel.x + panel.width - 36.0f, panel.y + 28.0f}, 24.0f, WithAlpha(palette::kUiDim, alpha), Align::Right);

    const EnemyKind killer = summary_.stats.killedBy;
    const bool showKiller = !summary_.victory && killer != EnemyKind::Count;
    // Shrink tiles until the whole build fits above the "destroyed by" footer.
    const float gridTop = panel.y + 84.0f;
    const float gridBottom = panel.y + panel.height - (showKiller ? 140.0f : 30.0f);
    const float gridWidth = panel.width - 72.0f;
    constexpr float kGap = 10.0f;
    float tile = 76.0f;
    int columns = 1;
    for (; tile > 36.0f; tile -= 4.0f) {
        columns = std::max(1, static_cast<int>((gridWidth + kGap) / (tile + kGap)));
        const int rows = (static_cast<int>(picks.size()) + columns - 1) / columns;
        if (static_cast<float>(rows) * (tile + kGap) - kGap <= gridBottom - gridTop) {
            break;
        }
    }
    auto tileRect = [&](int i) {
        return Rectangle{panel.x + 36.0f + static_cast<float>(i % columns) * (tile + kGap),
                         gridTop + static_cast<float>(i / columns) * (tile + kGap), tile, tile};
    };
    // Icons pop in one by one after the panel appears.
    auto reveal = [&](int i) { return Saturate((time_ - 0.7f - 0.05f * static_cast<float>(i)) / 0.2f) * alpha; };

    const int count = static_cast<int>(picks.size());
    for (int i = 0; i < count; ++i) {
        DrawRectangleRec(tileRect(i), WithAlpha(palette::kUiPanelLight, 0.9f * reveal(i)));
    }
    neon.Begin();
    for (int i = 0; i < count; ++i) {
        const float a = reveal(i);
        if (a <= 0.0f) {
            continue;
        }
        const Rectangle r = tileRect(i);
        const UpgradeInfo& info = GetUpgradeInfo(picks[static_cast<std::size_t>(i)]);
        const Color rarity = RarityColor(info.rarity);
        const std::array<Vector2, 4> corners{Vector2{r.x, r.y}, Vector2{r.x + r.width, r.y},
                                             Vector2{r.x + r.width, r.y + r.height}, Vector2{r.x, r.y + r.height}};
        neon.Polyline(corners, true, 1.5f, WithAlpha(rarity, 0.6f * a));
        shapes::DrawUpgradeIcon(neon, info.id, {r.x + r.width * 0.5f, r.y + r.height * 0.5f}, r.width * 0.62f,
                                WithAlpha(LerpColor(rarity, WHITE, 0.2f), a), time_);
    }
    if (showKiller) {
        const float y = panel.y + panel.height - 110.0f;
        neon.Line({panel.x + 36.0f, y}, {panel.x + panel.width - 36.0f, y}, 1.5f,
                  WithAlpha(palette::kUiBorder, 0.7f * alpha));
        const EnemyInfo& info = GetEnemyInfo(killer);
        const Vector2 icon{panel.x + 76.0f, y + 56.0f};
        neon.Glow(icon, 50.0f, WithAlpha(info.color, 0.15f * alpha));
        shapes::DrawEnemyPreview(neon, killer, icon, 28.0f / info.radius, time_);
    }
    neon.End();
    for (int i = 0; i < count; ++i) {
        const int stacks = summary_.build.Stacks(picks[static_cast<std::size_t>(i)]);
        if (stacks > 1) {
            const Rectangle r = tileRect(i);
            text.Draw(FontId::BodyBold, TextFormat("%d", stacks), {r.x + r.width - 6.0f, r.y + r.height - 24.0f}, 20.0f,
                      WithAlpha(palette::kUiText, reveal(i)), Align::Right);
        }
    }
    if (count == 0) {
        text.Draw(FontId::Body, "No upgrades taken.", {panel.x + 36.0f, gridTop}, 24.0f,
                  WithAlpha(palette::kUiFaint, alpha));
    }
    if (showKiller) {
        const float y = panel.y + panel.height - 110.0f;
        text.Draw(FontId::Heading, "DESTROYED BY", {panel.x + 130.0f, y + 26.0f}, 16.0f,
                  WithAlpha(palette::kUiDim, alpha), Align::Left, 0.2f);
        text.Draw(FontId::Heading, EnemyDisplayName(killer), {panel.x + 130.0f, y + 52.0f}, 28.0f,
                  WithAlpha(GetEnemyInfo(killer).color, alpha), Align::Left, 0.08f);
    }
}

} // namespace gb
