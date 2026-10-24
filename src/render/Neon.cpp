#include "render/Neon.h"

#include "render/ShaderLibrary.h"

#include <rlgl.h>

#include <algorithm>
#include <array>
#include <vector>

namespace gb {

namespace {

constexpr int kCell = 64;           // atlas cell size in pixels
constexpr int kAtlasWidth = kCell * 4;
constexpr int kAtlasHeight = kCell;
constexpr int kMaxPolylinePoints = 256;

float SmoothStep(float edge0, float edge1, float x) {
    const float t = Saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

// Fills one 64x64 atlas cell. `profile` returns {alpha, coreMask} for a pixel.
template <typename Fn>
void FillCell(std::vector<Color>& pixels, int cellIndex, Fn&& profile) {
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / kCell;
            const float fy = (static_cast<float>(y) + 0.5f) / kCell;
            const auto [alpha, core] = profile(fx, fy);
            const auto a = static_cast<unsigned char>(Saturate(alpha) * 255.0f);
            const auto c = static_cast<unsigned char>(Saturate(core) * 255.0f);
            pixels[static_cast<std::size_t>(y * kAtlasWidth + cellIndex * kCell + x)] = Color{c, c, c, a};
        }
    }
}

struct Profile {
    float alpha;
    float core;
};

Profile LineProfile(float, float fy) {
    const float d = std::abs(fy - 0.5f) * 2.0f;  // 0 at centre, 1 at quad edge
    const float core = 1.0f - SmoothStep(0.20f, 0.36f, d);
    const float halo = std::pow(1.0f - d, 2.4f) * 0.55f;
    return {core + halo * (1.0f - core), 1.0f - SmoothStep(0.0f, 0.22f, d)};
}

Profile DotProfile(float fx, float fy) {
    const float r = std::min(1.0f, Length(Vector2{fx - 0.5f, fy - 0.5f}) * 2.0f);
    const float falloff = std::pow(1.0f - r, 2.2f);
    const float core = 1.0f - SmoothStep(0.0f, 0.28f, r);
    return {falloff * 0.85f + core * 0.15f, core};
}

Profile SolidProfile(float, float) { return {1.0f, 0.0f}; }

Profile RingProfile(float fx, float fy) {
    const float r = Length(Vector2{fx - 0.5f, fy - 0.5f}) * 2.0f;
    const float x = (r - 0.86f);
    const float sharp = std::exp(-(x * x) / (0.05f * 0.05f));
    const float soft = std::exp(-(x * x) / (0.16f * 0.16f)) * 0.35f;
    const float alpha = r > 1.0f ? 0.0f : sharp + soft * (1.0f - sharp);
    return {alpha, std::exp(-(x * x) / (0.025f * 0.025f))};
}

} // namespace

bool NeonRenderer::Init() {
    std::vector<Color> pixels(static_cast<std::size_t>(kAtlasWidth * kAtlasHeight), Color{0, 0, 0, 0});
    FillCell(pixels, 0, LineProfile);
    FillCell(pixels, 1, DotProfile);
    FillCell(pixels, 2, SolidProfile);
    FillCell(pixels, 3, RingProfile);

    Image image{};
    image.data = pixels.data();
    image.width = kAtlasWidth;
    image.height = kAtlasHeight;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    atlas_ = LoadTextureFromImage(image);  // copies pixel data to the GPU
    if (atlas_.id == 0) {
        return false;
    }
    SetTextureFilter(atlas_, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(atlas_, TEXTURE_WRAP_CLAMP);

    // Inset regions by a couple of texels so bilinear filtering never bleeds
    // between neighbouring cells.
    auto region = [](int cell, bool insetVertical) {
        constexpr float inset = 2.0f;
        const float x0 = static_cast<float>(cell * kCell) + inset;
        const float x1 = static_cast<float>((cell + 1) * kCell) - inset;
        const float y0 = insetVertical ? inset : 0.0f;
        const float y1 = insetVertical ? kCell - inset : static_cast<float>(kCell);
        return Region{x0 / kAtlasWidth, y0 / kAtlasHeight, x1 / kAtlasWidth, y1 / kAtlasHeight};
    };
    lineRegion_ = region(0, false);
    dotRegion_ = region(1, true);
    solidRegion_ = region(2, true);
    ringRegion_ = region(3, true);

    shader_ = shaders::Load(nullptr, "shaders/neon.fs.glsl");
    if (!IsShaderValid(shader_)) {
        return false;
    }
    boostLoc_ = GetShaderLocation(shader_, "uBoost");
    whiteCoreLoc_ = GetShaderLocation(shader_, "uWhiteCore");
    return true;
}

void NeonRenderer::Shutdown() {
    if (atlas_.id != 0) {
        UnloadTexture(atlas_);
        atlas_ = {};
    }
    if (shader_.id != 0) {
        UnloadShader(shader_);
        shader_ = {};
    }
}

void NeonRenderer::Begin(float boost) {
    BeginShaderMode(shader_);
    const float whiteCore = 0.85f;
    SetShaderValue(shader_, boostLoc_, &boost, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, whiteCoreLoc_, &whiteCore, SHADER_UNIFORM_FLOAT);
    BeginBlendMode(BLEND_ADD_COLORS);
    // Quads are emitted with arbitrary winding (mirrored shapes, fans), so
    // culling is disabled while the batch is open.
    rlDisableBackfaceCulling();
    active_ = true;
}

void NeonRenderer::End() {
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    EndBlendMode();
    EndShaderMode();
    active_ = false;
}

void NeonRenderer::Quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, const Region& r, Color c) {
    // rlSetTexture/rlBegin are no-ops when the state already matches, so
    // consecutive quads land in the same draw call.
    rlSetTexture(atlas_.id);
    rlBegin(RL_QUADS);
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlTexCoord2f(r.u0, r.v0);
    rlVertex2f(p0.x, p0.y);
    rlTexCoord2f(r.u1, r.v0);
    rlVertex2f(p1.x, p1.y);
    rlTexCoord2f(r.u1, r.v1);
    rlVertex2f(p2.x, p2.y);
    rlTexCoord2f(r.u0, r.v1);
    rlVertex2f(p3.x, p3.y);
    rlEnd();
}

