#include "scenes/BootScene.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "scenes/TitleScene.h"
#include "view/Palette.h"

#include <array>
#include <string_view>

namespace gb {

namespace {

struct LogLine {
    std::string_view label;
    std::string_view status;  // shown in an aligned column once the label is typed
};

constexpr std::array<LogLine, 5> kLog{{
    {"> MAINFRAME INTEGRITY", "COMPROMISED"},
    {"> ROGUE PROCESS 0x7F3A", "ONLINE"},
    {"> LOADING COMBAT SUBROUTINES", "OK"},
    {"> SYNTHESISING AUDIO", "OK"},
    {"> EXECUTE GRIDBREAKER.EXE", ""},
}};
constexpr float kStatusColumn = 560.0f;
constexpr float kCharsPerSecond = 95.0f;
constexpr float kLinePause = 0.12f;
constexpr float kFlashAt = 2.75f;
constexpr float kDuration = 3.6f;

} // namespace

BootScene::BootScene(App& app) : Scene(app), backdrop_(palette::kUiAccent) {}

void BootScene::OnEnter() { HideCursor(); }

void BootScene::Finish() {
    if (finished_) {
        return;
    }
    finished_ = true;
    app_.ChangeScene(std::make_unique<TitleScene>(app_), 0.45f);
}

void BootScene::Update(float dt) {
    time_ += dt;
    backdrop_.Update(dt);
    if (!flashed_ && time_ >= kFlashAt) {
        flashed_ = true;
        app_.Audio().Play(SfxId::Nova, 0.55f, 1.4f);
        app_.Audio().Music().SetMood(MusicMood::Menu, 0);
    }
    const Input& in = app_.In();
    const bool skip = in.Pressed(Action::Confirm) || in.Pressed(Action::Back) || in.MouseClicked() ||
                      in.Pressed(Action::Fire) || in.Pressed(Action::Pause);
    if (time_ >= kDuration || (skip && time_ > 0.2f)) {
        if (!flashed_) {
            app_.Audio().Music().SetMood(MusicMood::Menu, 0);
        }
        Finish();
    }
}

void BootScene::DrawWorld() {
    if (time_ > kFlashAt - 0.3f) {
        backdrop_.Draw(app_);
    }
}

void BootScene::ConfigurePostFx(PostFxParams& params) const {
    const float sinceFlash = time_ - kFlashAt;
    if (sinceFlash >= 0.0f) {
        params.flashColor = palette::kUiAccent;
        params.flashAmount = std::max(0.0f, 0.5f - sinceFlash * 1.6f);
        params.aberration = std::max(0.0f, 0.02f - sinceFlash * 0.04f);
    }
}

void BootScene::DrawUi() {
    TextRenderer& text = app_.Gfx().Text();
    const Vector2 ui = app_.Gfx().UiSize();
    const float x = std::max(80.0f, ui.x * 0.5f - 520.0f);
    float y = ui.y * 0.5f - 150.0f;
    const float fade = time_ < kFlashAt ? 1.0f : Saturate(1.0f - (time_ - kFlashAt) * 3.0f);

    // Type the log out line by line; each status pops in once its label is done.
    float lineStart = 0.15f;
    for (const LogLine& line : kLog) {
        const float elapsed = time_ - lineStart;
        if (elapsed < 0.0f) {
            break;
        }
        const auto chars = std::min(line.label.size(), static_cast<std::size_t>(elapsed * kCharsPerSecond));
        const bool last = line.status.empty();
        const Color color = last ? palette::kUiAccent : palette::kUiGood;
        const std::string_view typed = line.label.substr(0, chars);
        text.Draw(FontId::BodyBold, typed, {x, y}, 34.0f, WithAlpha(color, fade));
        const float labelEnd = x + text.Measure(FontId::BodyBold, typed, 34.0f).x;
        if (chars < line.label.size() || (last && std::fmod(time_ * 3.0f, 1.0f) < 0.5f)) {
            DrawRectangleRec({labelEnd + 6.0f, y + 6.0f, 14.0f, 26.0f}, WithAlpha(color, fade));
            break;
        }
        if (!last) {
            for (float dot = labelEnd + 16.0f; dot < x + kStatusColumn - 14.0f; dot += 12.0f) {
                DrawRectangleRec({dot, y + 24.0f, 4.0f, 4.0f}, WithAlpha(palette::kUiFaint, fade));
            }
            const bool warning = line.status == "COMPROMISED";
            text.Draw(FontId::BodyBold, line.status, {x + kStatusColumn, y}, 34.0f,
                      WithAlpha(warning ? palette::kDanger : WHITE, fade));
        }
        y += 48.0f;
        lineStart += static_cast<float>(line.label.size()) / kCharsPerSecond + kLinePause;
    }

    if (time_ >= kFlashAt) {
        const float t = time_ - kFlashAt;
        const float scale = 1.0f + 0.25f * (1.0f - EaseOutCubic(Saturate(t / 0.5f)));
        text.DrawGlow(FontId::Display, "GRIDBREAKER", {ui.x * 0.5f, ui.y * 0.5f - 90.0f * scale}, 150.0f * scale,
                      WithAlpha(WHITE, Saturate(t * 4.0f)), palette::kUiAccent, Align::Center, 0.06f);
    }
    if (time_ > 0.6f && time_ < kFlashAt) {
        text.Draw(FontId::Body, "press any key to skip", {ui.x * 0.5f, ui.y - 70.0f}, 22.0f,
                  WithAlpha(palette::kUiFaint, 0.8f), Align::Center);
    }
}

} // namespace gb
