#include "render/CameraRig.h"

#include <algorithm>
#include <cmath>

namespace gb {

namespace {

constexpr float kTraumaDecay = 1.6f;
constexpr float kMaxShakeOffset = 26.0f;
constexpr float kMaxShakeRotation = 0.035f;
constexpr float kFollowHalfLife = 0.08f;
constexpr float kKickHalfLife = 0.06f;

// Cheap smooth 1D noise built from incommensurate sines, in [-1, 1].
float SmoothNoise(float t, float seed) {
    return (std::sin(t * 1.0f + seed) * 0.5f + std::sin(t * 2.31f + seed * 1.7f) * 0.3f +
            std::sin(t * 4.77f + seed * 2.9f) * 0.2f);
}

} // namespace

void CameraRig::SnapTo(Vector2 target) {
    position_ = target;
    kick_ = {};
    trauma_ = 0.0f;
}

void CameraRig::AddTrauma(float amount) { trauma_ = std::min(1.0f, trauma_ + amount); }

void CameraRig::Kick(Vector2 direction, float distance) { kick_ += Normalized(direction) * distance; }

void CameraRig::Update(float dt, Vector2 target, Vector2 lookAhead, float screenWidth, float screenHeight) {
    time_ += dt;
    Vector2 desired = target + lookAhead;

    // Keep the view mostly inside the arena; when the arena is smaller than the
    // view on an axis, centre it on that axis instead.
    if (arena_.width > 0.0f) {
        const float zoom = Zoom(screenHeight);
        const float halfW = screenWidth / zoom * 0.5f;
        const float halfH = screenHeight / zoom * 0.5f;
        const float minX = arena_.x - margin_ + halfW;
        const float maxX = arena_.x + arena_.width + margin_ - halfW;
        const float minY = arena_.y - margin_ + halfH;
        const float maxY = arena_.y + arena_.height + margin_ - halfH;
        desired.x = minX > maxX ? arena_.x + arena_.width * 0.5f : std::clamp(desired.x, minX, maxX);
        desired.y = minY > maxY ? arena_.y + arena_.height * 0.5f : std::clamp(desired.y, minY, maxY);
    }

    position_ = DampV(position_, desired, kFollowHalfLife, dt);
    kick_ = DampV(kick_, {0.0f, 0.0f}, kKickHalfLife, dt);
    zoomPunch_ = Damp(zoomPunch_, 0.0f, 0.12f, dt);

    trauma_ = std::max(0.0f, trauma_ - kTraumaDecay * dt);
    const float shake = trauma_ * trauma_ * shakeScale_;
    const float t = time_ * 28.0f;
    shakeOffset_ = Vector2{SmoothNoise(t, 1.3f), SmoothNoise(t, 7.9f)} * (kMaxShakeOffset * shake);
    shakeRotation_ = SmoothNoise(t, 4.2f) * kMaxShakeRotation * shake;
}

float CameraRig::Zoom(float screenHeight) const { return screenHeight / viewHeight_ * (1.0f + zoomPunch_); }

Camera2D CameraRig::Get(float screenWidth, float screenHeight) const {
    Camera2D camera{};
    camera.offset = {screenWidth * 0.5f, screenHeight * 0.5f};
    camera.target = position_ + shakeOffset_ + kick_;
    camera.rotation = shakeRotation_ * RAD2DEG;
    camera.zoom = Zoom(screenHeight);
    return camera;
}

Rectangle CameraRig::VisibleWorld(float screenWidth, float screenHeight) const {
    const float zoom = Zoom(screenHeight);
    const float w = screenWidth / zoom;
    const float h = screenHeight / zoom;
    // Pad for shake rotation so culling never pops at the corners.
    const float pad = std::max(w, h) * 0.05f;
    const Vector2 c = position_ + shakeOffset_ + kick_;
    return {c.x - w * 0.5f - pad, c.y - h * 0.5f - pad, w + pad * 2.0f, h + pad * 2.0f};
}

} // namespace gb
