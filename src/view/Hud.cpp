#include "view/Hud.h"

#include "core/App.h"
#include "core/Format.h"
#include "game/EnemyCatalog.h"
#include "view/GameView.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>
#include <cstdio>

namespace gb {

namespace {

void Hexagon(NeonRenderer& neon, Vector2 c, float r, Color color, bool filled, float width = 2.0f) {
    std::array<Vector2, 6> pts{};
    for (int i = 0; i < 6; ++i) {
        pts[static_cast<std::size_t>(i)] = c + FromAngle(kPi / 6.0f + kTau * static_cast<float>(i) / 6.0f, r);
    }
    if (filled) {
        neon.FillConvex(c, pts, WithAlpha(color, 0.55f));
    }
    neon.Polyline(pts, true, width, color);
}

const char* BossTitle(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::BossWarden: return "PERIMETER GUARDIAN";
    case EnemyKind::BossHydra: return "THE DATA SERPENT";
    case EnemyKind::BossSingularity: return "COLLAPSED MEMORY WELL";
    case EnemyKind::BossArchitect: return "FIREWALL ENGINEER";
    case EnemyKind::BossCore: return "HEART OF THE MAINFRAME";
    default: return "";
    }
}

const char* KeyHint(const App& app, const char* keyboard, const char* pad) {
    return app.In().LastDevice() == InputDevice::Gamepad ? pad : keyboard;
}

} // namespace

void Hud::Update(const GameView& view, float dt) {
    time_ += dt;
    const World& world = view.GetWorld();
    const auto score = static_cast<float>(world.Record().score);
    displayedScore_ = score < displayedScore_ ? score : Damp(displayedScore_, score, 0.08f, dt);
    if (score - displayedScore_ < 1.0f) {
        displayedScore_ = score;
    }
    const float xp = static_cast<float>(world.Xp()) / static_cast<float>(std::max(world.XpToNext(), 1));
    if (world.Level() != lastLevel_) {
        lastLevel_ = world.Level();
        levelFlash_ = 1.0f;
        xpFill_ = 0.0f;
    }
    xpFill_ = Damp(xpFill_, xp, 0.06f, dt);
    levelFlash_ = std::max(0.0f, levelFlash_ - dt * 1.5f);

    const int hp = world.GetPlayer().hp;
    if (lastHp_ >= 0 && hp < lastHp_) {
        hpFlash_ = 1.0f;
    }
    lastHp_ = hp;
    hpFlash_ = std::max(0.0f, hpFlash_ - dt * 2.0f);

    const Director& director = world.GetDirector();
    if (const Enemy* boss = world.FindEnemy(director.BossId());
        boss != nullptr && director.Phase() == SectorPhase::Boss) {
        bossFill_ = Saturate(boss->hp / std::max(boss->maxHp, 1.0f));
    } else if (director.Phase() != SectorPhase::Boss) {
        bossFill_ = 1.0f;
    } else {
        bossFill_ = 0.0f;
    }
    bossLag_ = bossFill_ > bossLag_ ? bossFill_ : Damp(bossLag_, bossFill_, 0.35f, dt);
}

void Hud::Draw(App& app, const GameView& view) {
    DrawIndicators(app, view);
    DrawFloatingText(app, view);
    DrawVitals(app, view);
    DrawScore(app, view);
    DrawProgress(app, view);
    DrawExperience(app, view);
    DrawNova(app, view);
    DrawModules(app, view);
    DrawBanners(app, view);
}

