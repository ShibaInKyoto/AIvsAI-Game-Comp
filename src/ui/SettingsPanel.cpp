#include "ui/SettingsPanel.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "view/Palette.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace gb {

namespace {

std::string Percent(float v) { return std::to_string(static_cast<int>(std::round(v * 100.0f))) + "%"; }
std::string OnOff(bool v) { return v ? "ON" : "OFF"; }

void Step(float& value, int direction, float step, float lo, float hi) {
    value = std::clamp(std::round((value + step * static_cast<float>(direction)) / step) * step, lo, hi);
}

} // namespace

void SettingsPanel::Open(App& app) {
    active_ = true;
    std::vector<ui::Menu::Item> items(ItemCount);
    const char* labels[ItemCount] = {"MASTER VOLUME",   "MUSIC VOLUME",      "EFFECTS VOLUME",
                                     "FULLSCREEN",      "V-SYNC",            "RENDER SCALE",
                                     "SCREEN SHAKE",    "BLOOM",             "SCREEN DISTORTION",
                                     "FILM GRAIN",      "SCANLINES",         "REDUCE FLASHING",
                                     "DAMAGE NUMBERS",  "AUTO-FIRE",         "GAMEPAD AIM ASSIST",
                                     "GAMEPAD VIBRATION", "SHOW FPS",        "BACK"};
    for (int i = 0; i < ItemCount; ++i) {
        items[static_cast<std::size_t>(i)].label = labels[i];
        items[static_cast<std::size_t>(i)].adjustable = i != Back;
    }
    menu_.SetItems(std::move(items));
    menu_.SetSelected(0);
    Refresh(app);
    app.In().ConsumeAll();
}

void SettingsPanel::Refresh(App& app) {
    const Settings& s = app.Prefs();
    menu_.SetValue(MasterVolume, Percent(s.masterVolume));
    menu_.SetValue(MusicVolume, Percent(s.musicVolume));
    menu_.SetValue(SfxVolume, Percent(s.sfxVolume));
    menu_.SetValue(Fullscreen, OnOff(s.fullscreen));
    menu_.SetValue(VSync, OnOff(s.vsync));
    menu_.SetValue(RenderScale, s.renderScale == 0 ? "AUTO" : Percent(static_cast<float>(s.renderScale) / 100.0f));
    menu_.SetValue(ScreenShake, Percent(s.screenShake));
    menu_.SetValue(Bloom, s.bloom ? Percent(s.bloomIntensity) : "OFF");
    menu_.SetValue(Aberration, OnOff(s.chromaticAberration));
    menu_.SetValue(FilmGrain, OnOff(s.filmGrain));
    menu_.SetValue(Scanlines, OnOff(s.scanlines));
    menu_.SetValue(ReduceFlashing, OnOff(s.reduceFlashing));
    menu_.SetValue(DamageNumbers, OnOff(s.damageNumbers));
    menu_.SetValue(AutoFire, OnOff(s.autoFire));
    menu_.SetValue(AimAssist, OnOff(s.aimAssist));
    menu_.SetValue(Vibration, OnOff(s.vibration));
    menu_.SetValue(ShowFps, OnOff(s.showFps));
}

void SettingsPanel::Adjust(App& app, int item, int direction) {
    Settings& s = app.Prefs();
    switch (item) {
    case MasterVolume: Step(s.masterVolume, direction, 0.1f, 0.0f, 1.0f); break;
    case MusicVolume: Step(s.musicVolume, direction, 0.1f, 0.0f, 1.0f); break;
    case SfxVolume: Step(s.sfxVolume, direction, 0.1f, 0.0f, 1.0f); break;
    case Fullscreen:
        s.fullscreen = !s.fullscreen;
        app.ApplyDisplaySettings();
        break;
    case VSync:
        s.vsync = !s.vsync;
        app.ApplyDisplaySettings();
        break;
    case RenderScale: {
        static constexpr std::array<int, 4> kScales{0, 100, 75, 50};
        const auto it = std::find(kScales.begin(), kScales.end(), s.renderScale);
        const int index = it == kScales.end() ? 0 : static_cast<int>(it - kScales.begin());
        s.renderScale = kScales[static_cast<std::size_t>((index + direction + 4) % 4)];
        break;
    }
    case ScreenShake: Step(s.screenShake, direction, 0.25f, 0.0f, 1.5f); break;
    case Bloom:
        if (!s.bloom) {
            s.bloom = true;
            s.bloomIntensity = direction > 0 ? 0.5f : 1.5f;
        } else {
            Step(s.bloomIntensity, direction, 0.25f, 0.0f, 1.5f);
            if (s.bloomIntensity <= 0.01f) {
                s.bloom = false;
                s.bloomIntensity = 1.0f;
            }
        }
        break;
    case Aberration: s.chromaticAberration = !s.chromaticAberration; break;
    case FilmGrain: s.filmGrain = !s.filmGrain; break;
    case Scanlines: s.scanlines = !s.scanlines; break;
    case ReduceFlashing: s.reduceFlashing = !s.reduceFlashing; break;
    case DamageNumbers: s.damageNumbers = !s.damageNumbers; break;
    case AutoFire: s.autoFire = !s.autoFire; break;
    case AimAssist: s.aimAssist = !s.aimAssist; break;
    case Vibration: s.vibration = !s.vibration; break;
    case ShowFps: s.showFps = !s.showFps; break;
    default: return;
    }
    s.Sanitize();
    app.Audio().ApplyVolumes(s);
    app.Audio().Play(SfxId::UiMove, 0.8f, direction > 0 ? 1.15f : 0.9f);
    Refresh(app);
}

bool SettingsPanel::Update(App& app) {
    if (!active_) {
        return false;
    }
    const int activated = menu_.Update(app);
    if (menu_.Adjustment() != 0) {
        Adjust(app, menu_.Selected(), menu_.Adjustment());
    }
    if (activated >= 0 && activated != Back) {
        Adjust(app, activated, 1);
    }
    if (activated == Back || app.In().Pressed(Action::Back) || app.In().Pressed(Action::Pause)) {
        app.Prefs().Save();
        app.Audio().Play(SfxId::UiBack, 0.8f);
        app.In().ConsumeAll();
        active_ = false;
        return true;
    }
    return false;
}

void SettingsPanel::Draw(App& app) {
    if (!active_) {
        return;
    }
    const Vector2 ui = app.Gfx().UiSize();
    const float width = 760.0f;
    const float itemHeight = 46.0f;
    const float height = itemHeight * static_cast<float>(ItemCount) + 150.0f;
    const Rectangle panel{ui.x * 0.5f - width * 0.5f, ui.y * 0.5f - height * 0.5f, width, height};
    ui::Dim(app, 0.55f);
    ui::Panel(app, panel, palette::kUiAccent);
    app.Gfx().Text().Draw(FontId::Heading, "SETTINGS", {panel.x + 36.0f, panel.y + 28.0f}, 34.0f, WHITE, Align::Left,
                          0.15f);
    menu_.Draw(app, {panel.x + 24.0f, panel.y + 90.0f}, width - 48.0f, itemHeight, palette::kUiAccent);
    ui::Prompt(app, {panel.x + width - 36.0f, panel.y + 36.0f}, "ARROWS", "D-PAD", "ADJUST", Align::Right, 20.0f);
}

} // namespace gb
