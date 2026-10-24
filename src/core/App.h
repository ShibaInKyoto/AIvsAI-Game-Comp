#pragma once

#include "core/Input.h"
#include "core/Scene.h"
#include "core/Settings.h"
#include "platform/Rumble.h"
#include "render/Graphics.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace gb {

class AudioSystem;
class Profile;

// Command-line driven options, mostly for automated testing.
struct LaunchOptions {
    bool windowed = false;
    int width = 1600;
    int height = 900;
    bool noAudio = false;
    // Automated runs: the title screen hands control to the bot for this many
    // seconds of game time, then the app exits.
    float autotestSeconds = 0.0f;
    // Captures the screen to `screenshotPath` after `screenshotAt` seconds and exits.
    std::string screenshotPath;
    float screenshotAt = 3.0f;
    std::string startScene;  // optional: jump straight to a scene ("game", "title", ...)
    std::string dataDirectory;
    std::uint64_t seed = 0;
    int startSector = 0;     // debug: begin runs in this sector (0-based)
    bool startAtBoss = false;  // debug: skip straight to the sector boss
    bool perfOverlay = false;  // show the F3 performance overlay from the start
};

using SceneFactory = std::function<std::unique_ptr<Scene>(App&)>;

class App {
public:
    explicit App(LaunchOptions options);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int Run(const SceneFactory& firstScene);

    Graphics& Gfx() { return graphics_; }
    const Graphics& Gfx() const { return graphics_; }
    Input& In() { return input_; }
    const Input& In() const { return input_; }
    Settings& Prefs() { return settings_; }
    AudioSystem& Audio() { return *audio_; }
    Profile& Progress() { return *profile_; }
    Rumble& Haptics() { return rumble_; }
    const LaunchOptions& Options() const { return options_; }

    // Applies display-related settings (fullscreen, vsync, render scale) after they change.
    void ApplyDisplaySettings();
    // Scene render scale actually in use (resolves "automatic").
    float EffectiveRenderScale() const;

    // Queues a scene change; the current scene fades out first.
    void ChangeScene(std::unique_ptr<Scene> next, float fadeSeconds = 0.3f);
    void Quit() { quitRequested_ = true; }

    Vector2 UiMouse() const { return graphics_.ScreenToUi(input_.MousePosition()); }
    // F3 toggles the performance overlay.
    bool DebugOverlay() const { return debugOverlay_; }
    float RealTime() const { return realTime_; }
    float FrameTimeMs() const { return frameTimeMs_; }

private:
    bool Init();
    void Shutdown();
    void SetWindowIconFromAssets();
    void Frame();
    void UpdateTransition(float dt);
    void HandleScreenshot();

    LaunchOptions options_;
    Graphics graphics_;
    Input input_;
    Settings settings_;
    Rumble rumble_;
    std::unique_ptr<AudioSystem> audio_;
    std::unique_ptr<Profile> profile_;

    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Scene> pendingScene_;
    float fadeDuration_ = 0.3f;
    float fadeAlpha_ = 1.0f;
    enum class Fade { None, Out, In } fade_ = Fade::In;

    bool quitRequested_ = false;
    bool debugOverlay_ = false;
    bool windowedFullscreen_ = false;
    bool vsyncApplied_ = false;
    float realTime_ = 0.0f;
    float frameTimeMs_ = 0.0f;
    float appliedRenderScale_ = 1.0f;
    int lastWidth_ = 0;
    int lastHeight_ = 0;
};

} // namespace gb