void Hud::DrawVitals(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    const Player& p = world.GetPlayer();
    const PlayerStats& stats = world.Stats();
    NeonRenderer& neon = app.Gfx().Neon();
    TextRenderer& text = app.Gfx().Text();
    const Color shipColor = GetShipInfo(world.Config().ship).color;

    const Vector2 origin{48.0f, 52.0f};
    neon.Begin();
    for (int i = 0; i < stats.maxHp; ++i) {
        const Vector2 c = origin + Vector2{static_cast<float>(i) * 34.0f, 0.0f};
        const bool full = i < p.hp;
        Color color = full ? shipColor : WithAlpha(palette::kUiFaint, 0.8f);
        if (full && p.hp == 1) {
            color = LerpColor(palette::kDanger, WHITE, 0.5f + 0.5f * std::sin(time_ * 8.0f));
        }
        if (!full && hpFlash_ > 0.0f && i == p.hp) {
            color = LerpColor(color, palette::kDanger, hpFlash_);
        }
        Hexagon(neon, c, 13.0f, color, full);
    }
    if (stats.shieldCharges > 0) {
        const Vector2 c = origin + Vector2{static_cast<float>(stats.maxHp) * 34.0f + 10.0f, 0.0f};
        const bool up = p.shieldCharges > 0;
        neon.Circle(c, 13.0f, 2.2f, up ? palette::kShield : WithAlpha(palette::kUiFaint, 0.8f), 20);
        if (up) {
            neon.Glow(c, 14.0f, WithAlpha(palette::kShield, 0.6f));
        } else {
            const float t = 1.0f - Saturate(p.shieldTimer / std::max(stats.shieldRecharge, 0.1f));
            neon.Arc(c, 13.0f, -kPi * 0.5f, -kPi * 0.5f + kTau * t, 2.2f, palette::kShield, 20);
        }
    }
    // Dash charges as small bars.
    for (int i = 0; i < stats.dashCharges; ++i) {
        const Vector2 a = origin + Vector2{-12.0f + static_cast<float>(i) * 40.0f, 30.0f};
        const bool ready = i < p.dashCharges;
        float fill = ready ? 1.0f : 0.0f;
        if (!ready && i == p.dashCharges) {
            fill = Saturate(p.dashRecharge / std::max(stats.dashCooldown, 0.01f));
        }
        neon.Line(a, a + Vector2{32.0f, 0.0f}, 3.0f, WithAlpha(palette::kUiFaint, 0.6f));
        if (fill > 0.0f) {
            neon.Line(a, a + Vector2{32.0f * fill, 0.0f}, 3.0f, ready ? palette::kUiAccent : palette::kUiDim);
        }
    }
    neon.End();
    text.Draw(FontId::BodyBold, KeyHint(app, "DASH [SPACE]", "DASH [LT]"),
              origin + Vector2{-12.0f + static_cast<float>(stats.dashCharges) * 40.0f + 4.0f, 21.0f}, 18.0f,
              palette::kUiFaint);
}

void Hud::DrawScore(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    TextRenderer& text = app.Gfx().Text();
    const Vector2 ui = app.Gfx().UiSize();
    const float right = ui.x - 48.0f;
    text.Draw(FontId::Heading, FormatThousands(static_cast<long long>(displayedScore_)), {right, 30.0f}, 40.0f,
              palette::kUiText, Align::Right, 0.04f);

    const int multiplier = world.ComboMultiplier();
    const int combo = world.Combo();
    if (combo > 0) {
        const Color c = multiplier >= 5 ? palette::kUiWarn : palette::kUiAccent;
        text.Draw(FontId::Heading, TextFormat("x%d", multiplier), {right, 78.0f}, 30.0f, c, Align::Right);
        text.Draw(FontId::BodyBold, TextFormat("%d COMBO", combo), {right - 60.0f, 82.0f}, 22.0f, palette::kUiDim,
                  Align::Right);
        const float t = Saturate(world.ComboTimer() / 3.0f);
        NeonRenderer& neon = app.Gfx().Neon();
        neon.Begin();
        neon.Line({right - 180.0f, 118.0f}, {right, 118.0f}, 2.0f, WithAlpha(palette::kUiFaint, 0.5f));
        neon.Line({right - 180.0f * t, 118.0f}, {right, 118.0f}, 2.5f, c);
        neon.End();
    }
}

