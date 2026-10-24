#pragma once

#include "core/MathUtil.h"

#include <raylib.h>

namespace gb {

// World camera: smooth follow, look-ahead, trauma-based screen shake and
// directional kicks. View height is expressed in world units so gameplay
// framing is identical at every resolution.
class CameraRig {
public:
    void SetViewHeight(float worldUnits) { viewHeight_ = worldUnits; }
    float ViewHeight() const { return viewHeight_; }

    void SetBounds(Rectangle arena, float margin) {
        arena_ = arena;
        margin_ = margin;
    }

    void SnapTo(Vector2 target);
    void Update(float dt, Vector2 target, Vector2 lookAhead, float screenWidth, float screenHeight);

    // Adds shake trauma in [0, 1]; perceived shake grows with trauma squared.
    void AddTrauma(float amount);
    void Kick(Vector2 direction, float distance);
    void SetShakeScale(float scale) { shakeScale_ = scale; }
    void SetZoomPunch(float amount) { zoomPunch_ = std::max(zoomPunch_, amount); }

    Camera2D Get(float screenWidth, float screenHeight) const;
    Rectangle VisibleWorld(float screenWidth, float screenHeight) const;
    Vector2 Position() const { return position_; }
    float Trauma() const { return trauma_; }

private:
    float Zoom(float screenHeight) const;

    Vector2 position_{};
    Vector2 shakeOffset_{};
    float shakeRotation_ = 0.0f;
    Vector2 kick_{};
    float trauma_ = 0.0f;
    float time_ = 0.0f;
    float shakeScale_ = 1.0f;
    float zoomPunch_ = 0.0f;
    float viewHeight_ = 1080.0f;
    Rectangle arena_{0.0f, 0.0f, 0.0f, 0.0f};
    float margin_ = 0.0f;
};

} // namespace gb
