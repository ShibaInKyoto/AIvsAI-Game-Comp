#pragma once

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

// Vector2 arithmetic lives in the global namespace (next to raylib's Vector2) so
// that argument-dependent lookup finds it everywhere. raymath's own operator
// overloads are disabled project-wide via RAYMATH_DISABLE_CPP_OPERATORS.
inline Vector2 operator+(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vector2 operator-(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vector2 operator-(Vector2 a) { return {-a.x, -a.y}; }
inline Vector2 operator*(Vector2 a, float s) { return {a.x * s, a.y * s}; }
inline Vector2 operator*(float s, Vector2 a) { return {a.x * s, a.y * s}; }
inline Vector2 operator/(Vector2 a, float s) { return {a.x / s, a.y / s}; }
inline Vector2& operator+=(Vector2& a, Vector2 b) { a.x += b.x; a.y += b.y; return a; }
inline Vector2& operator-=(Vector2& a, Vector2 b) { a.x -= b.x; a.y -= b.y; return a; }
inline Vector2& operator*=(Vector2& a, float s) { a.x *= s; a.y *= s; return a; }

namespace gb {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTau = 2.0f * kPi;

// ---------------------------------------------------------------------------
// Vector helpers (raymath provides the basics; these fill in common gaps).
// ---------------------------------------------------------------------------


inline float Dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
inline float Cross(Vector2 a, Vector2 b) { return a.x * b.y - a.y * b.x; }
inline float LengthSq(Vector2 v) { return v.x * v.x + v.y * v.y; }
inline float Length(Vector2 v) { return std::sqrt(LengthSq(v)); }
inline float DistanceSq(Vector2 a, Vector2 b) { return LengthSq(a - b); }
inline float Distance(Vector2 a, Vector2 b) { return Length(a - b); }

inline Vector2 Normalized(Vector2 v, Vector2 fallback = {1.0f, 0.0f}) {
    const float len = Length(v);
    return len > 1e-6f ? v / len : fallback;
}

inline Vector2 FromAngle(float radians, float length = 1.0f) {
    return {std::cos(radians) * length, std::sin(radians) * length};
}

inline float AngleOf(Vector2 v) { return std::atan2(v.y, v.x); }

inline Vector2 Rotated(Vector2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

inline Vector2 Perp(Vector2 v) { return {-v.y, v.x}; }

inline Vector2 ClampLength(Vector2 v, float maxLength) {
    const float lenSq = LengthSq(v);
    if (lenSq > maxLength * maxLength) {
        return v * (maxLength / std::sqrt(lenSq));
    }
    return v;
}

// ---------------------------------------------------------------------------
// Scalar helpers
// ---------------------------------------------------------------------------

inline float Saturate(float x) { return std::clamp(x, 0.0f, 1.0f); }

inline float LerpF(float a, float b, float t) { return a + (b - a) * t; }

inline Vector2 LerpV(Vector2 a, Vector2 b, float t) { return a + (b - a) * t; }

inline float InverseLerp(float a, float b, float v) {
    return std::abs(b - a) > 1e-6f ? (v - a) / (b - a) : 0.0f;
}

inline float RemapClamped(float inMin, float inMax, float outMin, float outMax, float v) {
    return LerpF(outMin, outMax, Saturate(InverseLerp(inMin, inMax, v)));
}

// Frame-rate independent exponential smoothing: moves `current` toward `target`
// so that the remaining distance halves every `halfLife` seconds.
inline float Damp(float current, float target, float halfLife, float dt) {
    return LerpF(target, current, std::exp2(-dt / std::max(halfLife, 1e-5f)));
}

inline Vector2 DampV(Vector2 current, Vector2 target, float halfLife, float dt) {
    const float t = std::exp2(-dt / std::max(halfLife, 1e-5f));
    return LerpV(target, current, t);
}

inline float MoveToward(float current, float target, float maxDelta) {
    if (std::abs(target - current) <= maxDelta) {
        return target;
    }
    return current + (target > current ? maxDelta : -maxDelta);
}

// Wraps an angle to (-pi, pi].
inline float WrapAngle(float a) {
    a = std::fmod(a + kPi, kTau);
    if (a < 0.0f) {
        a += kTau;
    }
    return a - kPi;
}

inline float AngleDelta(float from, float to) { return WrapAngle(to - from); }

// Rotates `current` toward `target` by at most `maxStep` radians.
inline float RotateToward(float current, float target, float maxStep) {
    const float delta = AngleDelta(current, target);
    if (std::abs(delta) <= maxStep) {
        return target;
    }
    return current + (delta > 0.0f ? maxStep : -maxStep);
}

inline float EaseOutCubic(float t) {
    const float u = 1.0f - Saturate(t);
    return 1.0f - u * u * u;
}

inline float EaseInOutCubic(float t) {
    t = Saturate(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

inline float EaseOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float u = Saturate(t) - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

inline float EaseOutElastic(float t) {
    t = Saturate(t);
    if (t <= 0.0f || t >= 1.0f) {
        return t;
    }
    constexpr float c4 = kTau / 3.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------

inline Color WithAlpha(Color c, float alpha) {
    c.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return c;
}

inline Color LerpColor(Color a, Color b, float t) {
    t = Saturate(t);
    auto mix = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

inline Color ScaleRgb(Color c, float s) {
    auto scale = [s](unsigned char x) {
        return static_cast<unsigned char>(std::clamp(static_cast<float>(x) * s, 0.0f, 255.0f));
    };
    return {scale(c.r), scale(c.g), scale(c.b), c.a};
}

inline Color Hsv(float hueDegrees, float saturation, float value, float alpha = 1.0f) {
    Color c = ColorFromHSV(hueDegrees, saturation, value);
    return WithAlpha(c, alpha);
}

} // namespace gb