void Hud::DrawProgress(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    const Director& director = world.GetDirector();
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const Vector2 ui = app.Gfx().UiSize();
    const float cx = ui.x * 0.5f;
    const Color accent = view.AccentColor();

    if (director.Phase() == SectorPhase::Boss) {
        const Enemy* boss = world.FindEnemy(director.BossId());
        const char* name = boss != nullptr ? GetEnemyInfo(boss->kind).name : "";
        const Color bossColor = boss != nullptr ? GetEnemyInfo(boss->kind).color : accent;
        const float width = std::min(900.0f, ui.x - 700.0f);
        const float x0 = cx - width * 0.5f;
        const float y = 64.0f;
        DrawRectangleRec({x0, y - 7.0f, width, 14.0f}, Color{10, 10, 20, 200});
        DrawRectangleRec({x0, y - 7.0f, width * bossLag_, 14.0f}, WithAlpha(WHITE, 0.55f));
        DrawRectangleRec({x0, y - 7.0f, width * bossFill_, 14.0f}, WithAlpha(bossColor, 0.95f));
        neon.Begin();
        neon.Polyline(std::array<Vector2, 4>{Vector2{x0 - 4.0f, y - 11.0f}, Vector2{x0 + width + 4.0f, y - 11.0f},
                                             Vector2{x0 + width + 4.0f, y + 11.0f}, Vector2{x0 - 4.0f, y + 11.0f}},
                      true, 1.5f, WithAlpha(bossColor, 0.8f));
        for (float mark : {0.33f, 0.66f}) {
            neon.Line({x0 + width * mark, y - 13.0f}, {x0 + width * mark, y + 13.0f}, 1.5f, WithAlpha(WHITE, 0.6f));
        }
        neon.End();
        text.Draw(FontId::Heading, name, {cx, 20.0f}, 26.0f, bossColor, Align::Center, 0.2f);
        return;
    }

    const SectorInfo& info = director.Info();
    const int sectorNumber = director.Sector() + 1;
    const char* title = TextFormat("SECTOR %02d  //  %s", sectorNumber, info.subtitle);
    text.Draw(FontId::Heading, title, {cx, 22.0f}, 20.0f, WithAlpha(accent, 0.9f), Align::Center, 0.15f);
    const float width = 420.0f;
    const float x0 = cx - width * 0.5f;
    const float y = 58.0f;
    const float progress = director.WaveProgress();
    neon.Begin();
    neon.Line({x0, y}, {x0 + width, y}, 2.0f, WithAlpha(palette::kUiFaint, 0.5f));
    neon.Line({x0, y}, {x0 + width * progress, y}, 3.0f, accent);
    neon.RegularPolygon({x0 + width, y}, 4, 8.0f, kPi * 0.25f, 2.0f, WithAlpha(palette::kDanger, 0.9f));
    neon.End();
    if (director.Phase() == SectorPhase::Waves) {
        const int remaining = static_cast<int>(std::ceil(info.duration - director.WaveTime()));
        text.Draw(FontId::BodyBold, TextFormat("%d:%02d", remaining / 60, remaining % 60), {cx, 66.0f}, 22.0f,
                  palette::kUiDim, Align::Center);
    }
}

void Hud::DrawExperience(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    const Vector2 ui = app.Gfx().UiSize();
    const float y = ui.y - 14.0f;
    DrawRectangleRec({0.0f, y - 5.0f, ui.x, 10.0f}, Color{6, 10, 20, 200});
    const Color c = LerpColor(palette::kXpSmall, WHITE, levelFlash_);
    DrawRectangleRec({0.0f, y - 5.0f, ui.x * xpFill_, 10.0f}, WithAlpha(c, 0.9f));
    NeonRenderer& neon = app.Gfx().Neon();
    neon.Begin();
    neon.Line({0.0f, y - 6.0f}, {ui.x * xpFill_, y - 6.0f}, 2.0f, WithAlpha(c, 0.8f));
    neon.End();
    app.Gfx().Text().Draw(FontId::Heading, TextFormat("LV %d", world.Level()), {24.0f, y - 42.0f}, 28.0f,
                          LerpColor(palette::kXpSmall, WHITE, 0.3f + 0.7f * levelFlash_));
}

void Hud::DrawNova(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    const Player& p = world.GetPlayer();
    const Vector2 ui = app.Gfx().UiSize();
    const Vector2 c{ui.x - 84.0f, ui.y - 96.0f};
    const bool ready = p.novaCharge >= 1.0f;
    const Color color = ready ? Color{190, 240, 255, 255} : palette::kUiDim;
    NeonRenderer& neon = app.Gfx().Neon();
    neon.Begin();
    neon.Circle(c, 38.0f, 2.0f, WithAlpha(palette::kUiFaint, 0.6f), 40);
    neon.Arc(c, 38.0f, -kPi * 0.5f, -kPi * 0.5f + kTau * p.novaCharge, 4.0f, color, 40);
    if (ready) {
        const float pulse = 0.5f + 0.5f * std::sin(time_ * 6.0f);
        neon.Glow(c, 50.0f + 10.0f * pulse, WithAlpha(color, 0.35f));
        neon.Circle(c, 44.0f + 6.0f * pulse, 1.5f, WithAlpha(color, 0.5f), 40);
    }
    neon.RegularPolygon(c, 8, 16.0f, time_ * 0.5f, 2.0f, color);
    neon.End();
    TextRenderer& text = app.Gfx().Text();
    text.Draw(FontId::Heading, "NOVA", {c.x, c.y + 46.0f}, 16.0f, color, Align::Center, 0.2f);
    if (ready) {
        text.Draw(FontId::BodyBold, KeyHint(app, "[Q]", "[RB]"), {c.x - 52.0f, c.y - 12.0f}, 24.0f, color,
                  Align::Right);
    }
}

