#include "render/Graphics.h"

#include <algorithm>

namespace gb {

bool Graphics::Init(int width, int height) {
    if (!neon_.Init()) {
        TraceLog(LOG_ERROR, "GFX: neon renderer failed to initialise");
        return false;
    }
    if (!post_.Init(width, height)) {
        TraceLog(LOG_ERROR, "GFX: post-processing failed to initialise");
        return false;
    }
    text_.Init();
    Resize(width, height);
    return true;
}

void Graphics::Shutdown() {
    text_.Shutdown();
    post_.Shutdown();
    neon_.Shutdown();
}

void Graphics::Resize(int width, int height, float renderScale) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    renderScale_ = std::clamp(renderScale, 0.25f, 1.0f);
    post_.Resize(std::max(1, static_cast<int>(static_cast<float>(width) * renderScale_)),
                 std::max(1, static_cast<int>(static_cast<float>(height) * renderScale_)));
    uiScale_ = static_cast<float>(height) / kUiHeight;
    uiSize_ = {static_cast<float>(width) / uiScale_, kUiHeight};
}

Vector2 Graphics::ScreenToScene(Vector2 screen) const {
    const float sx = static_cast<float>(post_.Width()) / static_cast<float>(std::max(GetRenderWidth(), 1));
    const float sy = static_cast<float>(post_.Height()) / static_cast<float>(std::max(GetRenderHeight(), 1));
    return {screen.x * sx, screen.y * sy};
}

Vector2 Graphics::SceneToScreen(Vector2 scene) const {
    const float sx = static_cast<float>(std::max(GetRenderWidth(), 1)) / static_cast<float>(std::max(post_.Width(), 1));
    const float sy =
        static_cast<float>(std::max(GetRenderHeight(), 1)) / static_cast<float>(std::max(post_.Height(), 1));
    return {scene.x * sx, scene.y * sy};
}

void Graphics::BeginUi() const {
    Camera2D camera{};
    camera.zoom = uiScale_;
    BeginMode2D(camera);
}

void Graphics::EndUi() const { EndMode2D(); }

} // namespace gb
