#include "core/App.h"

#include "audio/AudioSystem.h"
#include "core/BuildInfo.h"
#include "core/Storage.h"
#include "game/Profile.h"

#include <rlgl.h>

#include <algorithm>
#include <cstdio>

namespace gb {

namespace {

constexpr float kMaxFrameTime = 1.0f / 20.0f;  // clamp hitches so physics never explodes
constexpr Color kClearColor{3, 4, 10, 255};

} // namespace

App::App(LaunchOptions options) : options_(std::move(options)) {}

App::~App() = default;

bool App::Init() {
    if (!options_.dataDirectory.empty()) {
        storage::SetDataDirectory(options_.dataDirectory);
    }
    settings_.Load();
    if (options_.windowed) {
        settings_.fullscreen = false;
    }

    SetTraceLogLevel(LOG_INFO);
    unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT;
    if (settings_.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    SetConfigFlags(flags);
    const std::string title = std::string(build::kGameName);
    InitWindow(options_.width, options_.height, title.c_str());
    if (!IsWindowReady()) {
        std::fprintf(stderr, "Failed to create a window with OpenGL 3.3 support.\n");
        return false;
    }
    vsyncApplied_ = settings_.vsync;
    SetExitKey(KEY_NULL);  // Escape is a gameplay key (pause), never quit outright
    SetWindowMinSize(960, 540);

    const int monitor = GetCurrentMonitor();
    if (!options_.windowed) {
        // Size the window sensibly for the monitor before going fullscreen so
        // leaving fullscreen lands on a reasonable windowed size.
        const int mw = GetMonitorWidth(monitor);
        const int mh = GetMonitorHeight(monitor);
        if (mw > 0 && mh > 0) {
            const int w = std::max(960, mw * 3 / 4);
            const int h = std::max(540, mh * 3 / 4);
            SetWindowSize(w, h);
            SetWindowPosition((mw - w) / 2, (mh - h) / 2);
        }
    }
    ApplyDisplaySettings();

    if (!graphics_.Init(GetRenderWidth(), GetRenderHeight())) {
        std::fprintf(stderr, "Failed to initialise graphics (OpenGL 3.3 required).\n");
        return false;
    }
    lastWidth_ = GetRenderWidth();
    lastHeight_ = GetRenderHeight();

    audio_ = std::make_unique<AudioSystem>();
    audio_->Init(!options_.noAudio);
    audio_->ApplyVolumes(settings_);

    profile_ = std::make_unique<Profile>();
    profile_->Load();
    return true;
}

void App::Shutdown() {
    scene_.reset();
    pendingScene_.reset();
    if (profile_) {
        profile_->Save();
    }
    if (audio_) {
        audio_->Shutdown();
    }
    graphics_.Shutdown();
    if (IsWindowReady()) {
        CloseWindow();
    }
}

void App::ApplyDisplaySettings() {
    if (settings_.fullscreen != windowedFullscreen_) {
        ToggleBorderlessWindowed();
        windowedFullscreen_ = settings_.fullscreen;
    }
    if (settings_.vsync != vsyncApplied_) {
        // raylib maps the vsync window flag onto the swap interval at runtime.
        if (settings_.vsync) {
            SetWindowState(FLAG_VSYNC_HINT);
        } else {
            ClearWindowState(FLAG_VSYNC_HINT);
        }
        vsyncApplied_ = settings_.vsync;
    }
    // Without vsync, cap at the monitor refresh rate to avoid burning a CPU core.
    const int refresh = GetMonitorRefreshRate(GetCurrentMonitor());
    SetTargetFPS(settings_.vsync ? 0 : std::max(60, refresh > 0 ? refresh : 144));
}

void App::ChangeScene(std::unique_ptr<Scene> next, float fadeSeconds) {
    pendingScene_ = std::move(next);
    fadeDuration_ = std::max(fadeSeconds, 0.01f);
    fade_ = Fade::Out;
}

void App::UpdateTransition(float dt) {
    switch (fade_) {
    case Fade::Out:
        fadeAlpha_ += dt / fadeDuration_;
        if (fadeAlpha_ >= 1.0f) {
            fadeAlpha_ = 1.0f;
            scene_ = std::move(pendingScene_);
            input_.ConsumeAll();
            if (scene_) {
                scene_->OnEnter();
            }
            fade_ = Fade::In;
        }
        break;
    case Fade::In:
        fadeAlpha_ -= dt / fadeDuration_;
        if (fadeAlpha_ <= 0.0f) {
            fadeAlpha_ = 0.0f;
            fade_ = Fade::None;
        }
        break;
    case Fade::None:
        break;
    }
}

int App::Run(const SceneFactory& firstScene) {
    if (!Init()) {
        Shutdown();
        return 1;
    }
    scene_ = firstScene(*this);
    if (scene_) {
        scene_->OnEnter();
    }
    while (!quitRequested_ && !WindowShouldClose()) {
        Frame();
    }
    Shutdown();
    return 0;
}

void App::Frame() {
    const bool automated = options_.autotestSeconds > 0.0f || !options_.screenshotPath.empty();
    // Automated runs use a fixed step so results do not depend on machine speed.
    const float dt = automated ? 1.0f / 60.0f : std::min(GetFrameTime(), kMaxFrameTime);
    realTime_ += dt;
    frameTimeMs_ = GetFrameTime() * 1000.0f;

    if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) {
        settings_.fullscreen = !settings_.fullscreen;
        ApplyDisplaySettings();
        settings_.Save();
    }

    const int width = GetRenderWidth();
    const int height = GetRenderHeight();
    if ((width != lastWidth_ || height != lastHeight_) && width > 0 && height > 0) {
        graphics_.Resize(width, height);
        lastWidth_ = width;
        lastHeight_ = height;
    }

    input_.Update(dt);
    audio_->Update(dt);
    UpdateTransition(dt);
    if (scene_ && !IsWindowMinimized()) {
        scene_->Update(dt);
    }

    PostFxParams post;
    post.time = realTime_;
    post.bloomEnabled = settings_.bloom;
    post.bloomStrength = settings_.bloomIntensity;
    post.grain = settings_.filmGrain ? 0.03f : 0.0f;
    post.scanlines = settings_.scanlines ? 0.12f : 0.0f;
    if (scene_) {
        scene_->ConfigurePostFx(post);
    }
    if (!settings_.chromaticAberration) {
        post.aberration = 0.0f;
    }
    if (settings_.reduceFlashing) {
        post.flashAmount *= 0.25f;
    }

    BeginDrawing();
    graphics_.Post().BeginScene(kClearColor);
    if (scene_) {
        scene_->DrawWorld();
    }
    graphics_.Post().EndScene();

    ClearBackground(BLACK);
    graphics_.Post().Composite(post);

    graphics_.BeginUi();
    if (scene_) {
        scene_->DrawUi();
    }
    const Vector2 ui = graphics_.UiSize();
    if (fadeAlpha_ > 0.0f) {
        DrawRectangleRec({0.0f, 0.0f, ui.x, ui.y}, WithAlpha(BLACK, fadeAlpha_));
    }
    if (settings_.showFps) {
        graphics_.Text().Draw(FontId::BodyBold, TextFormat("%d FPS", GetFPS()), {ui.x - 16.0f, 10.0f}, 22.0f,
                              WithAlpha(WHITE, 0.6f), Align::Right);
    }
    graphics_.EndUi();

    HandleScreenshot();
    EndDrawing();

    // Safety net for automated runs: never outlive the requested duration.
    if (options_.autotestSeconds > 0.0f && realTime_ > options_.autotestSeconds + 5.0f) {
        quitRequested_ = true;
    }
}

void App::HandleScreenshot() {
    if (options_.screenshotPath.empty() || realTime_ < options_.screenshotAt) {
        return;
    }
    Image image = LoadImageFromScreen();
    ExportImage(image, options_.screenshotPath.c_str());
    UnloadImage(image);
    quitRequested_ = true;
}

} // namespace gb