void Hud::DrawModules(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    NeonRenderer& neon = app.Gfx().Neon();
    TextRenderer& text = app.Gfx().Text();
    const Vector2 ui = app.Gfx().UiSize();
    float x = 44.0f;
    const float y = ui.y - 90.0f;
    struct Slot {
        UpgradeId id;
        int level;
    };
    std::array<Slot, 8> slots{};
    int count = 0;
    for (const UpgradeInfo& info : AllUpgrades()) {
        const int level = world.Inventory().Stacks(info.id);
        if (info.category == UpgradeCategory::Module && level > 0 && count < 8) {
            slots[static_cast<std::size_t>(count++)] = {info.id, level};
        }
    }
    if (count == 0) {
        return;
    }
    neon.Begin();
    for (int i = 0; i < count; ++i) {
        const Vector2 c{x + static_cast<float>(i) * 64.0f, y};
        neon.RegularPolygon(c, 6, 27.0f, kPi / 6.0f, 1.5f, WithAlpha(palette::kUiBorder, 0.9f));
        shapes::DrawUpgradeIcon(neon, slots[static_cast<std::size_t>(i)].id, c, 30.0f, palette::kUiGood, time_);
        for (int pip = 0; pip < 5; ++pip) {
            const Vector2 pp{c.x - 16.0f + static_cast<float>(pip) * 8.0f, c.y + 34.0f};
            const bool lit = pip < slots[static_cast<std::size_t>(i)].level;
            neon.Line(pp, pp + Vector2{5.0f, 0.0f}, 2.5f, lit ? palette::kUiGood : WithAlpha(palette::kUiFaint, 0.6f));
        }
    }
    neon.End();
    (void)text;
}

void Hud::DrawBanners(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    const Director& director = world.GetDirector();
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const Vector2 ui = app.Gfx().UiSize();
    const float cx = ui.x * 0.5f;
    const float cy = ui.y * 0.36f;
    const float t = director.PhaseTime();

    auto fade = [](float time, float in, float hold, float out) {
        if (time < in) return time / in;
        if (time < in + hold) return 1.0f;
        return Saturate(1.0f - (time - in - hold) / out);
    };

    switch (director.Phase()) {
    case SectorPhase::Intro: {
        const float a = fade(t, 0.3f, 2.0f, 0.6f);
        const float slide = (1.0f - EaseOutCubic(Saturate(t / 0.5f))) * 60.0f;
        const SectorInfo& info = director.Info();
        text.Draw(FontId::Heading, TextFormat("SECTOR %02d", director.Sector() + 1), {cx, cy - 60.0f - slide}, 34.0f,
                  WithAlpha(view.AccentColor(), a), Align::Center, 0.4f);
        text.DrawGlow(FontId::Display, info.subtitle, {cx, cy - 18.0f}, 86.0f, WithAlpha(WHITE, a),
                      WithAlpha(view.AccentColor(), a), Align::Center, 0.08f);
        if (director.Loop() > 0) {
            text.Draw(FontId::BodyBold, TextFormat("ENDLESS LOOP %d", director.Loop()), {cx, cy + 82.0f}, 28.0f,
                      WithAlpha(palette::kUiWarn, a), Align::Center, 0.1f);
        }
        break;
    }
    case SectorPhase::Purge:
    case SectorPhase::BossWarning: {
        if (director.Phase() == SectorPhase::Purge) {
            break;
        }
        const float blink = std::fmod(t * 2.5f, 1.0f) < 0.6f ? 1.0f : 0.35f;
        const float a = fade(t, 0.2f, 2.6f, 0.4f) * blink;
        DrawRectangleRec({0.0f, cy - 70.0f, ui.x, 130.0f}, WithAlpha(Color{60, 0, 10, 255}, 0.45f * a));
        neon.Begin();
        for (float x = std::fmod(t * 200.0f, 60.0f) - 60.0f; x < ui.x + 60.0f; x += 60.0f) {
            neon.Line({x, cy - 70.0f}, {x + 30.0f, cy - 70.0f}, 4.0f, WithAlpha(palette::kDanger, 0.7f * a));
            neon.Line({x + 30.0f, cy + 60.0f}, {x + 60.0f, cy + 60.0f}, 4.0f, WithAlpha(palette::kDanger, 0.7f * a));
        }
        neon.End();
        text.DrawGlow(FontId::Display, "WARNING", {cx, cy - 52.0f}, 80.0f, WithAlpha(WHITE, a),
                      WithAlpha(palette::kDanger, a), Align::Center, 0.25f);
        text.Draw(FontId::Heading, "HOSTILE CORE PROCESS DETECTED", {cx, cy + 26.0f}, 24.0f,
                  WithAlpha(palette::kDanger, a), Align::Center, 0.2f);
        break;
    }
    case SectorPhase::Boss: {
        // Title card as the guardian materialises.
        if (t > 3.6f) {
            break;
        }
        const Enemy* boss = world.FindEnemy(director.BossId());
        if (boss == nullptr) {
            break;
        }
        const Color c = GetEnemyInfo(boss->kind).color;
        const float a = fade(t, 0.25f, 2.6f, 0.7f);
        const float slide = (1.0f - EaseOutCubic(Saturate(t / 0.6f))) * 80.0f;
        text.Draw(FontId::Heading, BossTitle(boss->kind), {cx, cy - 70.0f}, 26.0f, WithAlpha(c, a), Align::Center, 0.35f);
        text.DrawGlow(FontId::Display, GetEnemyInfo(boss->kind).name, {cx - slide, cy - 34.0f}, 96.0f,
                      WithAlpha(WHITE, a), WithAlpha(c, a), Align::Center, 0.12f);
        break;
    }
    case SectorPhase::Cleared:
    case SectorPhase::Victory: {
        const float a = Saturate(t / 0.4f);
        text.DrawGlow(FontId::Display, director.Phase() == SectorPhase::Victory ? "SYSTEM LIBERATED" : "SECTOR CLEARED",
                      {cx, cy - 30.0f}, 70.0f, WithAlpha(WHITE, a), WithAlpha(palette::kUiGood, a), Align::Center,
                      0.08f);
        break;
    }
    default: break;
    }

    if (!world.PlayerAlive()) {
        text.DrawGlow(FontId::Display, "SIGNAL LOST", {cx, cy - 30.0f}, 80.0f, WHITE, palette::kDanger, Align::Center,
                      0.1f);
    }
}

