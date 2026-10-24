#pragma once

#include "render/Neon.h"
#include "render/PostFx.h"
#include "render/Text.h"

#include <raylib.h>

namespace gb {

// Bundles the rendering subsystems and the virtual UI coordinate space.
class Graphics {
public:
    static constexpr float kUiHeight = 1080.0f;

    bool Init(int width, int height);
    void Shutdown();
    // Window resolution changed and/or the scene render scale (0..1].
    void Resize(int width, int height, float renderScale = 1.0f);

    NeonRenderer& Neon() { return neon_; }
    TextRenderer& Text() { return text_; }
    const TextRenderer& Text() const { return text_; }
    PostFx& Post() { return post_; }

    // UI is authored for a 1080-unit tall canvas; width follows the aspect ratio.
    // Resolution of the HDR scene buffer (window size times render scale).
    Vector2 SceneSize() const { return {static_cast<float>(post_.Width()), static_cast<float>(post_.Height())}; }
    // Converts between window pixels and scene-buffer pixels.
    Vector2 ScreenToScene(Vector2 screen) const;
    Vector2 SceneToScreen(Vector2 scene) const;

    float UiScale() const { return uiScale_; }
    Vector2 UiSize() const { return uiSize_; }
    Vector2 ScreenToUi(Vector2 screen) const { return {screen.x / uiScale_, screen.y / uiScale_}; }
    void BeginUi() const;
    void EndUi() const;

private:
    NeonRenderer neon_;
    TextRenderer text_;
    PostFx post_;
    float uiScale_ = 1.0f;
    float renderScale_ = 1.0f;
    Vector2 uiSize_{1920.0f, 1080.0f};
};

} // namespace gb
