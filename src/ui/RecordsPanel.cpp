#include "ui/RecordsPanel.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "core/Format.h"
#include "game/Challenges.h"
#include "game/EnemyCatalog.h"
#include "game/Profile.h"
#include "ui/Widgets.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>
#include <array>
#include <string>

namespace gb {

void RecordsPanel::Open(App& app) {
    active_ = true;
    app.In().ConsumeAll();
}

bool RecordsPanel::Update(App& app) {
    if (!active_) {
        return false;
    }
    const Input& in = app.In();
    if (in.Pressed(Action::TabPrev) || in.Pressed(Action::TabNext) || in.Pressed(Action::Left) ||
        in.Pressed(Action::Right)) {
        showRecentRuns_ = !showRecentRuns_;
        app.Audio().Play(SfxId::UiMove, 0.6f);
    }
    if (in.Pressed(Action::Back) || in.Pressed(Action::Confirm) || in.MouseClicked()) {
        active_ = false;
        app.Audio().Play(SfxId::UiBack, 0.7f);
        app.In().ConsumeAll();
        return true;
    }
    return false;
}

void RecordsPanel::Draw(App& app) {
    if (!active_) {
        return;
    }
    const Vector2 ui = app.Gfx().UiSize();
    const Profile& p = app.Progress();
    ui::Dim(app, 0.6f);
    const float width = std::min(1500.0f, ui.x - 80.0f);
    const Rectangle panel{ui.x * 0.5f - width * 0.5f, ui.y * 0.5f - 400.0f, width, 800.0f};
    ui::Panel(app, panel, palette::kUiAccent);
    TextRenderer& text = app.Gfx().Text();
    text.Draw(FontId::Heading, "RECORDS", {panel.x + 40.0f, panel.y + 30.0f}, 34.0f, WHITE, Align::Left, 0.15f);
    const int minutes = static_cast<int>(p.PlayTime() / 60.0f);
    struct Row {
        const char* label;
        std::string value;
    };
    const std::array<Row, 10> rows{{
        {"RUNS", std::to_string(p.Runs())},
        {"VICTORIES", std::to_string(p.Wins())},
        {"FURTHEST SECTOR", p.BestSector() > 0 ? std::to_string(p.BestSector()) : "-"},
        {"ENEMIES DESTROYED", FormatThousands(p.TotalKills())},
        {"GUARDIANS DESTROYED", std::to_string(p.BossKills())},
        {"TIME PLAYED", std::to_string(minutes / 60) + "h " + std::to_string(minutes % 60) + "m"},
        {"BEST - RELAXED", FormatThousands(p.BestScore(Difficulty::Relaxed))},
        {"BEST - STANDARD", FormatThousands(p.BestScore(Difficulty::Standard))},
        {"BEST - OVERCLOCKED", FormatThousands(p.BestScore(Difficulty::Overclocked))},
        {"BEST - BOSS RUSH", FormatThousands(p.BossRushBest())},
    }};
    const float leftWidth = width * 0.38f;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = panel.y + 100.0f + static_cast<float>(i) * 50.0f;
        text.Draw(FontId::BodyBold, rows[i].label, {panel.x + 40.0f, y}, 26.0f, palette::kUiDim);
        text.Draw(FontId::BodyBold, rows[i].value, {panel.x + leftWidth, y - 2.0f}, 30.0f, palette::kUiText,
                  Align::Right);
    }
    if (p.DailyDate() == TodayStamp() && p.DailyBest() > 0) {
        text.Draw(FontId::BodyBold, TextFormat("TODAY'S DAILY BEST: %s", FormatThousands(p.DailyBest()).c_str()),
                  {panel.x + 40.0f, panel.y + 620.0f}, 24.0f, palette::kUiWarn);
    }

    // Right column: challenges or the recent run history.
    const float cx = panel.x + leftWidth + 70.0f;
    const Rectangle column{cx, panel.y + 34.0f, panel.x + width - 40.0f - cx, panel.height - 80.0f};
    if (showRecentRuns_) {
        DrawRecentRuns(app, column);
    } else {
        DrawChallenges(app, column);
    }
    ui::Prompt(app, {panel.x + width - 40.0f, panel.y + panel.height - 40.0f}, "Q / E", "LB / RB",
               showRecentRuns_ ? "CHALLENGES" : "RECENT RUNS", Align::Right, 20.0f);
}

