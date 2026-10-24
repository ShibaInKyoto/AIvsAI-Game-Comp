#include "render/PostFx.h"

#include "render/ShaderLibrary.h"

#include <rlgl.h>

#include <algorithm>

namespace gb {

bool PostFx::Init(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);

    downShader_ = shaders::Load(nullptr, "shaders/bloom_down.fs.glsl");
    upShader_ = shaders::Load(nullptr, "shaders/bloom_up.fs.glsl");
    compositeShader_ = shaders::Load(nullptr, "shaders/composite.fs.glsl");
    if (!IsShaderValid(downShader_) || !IsShaderValid(upShader_) || !IsShaderValid(compositeShader_)) {
        return false;
    }

    downLocs_ = {GetShaderLocation(downShader_, "uTexel"), GetShaderLocation(downShader_, "uPrefilter"),
                 GetShaderLocation(downShader_, "uThreshold"), GetShaderLocation(downShader_, "uKnee")};
    upLocs_ = {GetShaderLocation(upShader_, "uTexel"), GetShaderLocation(upShader_, "uRadius")};
    compLocs_ = {GetShaderLocation(compositeShader_, "uBloom"),
                 GetShaderLocation(compositeShader_, "uResolution"),
                 GetShaderLocation(compositeShader_, "uTime"),
                 GetShaderLocation(compositeShader_, "uBloomStrength"),
                 GetShaderLocation(compositeShader_, "uExposure"),
                 GetShaderLocation(compositeShader_, "uVignette"),
                 GetShaderLocation(compositeShader_, "uAberration"),
                 GetShaderLocation(compositeShader_, "uGrain"),
                 GetShaderLocation(compositeShader_, "uScanlines"),
                 GetShaderLocation(compositeShader_, "uSaturation"),
                 GetShaderLocation(compositeShader_, "uFlash"),
                 GetShaderLocation(compositeShader_, "uRipples"),
                 GetShaderLocation(compositeShader_, "uRippleCount")};

    CreateTargets();
    // (IsRenderTextureValid would demand a depth buffer, which these targets do not need.)
    return scene_.id != 0 && scene_.texture.id != 0;
}

void PostFx::Shutdown() {
    DestroyTargets();
    for (Shader* shader : {&downShader_, &upShader_, &compositeShader_}) {
        if (shader->id != 0) {
            UnloadShader(*shader);
            *shader = {};
        }
    }
}

void PostFx::Resize(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == width_ && height == height_) {
        return;
    }
    width_ = width;
    height_ = height;
    DestroyTargets();
    CreateTargets();
}

RenderTexture2D PostFx::CreateTarget(int width, int height) {
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0) {
        return target;
    }
    rlEnableFramebuffer(target.id);

    // Prefer a half-float target so bright additive geometry can exceed 1.0
    // and bloom/tone mapping behave like a real HDR pipeline.
    const int format = hdr_ ? PIXELFORMAT_UNCOMPRESSED_R16G16B16A16 : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    target.texture.id = rlLoadTexture(nullptr, width, height, format, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.format = format;
    target.texture.mipmaps = 1;
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);

    if (!rlFramebufferComplete(target.id)) {
        rlDisableFramebuffer();
        rlUnloadTexture(target.texture.id);
        rlUnloadFramebuffer(target.id);
        if (hdr_) {
            TraceLog(LOG_WARNING, "POSTFX: half-float targets unsupported, falling back to 8-bit");
            hdr_ = false;
            return CreateTarget(width, height);
        }
        return RenderTexture2D{};
    }
    rlDisableFramebuffer();

    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(target.texture, TEXTURE_WRAP_CLAMP);
    return target;
}

void PostFx::DestroyTarget(RenderTexture2D& target) {
    if (target.texture.id != 0) {
        rlUnloadTexture(target.texture.id);
    }
    if (target.id != 0) {
        rlUnloadFramebuffer(target.id);
    }
    target = {};
}

void PostFx::CreateTargets() {
    scene_ = CreateTarget(width_, height_);
    mipCount_ = 0;
    int w = width_ / 2;
    int h = height_ / 2;
    for (int i = 0; i < kBloomLevels && w >= 2 && h >= 2; ++i) {
        mips_[static_cast<std::size_t>(i)] = CreateTarget(w, h);
        ++mipCount_;
        w /= 2;
        h /= 2;
    }
    // Freshly allocated textures hold undefined data (possibly NaNs in float
    // formats); clear them so a disabled bloom pass can never leak garbage.
    for (int i = 0; i < mipCount_; ++i) {
        BeginTextureMode(mips_[static_cast<std::size_t>(i)]);
        ClearBackground(BLANK);
        EndTextureMode();
    }
}

void PostFx::DestroyTargets() {
    DestroyTarget(scene_);
    for (auto& mip : mips_) {
        DestroyTarget(mip);
    }
    mipCount_ = 0;
}

void PostFx::BeginScene(Color clearColor) {
    BeginTextureMode(scene_);
    ClearBackground(clearColor);
}

void PostFx::EndScene() { EndTextureMode(); }

