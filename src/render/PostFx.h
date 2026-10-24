#pragma once

#include <raylib.h>

#include <array>

namespace gb {

struct PostFxParams {
    float bloomStrength = 1.0f;
    float bloomThreshold = 0.6f;
    float bloomKnee = 0.5f;
    float bloomRadius = 1.0f;
    float exposure = 1.6f;
    float vignette = 0.45f;
    float aberration = 0.0f;
    float grain = 0.025f;
    float scanlines = 0.0f;
    float saturation = 1.05f;
    float time = 0.0f;
    Color flashColor = WHITE;
    float flashAmount = 0.0f;
    bool bloomEnabled = true;

    // Screen-space shockwave distortions (uv centre, uv radius, strength).
    static constexpr int kMaxRipples = 4;
    struct Ripple {
        Vector2 center;
        float radius;
        float strength;
    };
    std::array<Ripple, kMaxRipples> ripples{};
    int rippleCount = 0;
};

// HDR scene target plus a physically-inspired bloom chain and a final
// composite pass (tone mapping, vignette, chromatic aberration, grain).
class PostFx {
public:
    static constexpr int kBloomLevels = 6;

    bool Init(int width, int height);
    void Shutdown();
    void Resize(int width, int height);

    // Redirects drawing into the HDR scene buffer.
    void BeginScene(Color clearColor);
    void EndScene();

    // Runs the bloom chain and draws the final image into the current target.
    void Composite(const PostFxParams& params);

    int Width() const { return width_; }
    int Height() const { return height_; }
    bool HasHdr() const { return hdr_; }

private:
    RenderTexture2D CreateTarget(int width, int height);
    void DestroyTarget(RenderTexture2D& target);
    void CreateTargets();
    void DestroyTargets();
    void Blit(const Texture2D& source, RenderTexture2D& destination, bool clear);

    int width_ = 0;
    int height_ = 0;
    bool hdr_ = true;

    RenderTexture2D scene_{};
    std::array<RenderTexture2D, kBloomLevels> mips_{};
    int mipCount_ = 0;

    Shader downShader_{};
    Shader upShader_{};
    Shader compositeShader_{};

    struct DownLocs {
        int texel, prefilter, threshold, knee;
    } downLocs_{};
    struct UpLocs {
        int texel, radius;
    } upLocs_{};
    struct CompositeLocs {
        int bloom, resolution, time, bloomStrength, exposure, vignette, aberration, grain, scanlines, saturation,
            flash, ripples, rippleCount;
    } compLocs_{};
};

} // namespace gb
