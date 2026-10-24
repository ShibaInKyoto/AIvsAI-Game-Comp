#include "ui/UpgradeDraft.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "game/World.h"
#include "ui/Widgets.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>

namespace gb {

namespace {

constexpr float kCardWidth = 360.0f;
constexpr float kCardHeight = 480.0f;
constexpr float kCardGap = 44.0f;
constexpr float kInputDelay = 0.4f; // ignore input briefly so held buttons don't auto-pick
constexpr float kPickDuration = 0.32f;
constexpr float kCacheLuck = 2.5f;

} // namespace

void UpgradeDraft::Open(App& app, World& world, bool cache) {
    cache_ = cache;
    offers_ = world.RollOffers(3, {}, cache ? kCacheLuck : 0.0f);
    if (offers_.empty()) {
        offers_.push_back(UpgradeId::NanoRepair);
    }
    active_ = true;
    picked_ = -1;
    time_ = 0.0f;
    selected_ = std::min(1, static_cast<int>(offers_.size()) - 1);
    app.In().ConsumeAll();
    app.Audio().Play(cache ? SfxId::PickupCache : SfxId::CardFlip, 0.7f);
}

Rectangle UpgradeDraft::CardRect(const App& app, int index) const {
    const Vector2 ui = app.Gfx().UiSize();
    const int n = static_cast<int>(offers_.size());
    const float total = static_cast<float>(n) * kCardWidth + static_cast<float>(n - 1) * kCardGap;
    const float x = ui.x * 0.5f - total * 0.5f + static_cast<float>(index) * (kCardWidth + kCardGap);
    return {x, ui.y * 0.5f - kCardHeight * 0.5f + 30.0f, kCardWidth, kCardHeight};
}

void UpgradeDraft::Pick(App& app, World& world, int index) {
    picked_ = index;
    pickTime_ = 0.0f;
    const UpgradeId id = offers_[static_cast<std::size_t>(index)];
    world.ApplyUpgrade(id);
    if (GetUpgradeInfo(id).category == UpgradeCategory::Evolution) {
        app.Audio().Play(SfxId::SectorClear, 0.9f, 1.2f);
    }
    if (cache_) {
        world.ConsumeCache();
    } else {
        world.ConsumeLevelUp();
    }
    app.Audio().Play(SfxId::UiConfirm, 0.8f);
    app.Audio().Play(SfxId::LevelUp, 0.4f, 1.5f);
}

bool UpgradeDraft::Update(App& app, World& world, float dt) {
    if (!active_) {
        return false;
    }
    time_ += dt;
    rerollFlash_ = std::max(0.0f, rerollFlash_ - dt * 3.0f);

    if (picked_ >= 0) {
        pickTime_ += dt;
        if (pickTime_ >= kPickDuration) {
            active_ = false;
            return true;
        }
        return false;
    }
    if (time_ < kInputDelay) {
        return false;
    }

    Input& in = app.In();
    const int n = static_cast<int>(offers_.size());
    if (in.Repeated(Action::Left)) {
        selected_ = (selected_ + n - 1) % n;
        app.Audio().Play(SfxId::UiMove, 0.6f);
    }
    if (in.Repeated(Action::Right)) {
        selected_ = (selected_ + 1) % n;
        app.Audio().Play(SfxId::UiMove, 0.6f);
    }
    for (int i = 0; i < n; ++i) {
        const Rectangle r = CardRect(app, i);
        if (ui::MouseOver(app, r)) {
            if (in.MouseMoved() && selected_ != i) {
                selected_ = i;
                app.Audio().Play(SfxId::UiMove, 0.4f);
            }
            if (in.MouseClicked()) {
                Pick(app, world, i);
                return false;
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        if (IsKeyPressed(KEY_ONE + i)) {
            Pick(app, world, i);
            return false;
        }
    }
    if (in.Pressed(Action::Confirm)) {
        Pick(app, world, selected_);
        return false;
    }
    if (in.Pressed(Action::Alt)) {
        if (world.UseReroll()) {
            offers_ = world.RollOffers(3, offers_, cache_ ? kCacheLuck : 0.0f);
            if (offers_.empty()) {
                offers_ = world.RollOffers(3, {}, 0.0f);
            }
            selected_ = std::min(selected_, static_cast<int>(offers_.size()) - 1);
            time_ = kInputDelay;
            rerollFlash_ = 1.0f;
            app.Audio().Play(SfxId::Reroll, 0.8f);
        } else {
            app.Audio().Play(SfxId::UiDeny, 0.7f);
        }
    }
    return false;
}

void UpgradeDraft::Draw(App& app, const World& world) {
    if (!active_) {
        return;
    }
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const Vector2 ui = app.Gfx().UiSize();
    const float appear = EaseOutCubic(Saturate(time_ / 0.25f));
    const float closing = picked_ >= 0 ? Saturate(pickTime_ / kPickDuration) : 0.0f;
    ui::Dim(app, 0.62f * appear * (1.0f - closing));

    const Color titleColor = cache_ ? palette::kCache : palette::kUiGood;
    const float titleAlpha = appear * (1.0f - closing);
    text.DrawGlow(FontId::Display, cache_ ? "DATA CACHE" : "LEVEL UP", {ui.x * 0.5f, ui.y * 0.5f - 330.0f}, 64.0f,
                  WithAlpha(WHITE, titleAlpha), WithAlpha(titleColor, titleAlpha), Align::Center, 0.12f);
    text.Draw(FontId::BodyBold,
              cache_ ? "Rare upgrades are more likely."
                     : TextFormat("Level %d reached. Choose an upgrade.", world.Level()),
              {ui.x * 0.5f, ui.y * 0.5f - 252.0f}, 26.0f, WithAlpha(palette::kUiDim, titleAlpha), Align::Center);

    const int n = static_cast<int>(offers_.size());
    for (int i = 0; i < n; ++i) {
        const UpgradeId id = offers_[static_cast<std::size_t>(i)];
        const UpgradeInfo& info = GetUpgradeInfo(id);
        const Color rarity = RarityColor(info.rarity);
        const bool selected = i == selected_;
        const int owned = world.Inventory().Stacks(id);

        // Staggered entrance; chosen card flares while others fall away.
        const float entry = EaseOutBack(Saturate((time_ - 0.05f * static_cast<float>(i)) / 0.35f));
        Rectangle r = CardRect(app, i);
        r.y += (1.0f - entry) * 160.0f;
        float alpha = Saturate(entry);
        if (picked_ >= 0) {
            if (i == picked_) {
                r.y -= 30.0f * closing;
                alpha *= 1.0f - closing * closing;
            } else {
                r.y += 120.0f * closing;
                alpha *= 1.0f - closing;
            }
        }
        if (selected && picked_ < 0) {
            r.y -= 14.0f;
        }
        if (alpha <= 0.01f) {
            continue;
        }

        DrawRectangleRec(r, WithAlpha(palette::kUiPanel, 0.94f * alpha));
        DrawRectangleRec({r.x, r.y, r.width, 6.0f}, WithAlpha(rarity, alpha));
        if (rerollFlash_ > 0.0f) {
            DrawRectangleRec(r, WithAlpha(WHITE, 0.25f * rerollFlash_ * alpha));
        }
        neon.Begin();
        const std::array<Vector2, 4> corners{Vector2{r.x, r.y}, Vector2{r.x + r.width, r.y},
                                             Vector2{r.x + r.width, r.y + r.height}, Vector2{r.x, r.y + r.height}};
        neon.Polyline(corners, true, selected ? 3.0f : 1.5f, WithAlpha(selected ? rarity : palette::kUiBorder, alpha));
        if (selected) {
            neon.Glow({r.x + r.width * 0.5f, r.y + r.height * 0.5f}, r.width * 0.8f, WithAlpha(rarity, 0.08f * alpha));
        }
        const Vector2 iconCenter{r.x + r.width * 0.5f, r.y + 140.0f};
        neon.RegularPolygon(iconCenter, 6, 74.0f, kPi / 6.0f, 2.0f, WithAlpha(rarity, 0.5f * alpha));
        neon.Glow(iconCenter, 90.0f, WithAlpha(rarity, 0.12f * alpha));
        shapes::DrawUpgradeIcon(neon, id, iconCenter, 92.0f, WithAlpha(LerpColor(rarity, WHITE, 0.2f), alpha), time_);
        // Level pips for stackable upgrades.
        if (info.maxStacks > 1) {
            const float pipWidth = 18.0f;
            const float startX = r.x + r.width * 0.5f - static_cast<float>(info.maxStacks) * pipWidth * 0.5f;
            for (int s = 0; s < info.maxStacks; ++s) {
                const Vector2 a{startX + static_cast<float>(s) * pipWidth + 3.0f, r.y + 312.0f};
                Color c = WithAlpha(palette::kUiFaint, 0.7f * alpha);
                if (s < owned) {
                    c = WithAlpha(rarity, alpha);
                } else if (s == owned) {
                    c = WithAlpha(WHITE, (0.5f + 0.5f * std::sin(time_ * 8.0f)) * alpha);
                }
                neon.Line(a, a + Vector2{pipWidth - 6.0f, 0.0f}, 4.0f, c);
            }
        }
        neon.End();

        text.Draw(FontId::Heading, CategoryName(info.category), {r.x + 22.0f, r.y + 24.0f}, 16.0f,
                  WithAlpha(rarity, alpha), Align::Left, 0.2f);
        text.Draw(FontId::Heading, RarityName(info.rarity), {r.x + r.width - 22.0f, r.y + 24.0f}, 16.0f,
                  WithAlpha(rarity, alpha), Align::Right, 0.2f);
        text.Draw(FontId::Heading, info.name, {r.x + r.width * 0.5f, r.y + 240.0f}, 28.0f, WithAlpha(WHITE, alpha),
                  Align::Center, 0.05f);
        const char* level = owned == 0 ? "NEW" : TextFormat("LEVEL %d  >  %d", owned, owned + 1);
        if (info.maxStacks == 0) {
            level = id == UpgradeId::Overcharge ? TextFormat("STACK %d", owned + 1) : "INSTANT";
        }
        if (info.category == UpgradeCategory::Evolution) {
            level = "MODULE EVOLUTION";
        }
        text.Draw(FontId::BodyBold, level, {r.x + r.width * 0.5f, r.y + 278.0f}, 22.0f,
                  WithAlpha(owned == 0 ? palette::kUiGood : palette::kUiDim, alpha), Align::Center, 0.1f);
        text.DrawWrapped(FontId::Body, UpgradeLevelText(id, owned), {r.x + 26.0f, r.y + 336.0f}, r.width - 52.0f, 27.0f,
                         WithAlpha(palette::kUiText, alpha), 1.12f, Align::Center);
        if (const EvolutionRecipe* recipe = EvolutionForModule(id); recipe != nullptr) {
            // Teach the synergy: which catalyst evolves this module at level 5.
            const bool hasCatalyst = world.Inventory().Stacks(recipe->catalyst) > 0;
            const char* hint = TextFormat("EVOLVES WITH %s", GetUpgradeInfo(recipe->catalyst).name);
            text.Draw(FontId::BodyBold, hint, {r.x + r.width * 0.5f, r.y + r.height - 72.0f}, 19.0f,
                      WithAlpha(hasCatalyst ? palette::kUiGood : palette::kElite, alpha * 0.9f), Align::Center, 0.06f);
        }
        if (app.In().LastDevice() == InputDevice::KeyboardMouse) {
            text.Draw(FontId::Heading, TextFormat("%d", i + 1), {r.x + r.width * 0.5f, r.y + r.height - 38.0f}, 20.0f,
                      WithAlpha(palette::kUiFaint, alpha), Align::Center);
        }
    }

    if (picked_ < 0) {
        const float y = ui.y * 0.5f + kCardHeight * 0.5f + 70.0f;
        ui::Prompt(app, {ui.x * 0.5f - 30.0f, y}, "ENTER", "A", "SELECT", Align::Right);
        const int rerolls = world.Rerolls();
        if (rerolls > 0) {
            ui::Prompt(app, {ui.x * 0.5f + 30.0f, y}, "R", "Y", TextFormat("REROLL (%d)", rerolls), Align::Left);
        } else {
            app.Gfx().Text().Draw(FontId::BodyBold, "NO REROLLS", {ui.x * 0.5f + 30.0f, y}, 22.0f, palette::kUiFaint);
        }
    }
}

} // namespace gb