void Hud::DrawIndicators(App& app, const GameView& view) const {
    const World& world = view.GetWorld();
    const Vector2 ui = app.Gfx().UiSize();
    const float margin = 46.0f;
    NeonRenderer& neon = app.Gfx().Neon();
    neon.Begin();
    for (const Enemy& e : world.Enemies()) {
        if (!e.Active() || (!IsBoss(e.kind) && !e.elite)) {
            continue;
        }
        const Vector2 p = view.WorldToUi(e.position);
        if (p.x > 0.0f && p.x < ui.x && p.y > 0.0f && p.y < ui.y) {
            continue;
        }
        const Vector2 center{ui.x * 0.5f, ui.y * 0.5f};
        const Vector2 dir = Normalized(p - center);
        const float sx = (ui.x * 0.5f - margin) / std::max(std::abs(dir.x), 1e-3f);
        const float sy = (ui.y * 0.5f - margin) / std::max(std::abs(dir.y), 1e-3f);
        const Vector2 edge = center + dir * std::min(sx, sy);
        const Color c = IsBoss(e.kind) ? GetEnemyInfo(e.kind).color : palette::kElite;
        const float a = AngleOf(dir);
        const std::array<Vector2, 3> tri{edge + FromAngle(a, 16.0f), edge + FromAngle(a + 2.5f, 12.0f),
                                         edge + FromAngle(a - 2.5f, 12.0f)};
        neon.Polyline(tri, true, 2.5f, c);
        neon.Glow(edge, 20.0f, WithAlpha(c, 0.4f));
    }
    neon.End();
}

void Hud::DrawFloatingText(App& app, const GameView& view) const {
    TextRenderer& text = app.Gfx().Text();
    for (const FloatingText& t : view.Effects().Texts()) {
        const Vector2 p = view.WorldToUi(t.position);
        const float life = t.life / t.maxLife;
        const float alpha = Saturate(life * 3.0f);
        const float pop = t.big ? 1.0f + 0.4f * EaseOutCubic(Saturate((1.0f - life) * 6.0f)) - 0.4f : 1.0f;
        const float size = t.size * pop;
        if (t.big) {
            text.DrawGlow(FontId::Heading, t.text.data(), p, size, WithAlpha(WHITE, alpha), WithAlpha(t.color, alpha),
                          Align::Center, 0.08f);
        } else {
            text.Draw(FontId::Heading, t.text.data(), {p.x + 1.5f, p.y + 1.5f}, size, WithAlpha(BLACK, alpha * 0.6f),
                      Align::Center);
            text.Draw(FontId::Heading, t.text.data(), p, size, WithAlpha(t.color, alpha), Align::Center);
        }
    }
}

} // namespace gb
