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

void Graphics::Resize(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    post_.Resize(width, height);
    uiScale_ = static_cast<float>(height) / kUiHeight;
    uiSize_ = {static_cast<float>(width) / uiScale_, kUiHeight};
}

void Graphics::BeginUi() const {
    Camera2D camera{};
    camera.zoom = uiScale_;
    BeginMode2D(camera);
}

void Graphics::EndUi() const { EndMode2D(); }

} // namespace gb
