#include "scenes/HangarScene.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "game/Profile.h"
#include "scenes/GameScene.h"
#include "scenes/TitleScene.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>

namespace gb {

namespace {

constexpr int kShipCount = static_cast<int>(ShipId::Count);
enum HangarItem { ItemDifficulty = kShipCount, ItemMode, ItemLaunch, ItemFirmware, ItemBack };

const char* ModeName(GameMode mode) {
    switch (mode) {
    case GameMode::Campaign: return "CAMPAIGN";
    case GameMode::Endless: return "ENDLESS";
    case GameMode::Daily: return "DAILY";
    case GameMode::Count: break;
    }
    return "";
}

const char* DifficultyBlurb(Difficulty d) {
    switch (d) {
    case Difficulty::Relaxed: return "+2 integrity, gentler enemies. Reduced score and credits.";
    case Difficulty::Standard: return "The intended experience.";
    case Difficulty::Overclocked: return "Tougher, faster, denser. +60% score, +50% credits.";
    case Difficulty::Count: break;
    }
    return "";
}

// Rough 0..1 ratings for the ship stat bars.
struct ShipRatings {
    float firepower, defense, mobility;
};

ShipRatings Ratings(ShipId id) {
    const PlayerStats s = ShipBaseStats(id);
    const float dps = s.damage * s.fireRate * static_cast<float>(s.projectiles) * (1.0f + 0.35f * s.pierce);
    return {std::clamp(dps / 260.0f, 0.15f, 1.0f), std::clamp(static_cast<float>(s.maxHp) / 7.0f, 0.15f, 1.0f),
            std::clamp((s.moveSpeed / 480.0f) * 0.6f + (static_cast<float>(s.dashCharges) / 2.0f) * 0.4f, 0.15f, 1.0f)};
}

} // namespace

HangarScene::HangarScene(App& app, bool openFirmware) : Scene(app), firmwareOpen_(openFirmware) {
    const Profile& profile = app.Progress();
    ship_ = previewShip_ = profile.LastShip();
    difficulty_ = profile.LastDifficulty();
    RefreshMenu();
    menu_.SetSelected(ItemLaunch);
    std::vector<ui::Menu::Item> firmware;
    for (int i = 0; i < static_cast<int>(FirmwareId::Count); ++i) {
        firmware.push_back({GetFirmwareInfo(static_cast<FirmwareId>(i)).name});
    }
    firmware.push_back({"BACK"});
    firmwareMenu_.SetItems(std::move(firmware));
    backdrop_.SetAccent(GetShipInfo(ship_).color);
}

void HangarScene::OnEnter() {
    app_.Audio().Music().SetMood(MusicMood::Menu, 0);
    ShowCursor();
}

void HangarScene::RefreshMenu() {
    const Profile& profile = app_.Progress();
    std::vector<ui::Menu::Item> items;
    for (const ShipInfo& info : AllShips()) {
        ui::Menu::Item item{info.name};
        if (!profile.ShipUnlocked(info.id)) {
            item.value = TextFormat("LOCKED  %d CR", info.cost);
        } else if (info.id == ship_) {
            item.value = "SELECTED";
        }
        items.push_back(item);
    }
    items.push_back({"DIFFICULTY", DifficultyName(difficulty_)});
    items.push_back({"MODE", ModeName(mode_)});
    items.push_back({"LAUNCH"});
    items.push_back({"FIRMWARE UPGRADES"});
    items.push_back({"BACK"});
    const int selected = menu_.Selected();
    menu_.SetItems(std::move(items));
    menu_.SetSelected(selected);

    for (int i = 0; i < static_cast<int>(FirmwareId::Count); ++i) {
        const auto id = static_cast<FirmwareId>(i);
        const int cost = profile.FirmwareNextCost(id);
        firmwareMenu_.SetValue(static_cast<std::size_t>(i), cost < 0 ? "MAX" : TextFormat("%d CR", cost));
    }
}

void HangarScene::Launch() {
    Profile& profile = app_.Progress();
    profile.SetLastSelection(ship_, difficulty_);
    profile.Save();
    RunConfig config;
    config.ship = ship_;
    config.difficulty = difficulty_;
    config.mode = mode_;
    config.seed = app_.Options().seed != 0 ? app_.Options().seed
                                            : static_cast<std::uint64_t>(GetRandomValue(1, 0x7fffffff)) ^
                                                  (static_cast<std::uint64_t>(GetTime() * 1000.0) << 20);
    config.meta = profile.Bonuses();
    app_.Audio().Play(SfxId::UiConfirm, 1.0f);
    app_.ChangeScene(std::make_unique<GameScene>(app_, config), 0.5f);
}

void HangarScene::UpdateFirmware() {
    Profile& profile = app_.Progress();
    const int activated = firmwareMenu_.Update(app_);
    if (activated == static_cast<int>(FirmwareId::Count) || app_.In().Pressed(Action::Back)) {
        firmwareOpen_ = false;
        app_.Audio().Play(SfxId::UiBack, 0.7f);
        app_.In().ConsumeAll();
        return;
    }
    if (activated >= 0) {
        if (profile.TryBuyFirmware(static_cast<FirmwareId>(activated))) {
            app_.Audio().Play(SfxId::PickupCache, 0.8f);
            purchaseFlash_ = 1.0f;
            RefreshMenu();
        } else {
            app_.Audio().Play(SfxId::UiDeny, 0.8f);
            denyFlash_ = 1.0f;
        }
    }
}

void HangarScene::Update(float dt) {
    time_ += dt;
    denyFlash_ = std::max(0.0f, denyFlash_ - dt * 3.0f);
    purchaseFlash_ = std::max(0.0f, purchaseFlash_ - dt * 2.0f);
    backdrop_.Update(dt);

    if (firmwareOpen_) {
        UpdateFirmware();
        return;
    }

    Profile& profile = app_.Progress();
    const int activated = menu_.Update(app_);
    const int selected = menu_.Selected();
    if (selected < kShipCount) {
        previewShip_ = static_cast<ShipId>(selected);
        if (profile.ShipUnlocked(previewShip_) && previewShip_ != ship_) {
            ship_ = previewShip_;
            RefreshMenu();
        }
        backdrop_.SetAccent(GetShipInfo(previewShip_).color);
    }

    // Left/right (or confirm/click) cycles the value rows.
    int adjust = menu_.Adjustment();
    if (activated == ItemDifficulty || activated == ItemMode) {
        adjust = 1;
    }
    if (adjust != 0 && selected == ItemDifficulty) {
        const int count = static_cast<int>(Difficulty::Count);
        difficulty_ = static_cast<Difficulty>((static_cast<int>(difficulty_) + adjust + count) % count);
        app_.Audio().Play(SfxId::UiMove, 0.7f);
        RefreshMenu();
    } else if (adjust != 0 && selected == ItemMode) {
        mode_ = mode_ == GameMode::Campaign ? GameMode::Endless : GameMode::Campaign;
        app_.Audio().Play(SfxId::UiMove, 0.7f);
        RefreshMenu();
    }

    if (activated >= 0 && activated < kShipCount) {
        const auto id = static_cast<ShipId>(activated);
        if (!profile.ShipUnlocked(id)) {
            if (profile.TryUnlockShip(id)) {
                ship_ = id;
                app_.Audio().Play(SfxId::PickupCache, 0.9f);
                purchaseFlash_ = 1.0f;
            } else {
                app_.Audio().Play(SfxId::UiDeny, 0.8f);
                denyFlash_ = 1.0f;
            }
        } else {
            ship_ = id;
            menu_.SetSelected(ItemLaunch);
            app_.Audio().Play(SfxId::UiConfirm, 0.7f);
        }
        RefreshMenu();
    } else if (activated == ItemLaunch) {
        Launch();
    } else if (activated == ItemFirmware) {
        firmwareOpen_ = true;
        firmwareMenu_.SetSelected(0);
        app_.Audio().Play(SfxId::UiConfirm, 0.7f);
        app_.In().ConsumeAll();
    } else if (activated == ItemBack || app_.In().Pressed(Action::Back)) {
        app_.Audio().Play(SfxId::UiBack, 0.7f);
        app_.ChangeScene(std::make_unique<TitleScene>(app_));
    }
}

void HangarScene::DrawWorld() { backdrop_.Draw(app_); }

void HangarScene::DrawShipDetails(Rectangle area) {
    const ShipInfo& info = GetShipInfo(previewShip_);
    const Profile& profile = app_.Progress();
    const bool unlocked = profile.ShipUnlocked(previewShip_);
    TextRenderer& text = app_.Gfx().Text();
    NeonRenderer& neon = app_.Gfx().Neon();
    ui::Panel(app_, area, info.color);

    const Vector2 center{area.x + area.width * 0.5f, area.y + 230.0f};
    neon.Begin();
    neon.Circle(center, 150.0f, 1.5f, WithAlpha(info.color, 0.3f), 64);
    neon.Circle(center, 165.0f + 6.0f * std::sin(time_ * 2.0f), 1.0f, WithAlpha(info.color, 0.15f), 64);
    const float angle = -kPi * 0.5f + std::sin(time_ * 0.8f) * 0.25f;
    shapes::DrawShip(neon, previewShip_, center, angle, 90.0f, unlocked ? info.color : palette::kUiFaint,
                     unlocked ? 1.0f : 0.6f, 5.0f);
    neon.Glow(center, 200.0f, WithAlpha(info.color, 0.1f));
    neon.End();

    float y = area.y + 420.0f;
    text.DrawGlow(FontId::Display, info.name, {center.x, y}, 56.0f, WHITE, info.color, Align::Center, 0.1f);
    y += 70.0f;
    text.Draw(FontId::Heading, info.role, {center.x, y}, 22.0f, info.color, Align::Center, 0.2f);
    y += 46.0f;
    y += text.DrawWrapped(FontId::Body, info.description, {area.x + 50.0f, y}, area.width - 100.0f, 27.0f,
                          palette::kUiText, 1.15f, Align::Center);
    y += 26.0f;

    const ShipRatings ratings = Ratings(previewShip_);
    const std::array<std::pair<const char*, float>, 3> bars{
        {{"FIREPOWER", ratings.firepower}, {"DEFENSE", ratings.defense}, {"MOBILITY", ratings.mobility}}};
    for (const auto& [label, value] : bars) {
        text.Draw(FontId::BodyBold, label, {area.x + 60.0f, y}, 22.0f, palette::kUiDim);
        const float x0 = area.x + 220.0f;
        const float width = area.width - 280.0f;
        DrawRectangleRec({x0, y + 8.0f, width, 8.0f}, Color{20, 28, 50, 255});
        DrawRectangleRec({x0, y + 8.0f, width * value, 8.0f}, info.color);
        y += 34.0f;
    }
    y += 10.0f;
    const PlayerStats stats = ShipBaseStats(previewShip_);
    text.Draw(FontId::BodyBold, TextFormat("INTEGRITY %d    DASH CHARGES %d", stats.maxHp, stats.dashCharges),
              {center.x, y}, 22.0f, palette::kUiDim, Align::Center);
    y += 40.0f;
    if (!unlocked) {
        const bool affordable = profile.Credits() >= info.cost;
        const Color c = LerpColor(affordable ? palette::kCache : palette::kDanger, WHITE, denyFlash_ * 0.5f);
        text.Draw(FontId::Heading, TextFormat("UNLOCK FOR %d CREDITS", info.cost), {center.x, y}, 26.0f, c,
                  Align::Center, 0.08f);
    } else if (const long long best = profile.BestScoreForShip(previewShip_); best > 0) {
        text.Draw(FontId::BodyBold, TextFormat("BEST SCORE  %lld", best), {center.x, y}, 24.0f, palette::kUiWarn,
                  Align::Center);
    }
}

void HangarScene::DrawFirmware() {
    const Vector2 ui = app_.Gfx().UiSize();
    const Profile& profile = app_.Progress();
    TextRenderer& text = app_.Gfx().Text();
    NeonRenderer& neon = app_.Gfx().Neon();
    ui::Dim(app_, 0.6f);
    const Rectangle panel{ui.x * 0.5f - 560.0f, ui.y * 0.5f - 380.0f, 1120.0f, 760.0f};
    ui::Panel(app_, panel, palette::kCache);
    text.Draw(FontId::Heading, "FIRMWARE UPGRADES", {panel.x + 40.0f, panel.y + 30.0f}, 36.0f, WHITE, Align::Left, 0.12f);
    text.Draw(FontId::BodyBold, "Permanent upgrades that apply to every run.", {panel.x + 40.0f, panel.y + 80.0f}, 24.0f,
              palette::kUiDim);
    text.Draw(FontId::Heading, TextFormat("%d CR", profile.Credits()), {panel.x + panel.width - 40.0f, panel.y + 34.0f},
              34.0f, LerpColor(palette::kCache, WHITE, purchaseFlash_), Align::Right);

    const float listX = panel.x + 30.0f;
    const float listY = panel.y + 140.0f;
    const float itemHeight = 70.0f;
    firmwareMenu_.Draw(app_, {listX, listY}, 640.0f, itemHeight, palette::kCache);

    // Rank pips beside each entry.
    neon.Begin();
    for (int i = 0; i < static_cast<int>(FirmwareId::Count); ++i) {
        const auto id = static_cast<FirmwareId>(i);
        const FirmwareInfo& info = GetFirmwareInfo(id);
        const int rank = profile.FirmwareRank(id);
        for (int r = 0; r < info.maxRank; ++r) {
            const Vector2 a{listX + 690.0f + static_cast<float>(r) * 34.0f, listY + static_cast<float>(i) * itemHeight + 30.0f};
            neon.Line(a, a + Vector2{24.0f, 0.0f}, 5.0f, r < rank ? palette::kCache : WithAlpha(palette::kUiFaint, 0.7f));
        }
    }
    neon.End();
    const int selected = firmwareMenu_.Selected();
    if (selected < static_cast<int>(FirmwareId::Count)) {
        const FirmwareInfo& info = GetFirmwareInfo(static_cast<FirmwareId>(selected));
        text.Draw(FontId::BodyBold, TextFormat("%s per rank", info.description),
                  {panel.x + panel.width * 0.5f, panel.y + panel.height - 60.0f}, 26.0f,
                  LerpColor(palette::kUiText, palette::kDanger, denyFlash_), Align::Center);
    }
}

void HangarScene::DrawUi() {
    const Vector2 ui = app_.Gfx().UiSize();
    TextRenderer& text = app_.Gfx().Text();
    text.DrawGlow(FontId::Display, "HANGAR", {90.0f, 60.0f}, 64.0f, WHITE, palette::kUiAccent, Align::Left, 0.1f);
    text.Draw(FontId::Heading, TextFormat("CREDITS  %d", app_.Progress().Credits()), {ui.x - 90.0f, 78.0f}, 30.0f,
              LerpColor(palette::kCache, WHITE, purchaseFlash_), Align::Right, 0.05f);

    menu_.Draw(app_, {90.0f, 190.0f}, 560.0f, 62.0f, palette::kUiAccent);
    const int selected = menu_.Selected();
    if (selected == ItemDifficulty) {
        text.DrawWrapped(FontId::Body, DifficultyBlurb(difficulty_), {110.0f, 190.0f + 62.0f * 11.0f + 10.0f}, 520.0f,
                         24.0f, palette::kUiDim);
    } else if (selected == ItemMode) {
        text.DrawWrapped(FontId::Body,
                         mode_ == GameMode::Campaign ? "Clear five sectors and destroy the Core."
                                                     : "No finish line. Sectors loop with rising threat.",
                         {110.0f, 190.0f + 62.0f * 11.0f + 10.0f}, 520.0f, 24.0f, palette::kUiDim);
    }

    const float detailWidth = std::min(720.0f, ui.x - 820.0f);
    DrawShipDetails({ui.x - detailWidth - 90.0f, 150.0f, detailWidth, 860.0f});
    ui::Prompt(app_, {90.0f, ui.y - 50.0f}, "ESC", "B", "BACK", Align::Left, 22.0f);

    if (firmwareOpen_) {
        DrawFirmware();
    }
}

} // namespace gb
