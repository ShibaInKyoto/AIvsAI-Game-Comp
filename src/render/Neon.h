#pragma once

#include "core/MathUtil.h"

#include <raylib.h>

#include <span>

namespace gb {

// Batched additive "neon" primitive renderer.
//
// Every primitive is a textured quad sampling a small procedurally generated
// atlas (soft line profile, radial glow, solid fill, ring). All of them share a
// single texture so a whole frame of glowing geometry batches into very few
// draw calls. Colour alpha is used as brightness.
class NeonRenderer {
public:
    // Visual core thickness occupies the middle third of a glow quad.
    static constexpr float kLineGlowScale = 3.0f;

    bool Init();
    void Shutdown();

    // Begin/End wrap a block of neon drawing: they bind the additive shader and
    // blend mode. Plain raylib drawing must not happen inside the block.
    // boost scales brightness (HDR); whiteCore pushes line centres toward white.
    void Begin(float boost = 1.0f, float whiteCore = 0.85f);
    void End();
    bool Active() const { return active_; }

    void Line(Vector2 a, Vector2 b, float width, Color color);
    void Polyline(std::span<const Vector2> points, bool closed, float width, Color color);
    void Circle(Vector2 center, float radius, float width, Color color, int segments = 0);
    void Arc(Vector2 center, float radius, float startAngle, float endAngle, float width, Color color, int segments = 0);
    void RegularPolygon(Vector2 center, int sides, float radius, float rotation, float width, Color color);

    // Soft round glow sprite.
    void Glow(Vector2 center, float radius, Color color);
    // Velocity-stretched glow, used for sparks and bullets.
    void Streak(Vector2 center, Vector2 direction, float length, float width, Color color);
    // Bright ring (shockwaves). Small rings use the atlas sprite; large ones
    // switch to line geometry so the profile never shows magnified texels.
    void Ring(Vector2 center, float radius, Color color);
    // Flat additive fill.
    void FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color);
    void FillConvex(Vector2 center, std::span<const Vector2> points, Color color);
    void FillRect(Rectangle rect, Color color);

    const Texture2D& Atlas() const { return atlas_; }

private:
    struct Region {
        float u0, v0, u1, v1;
    };

    void Quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, const Region& region, Color color);
    void SpriteQuad(Vector2 center, Vector2 axisX, Vector2 axisY, const Region& region, Color color);

    Texture2D atlas_{};
    Shader shader_{};
    int boostLoc_ = -1;
    int whiteCoreLoc_ = -1;
    bool active_ = false;

    Region lineRegion_{};
    Region dotRegion_{};
    Region solidRegion_{};
    Region ringRegion_{};
};

} // namespace gb
