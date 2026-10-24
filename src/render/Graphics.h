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
    void Resize(int width, int height);

    NeonRenderer& Neon() { return neon_; }
    TextRenderer& Text() { return text_; }
    const TextRenderer& Text() const { return text_; }
    PostFx& Post() { return post_; }

    // UI is authored for a 1080-unit tall canvas; width follows the aspect ratio.
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
    Vector2 uiSize_{1920.0f, 1080.0f};
};

} // namespace gb