void PostFx::Blit(const Texture2D& source, RenderTexture2D& destination, bool clear) {
    BeginTextureMode(destination);
    if (clear) {
        ClearBackground(BLANK);
    }
    // Render textures are stored bottom-up; a negative source height keeps
    // every pass in the same orientation.
    const Rectangle src{0.0f, 0.0f, static_cast<float>(source.width), -static_cast<float>(source.height)};
    const Rectangle dst{0.0f, 0.0f, static_cast<float>(destination.texture.width),
                        static_cast<float>(destination.texture.height)};
    DrawTexturePro(source, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    EndTextureMode();
}

void PostFx::Composite(const PostFxParams& params) {
    const bool bloom = params.bloomEnabled && mipCount_ > 0;

    if (bloom) {
        // Downsample chain: scene -> mip0 -> mip1 -> ...
        BeginShaderMode(downShader_);
        const float knee = std::max(params.bloomKnee, 0.01f);
        SetShaderValue(downShader_, downLocs_.threshold, &params.bloomThreshold, SHADER_UNIFORM_FLOAT);
        SetShaderValue(downShader_, downLocs_.knee, &knee, SHADER_UNIFORM_FLOAT);
        const Texture2D* source = &scene_.texture;
        for (int i = 0; i < mipCount_; ++i) {
            const float texel[2] = {1.0f / static_cast<float>(source->width), 1.0f / static_cast<float>(source->height)};
            const float prefilter = i == 0 ? 1.0f : 0.0f;
            SetShaderValue(downShader_, downLocs_.texel, texel, SHADER_UNIFORM_VEC2);
            SetShaderValue(downShader_, downLocs_.prefilter, &prefilter, SHADER_UNIFORM_FLOAT);
            Blit(*source, mips_[static_cast<std::size_t>(i)], true);
            source = &mips_[static_cast<std::size_t>(i)].texture;
        }
        EndShaderMode();

        // Upsample chain, accumulating additively back up to mip0.
        BeginShaderMode(upShader_);
        SetShaderValue(upShader_, upLocs_.radius, &params.bloomRadius, SHADER_UNIFORM_FLOAT);
        BeginBlendMode(BLEND_ADD_COLORS);
        for (int i = mipCount_ - 1; i > 0; --i) {
            const Texture2D& small = mips_[static_cast<std::size_t>(i)].texture;
            const float texel[2] = {1.0f / static_cast<float>(small.width), 1.0f / static_cast<float>(small.height)};
            SetShaderValue(upShader_, upLocs_.texel, texel, SHADER_UNIFORM_VEC2);
            Blit(small, mips_[static_cast<std::size_t>(i - 1)], false);
        }
        EndBlendMode();
        EndShaderMode();
    }

    // Final composite into whatever target is current (normally the backbuffer).
    BeginShaderMode(compositeShader_);
    const float resolution[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    const float bloomStrength = bloom ? params.bloomStrength / static_cast<float>(std::max(mipCount_, 1)) * 2.0f : 0.0f;
    const float flash[4] = {params.flashColor.r / 255.0f, params.flashColor.g / 255.0f, params.flashColor.b / 255.0f,
                            params.flashAmount};
    SetShaderValue(compositeShader_, compLocs_.resolution, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(compositeShader_, compLocs_.time, &params.time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.bloomStrength, &bloomStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.exposure, &params.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.vignette, &params.vignette, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.aberration, &params.aberration, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.grain, &params.grain, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.scanlines, &params.scanlines, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.saturation, &params.saturation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader_, compLocs_.flash, flash, SHADER_UNIFORM_VEC4);
    std::array<float, PostFxParams::kMaxRipples * 4> ripples{};
    const int rippleCount = std::clamp(params.rippleCount, 0, PostFxParams::kMaxRipples);
    for (int i = 0; i < rippleCount; ++i) {
        const PostFxParams::Ripple& r = params.ripples[static_cast<std::size_t>(i)];
        ripples[static_cast<std::size_t>(i) * 4 + 0] = r.center.x;
        ripples[static_cast<std::size_t>(i) * 4 + 1] = r.center.y;
        ripples[static_cast<std::size_t>(i) * 4 + 2] = r.radius;
        ripples[static_cast<std::size_t>(i) * 4 + 3] = r.strength;
    }
    SetShaderValueV(compositeShader_, compLocs_.ripples, ripples.data(), SHADER_UNIFORM_VEC4, PostFxParams::kMaxRipples);
    SetShaderValue(compositeShader_, compLocs_.rippleCount, &rippleCount, SHADER_UNIFORM_INT);
    // When bloom is disabled the (cleared) mip is still bound; its strength is 0.
    SetShaderValueTexture(compositeShader_, compLocs_.bloom, mips_[0].texture);

    const Rectangle src{0.0f, 0.0f, static_cast<float>(width_), -static_cast<float>(height_)};
    const Rectangle dst{0.0f, 0.0f, static_cast<float>(GetRenderWidth()), static_cast<float>(GetRenderHeight())};
    DrawTexturePro(scene_.texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    EndShaderMode();
}

} // namespace gb