void RecordsPanel::DrawChallenges(App& app, Rectangle area) {
    const Profile& p = app.Progress();
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const float cx = area.x;
    const float columnWidth = area.width;
    text.Draw(FontId::Heading,
              TextFormat("CHALLENGES  %d / %d", p.CompletedChallengeCount(), static_cast<int>(AllChallenges().size())),
              {cx, area.y}, 26.0f, palette::kCache, Align::Left, 0.1f);
    const auto challenges = AllChallenges();
    const float rowHeight = 41.0f;
    const float top = area.y + 66.0f;
    neon.Begin();
    for (std::size_t i = 0; i < challenges.size(); ++i) {
        const bool done = p.ChallengeCompleted(challenges[i].id);
        const Vector2 c{cx + 12.0f, top + static_cast<float>(i) * rowHeight + 13.0f};
        neon.RegularPolygon(c, 4, 9.0f, kPi * 0.25f, 2.0f, done ? palette::kCache : WithAlpha(palette::kUiFaint, 0.8f));
        if (done) {
            neon.Glow(c, 10.0f, palette::kCache);
        }
    }
    neon.End();
    for (std::size_t i = 0; i < challenges.size(); ++i) {
        const ChallengeInfo& info = challenges[i];
        const bool done = p.ChallengeCompleted(info.id);
        const float y = top + static_cast<float>(i) * rowHeight;
        text.Draw(FontId::Heading, info.name, {cx + 36.0f, y}, 19.0f, done ? WHITE : palette::kUiDim, Align::Left,
                  0.08f);
        text.Draw(FontId::Body, info.description, {cx + 300.0f, y - 2.0f}, 23.0f,
                  done ? palette::kUiText : palette::kUiFaint);
        text.Draw(FontId::BodyBold, TextFormat("%d CR", info.reward), {cx + columnWidth, y - 2.0f}, 22.0f,
                  done ? palette::kUiFaint : palette::kCache, Align::Right);
    }
}

void RecordsPanel::DrawRecentRuns(App& app, Rectangle area) {
    const auto& runs = app.Progress().RecentRuns();
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    text.Draw(FontId::Heading, "RECENT RUNS", {area.x, area.y}, 26.0f, palette::kUiAccent, Align::Left, 0.1f);
    if (runs.empty()) {
        text.Draw(FontId::Body, "No runs yet. Your last ten runs will be listed here.", {area.x, area.y + 70.0f}, 24.0f,
                  palette::kUiFaint);
        return;
    }
    const float rowHeight = 64.0f;
    const float top = area.y + 66.0f;
    neon.Begin();
    for (std::size_t i = 0; i < runs.size(); ++i) {
        const ShipInfo& ship = GetShipInfo(runs[i].ship);
        const Vector2 c{area.x + 18.0f, top + static_cast<float>(i) * rowHeight + 22.0f};
        shapes::DrawShip(neon, runs[i].ship, c, -kPi * 0.5f, 15.0f, ship.color, 1.0f, 2.0f);
    }
    neon.End();
    for (std::size_t i = 0; i < runs.size(); ++i) {
        const RunRecord& run = runs[i];
        const ShipInfo& ship = GetShipInfo(run.ship);
        const float y = top + static_cast<float>(i) * rowHeight;
        text.Draw(FontId::Heading, ship.name, {area.x + 48.0f, y}, 19.0f, ship.color, Align::Left, 0.08f);
        text.Draw(FontId::Body, TextFormat("%s  //  %s", DifficultyName(run.difficulty), GameModeName(run.mode)),
                  {area.x + 48.0f, y + 24.0f}, 20.0f, palette::kUiFaint);
        const float mid = area.x + area.width * 0.42f;
        if (run.victory) {
            text.Draw(FontId::Heading, "LIBERATED", {mid, y + 8.0f}, 19.0f, palette::kUiGood, Align::Left, 0.1f);
        } else {
            text.Draw(FontId::Heading, TextFormat("SECTOR %d", run.sector), {mid, y}, 19.0f, palette::kUiDim,
                      Align::Left, 0.1f);
            if (run.killedBy != EnemyKind::Count) {
                text.Draw(FontId::Body, TextFormat("by %s", EnemyDisplayName(run.killedBy).c_str()), {mid, y + 24.0f},
                          20.0f, WithAlpha(palette::kDanger, 0.85f));
            }
        }
        const int seconds = static_cast<int>(run.time);
        text.Draw(FontId::BodyBold, FormatThousands(run.score), {area.x + area.width, y - 2.0f}, 26.0f,
                  palette::kUiText, Align::Right);
        text.Draw(FontId::Body, TextFormat("LV %d  //  %d:%02d", run.level, seconds / 60, seconds % 60),
                  {area.x + area.width, y + 26.0f}, 19.0f, palette::kUiFaint, Align::Right);
    }
}

} // namespace gb