void NeonRenderer::SpriteQuad(Vector2 center, Vector2 axisX, Vector2 axisY, const Region& region, Color color) {
    Quad(center - axisX - axisY, center + axisX - axisY, center + axisX + axisY, center - axisX + axisY, region, color);
}

void NeonRenderer::Line(Vector2 a, Vector2 b, float width, Color color) {
    const Vector2 dir = Normalized(b - a);
    const Vector2 n = Perp(dir) * (width * kLineGlowScale * 0.5f);
    // Line profile varies across V; U runs along the segment.
    Quad(a + n, b + n, b - n, a - n, lineRegion_, color);
}

void NeonRenderer::Polyline(std::span<const Vector2> points, bool closed, float width, Color color) {
    const std::size_t count = std::min<std::size_t>(points.size(), kMaxPolylinePoints);
    if (count < 2) {
        return;
    }
    const float halfWidth = width * kLineGlowScale * 0.5f;
    std::array<Vector2, kMaxPolylinePoints> offsets{};

    for (std::size_t i = 0; i < count; ++i) {
        const bool hasPrev = closed || i > 0;
        const bool hasNext = closed || i + 1 < count;
        const Vector2 cur = points[i];
        const Vector2 prev = points[(i + count - 1) % count];
        const Vector2 next = points[(i + 1) % count];
        if (hasPrev && hasNext) {
            const Vector2 n0 = Perp(Normalized(cur - prev));
            const Vector2 n1 = Perp(Normalized(next - cur));
            const Vector2 miter = Normalized(n0 + n1, n1);
            // Clamp the miter so sharp corners do not spike out.
            const float scale = halfWidth / std::max(Dot(miter, n1), 0.35f);
            offsets[i] = miter * scale;
        } else if (hasNext) {
            offsets[i] = Perp(Normalized(next - cur)) * halfWidth;
        } else {
            offsets[i] = Perp(Normalized(cur - prev)) * halfWidth;
        }
    }

    const std::size_t segments = closed ? count : count - 1;
    for (std::size_t i = 0; i < segments; ++i) {
        const std::size_t j = (i + 1) % count;
        Quad(points[i] + offsets[i], points[j] + offsets[j], points[j] - offsets[j], points[i] - offsets[i],
             lineRegion_, color);
    }
}

void NeonRenderer::Circle(Vector2 center, float radius, float width, Color color, int segments) {
    Arc(center, radius, 0.0f, kTau, width, color, segments);
}

void NeonRenderer::Arc(Vector2 center, float radius, float startAngle, float endAngle, float width, Color color,
                       int segments) {
    const float sweep = endAngle - startAngle;
    const bool closed = std::abs(sweep) >= kTau - 1e-4f;
    if (segments <= 0) {
        // Roughly one segment per 10 world units of arc length, within sane bounds.
        segments = std::clamp(static_cast<int>(std::abs(sweep) * radius / 10.0f), 8, 96);
    }
    std::array<Vector2, kMaxPolylinePoints> pts{};
    const int count = std::min(closed ? segments : segments + 1, kMaxPolylinePoints);
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        pts[static_cast<std::size_t>(i)] = center + FromAngle(startAngle + sweep * t, radius);
    }
    Polyline(std::span<const Vector2>(pts.data(), static_cast<std::size_t>(count)), closed, width, color);
}

void NeonRenderer::RegularPolygon(Vector2 center, int sides, float radius, float rotation, float width, Color color) {
    sides = std::clamp(sides, 3, 64);
    std::array<Vector2, 64> pts{};
    for (int i = 0; i < sides; ++i) {
        pts[static_cast<std::size_t>(i)] = center + FromAngle(rotation + kTau * static_cast<float>(i) / sides, radius);
    }
    Polyline(std::span<const Vector2>(pts.data(), static_cast<std::size_t>(sides)), true, width, color);
}

void NeonRenderer::Glow(Vector2 center, float radius, Color color) {
    SpriteQuad(center, {radius, 0.0f}, {0.0f, radius}, dotRegion_, color);
}

void NeonRenderer::Streak(Vector2 center, Vector2 direction, float length, float width, Color color) {
    const Vector2 dir = Normalized(direction);
    const float halfLength = std::max(length, width) * 0.5f;
    SpriteQuad(center, dir * halfLength, Perp(dir) * (width * 0.5f), dotRegion_, color);
}

void NeonRenderer::Ring(Vector2 center, float radius, Color color) {
    SpriteQuad(center, {radius, 0.0f}, {0.0f, radius}, ringRegion_, color);
}

void NeonRenderer::FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    Quad(a, b, c, c, solidRegion_, color);
}

void NeonRenderer::FillConvex(Vector2 center, std::span<const Vector2> points, Color color) {
    for (std::size_t i = 0; i < points.size(); ++i) {
        FillTriangle(center, points[i], points[(i + 1) % points.size()], color);
    }
}

void NeonRenderer::FillRect(Rectangle rect, Color color) {
    Quad({rect.x, rect.y}, {rect.x + rect.width, rect.y}, {rect.x + rect.width, rect.y + rect.height},
         {rect.x, rect.y + rect.height}, solidRegion_, color);
}

} // namespace gb
