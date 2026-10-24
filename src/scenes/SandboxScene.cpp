#include "scenes/SandboxScene.h"

#include "core/App.h"

namespace gb {

namespace {
constexpr Rectangle kArena{-1400.0f, -900.0f, 2800.0f, 1800.0f};
}

SandboxScene::SandboxScene(App& app) : Scene(app) {
    grid_.Build(kArena, 40.0f);
    camera_.SetBounds(kArena, 80.0f);
    camera_.SnapTo({0.0f, 0.0f});
}

void SandboxScene::Update(float dt) {
    time_ += dt;
    burstTimer_ -= dt;
    if (burstTimer_ <= 0.0f) {
        burstTimer_ = 0.6f;
        Rng& rng = particles_.Random();
        const Vector2 p{rng.Range(-700.0f, 700.0f), rng.Range(-400.0f, 400.0f)};
        const Color colors[] = {{255, 60, 200, 255}, {60, 220, 255, 255}, {255, 180, 40, 255}, {120, 255, 90, 255}};
        const Color c = colors[rng.RangeInt(0, 3)];
        particles_.Burst(p, 60, c, 200.0f, 900.0f, 0.4f, 1.1f, 18.0f, 3.0f);
        particles_.ShockRing(p, 10.0f, 220.0f, 0.45f, c);
        particles_.GlowPuff(p, 90.0f, 0.4f, c);
        grid_.ApplyExplosiveForce(80.0f, p, 260.0f);
    }
    grid_.ApplyImplosiveForce(30.0f, {std::cos(time_) * 300.0f, 250.0f}, 260.0f);
    grid_.Update(dt);
    particles_.Update(dt, kArena);
    camera_.Update(dt, {0.0f, 0.0f}, {0.0f, 0.0f}, static_cast<float>(GetRenderWidth()),
                   static_cast<float>(GetRenderHeight()));
}

void SandboxScene::DrawWorld() {
    const float w = static_cast<float>(GetRenderWidth());
    const float h = static_cast<float>(GetRenderHeight());
    BeginMode2D(camera_.Get(w, h));
    NeonRenderer& neon = app_.Gfx().Neon();
    neon.Begin(1.0f);
    grid_.Draw(neon, camera_.VisibleWorld(w, h), Color{40, 60, 160, 70}, Color{60, 90, 220, 120});
    neon.Polyline(std::array<Vector2, 4>{Vector2{kArena.x, kArena.y}, {kArena.x + kArena.width, kArena.y},
                                         {kArena.x + kArena.width, kArena.y + kArena.height},
                                         {kArena.x, kArena.y + kArena.height}},
                  true, 4.0f, Color{120, 160, 255, 200});
    // Player-like ship
    const Vector2 shipPos{std::cos(time_ * 0.7f) * 200.0f, std::sin(time_ * 0.9f) * 120.0f};
    const float a = time_;
    std::array<Vector2, 4> ship{shipPos + FromAngle(a, 28.0f), shipPos + FromAngle(a + 2.5f, 22.0f),
                                shipPos + FromAngle(a + kPi, 8.0f), shipPos + FromAngle(a - 2.5f, 22.0f)};
    neon.Polyline(ship, true, 3.5f, Color{240, 250, 255, 255});
    neon.Glow(shipPos, 60.0f, Color{120, 200, 255, 90});
    // Enemies
    for (int i = 0; i < 12; ++i) {
        const float t = time_ * 0.8f + static_cast<float>(i);
        const Vector2 p{std::cos(t * 0.5f + i) * 600.0f, std::sin(t * 0.6f + i * 2) * 350.0f};
        if (i % 3 == 0) {
            neon.RegularPolygon(p, 4, 26.0f, t * 2.0f, 3.0f, Color{60, 230, 255, 255});
        } else if (i % 3 == 1) {
            neon.RegularPolygon(p, 3, 26.0f, -t * 3.0f, 3.0f, Color{255, 120, 40, 255});
        } else {
            neon.RegularPolygon(p, 6, 30.0f, t, 3.0f, Color{255, 60, 220, 255});
            neon.RegularPolygon(p, 6, 14.0f, -t, 2.0f, Color{255, 60, 220, 200});
        }
    }
    for (int i = 0; i < 40; ++i) {
        const Vector2 p{-500.0f + i * 25.0f, -300.0f + std::sin(time_ * 4.0f + i) * 20.0f};
        neon.Streak(p, {1.0f, 0.2f}, 26.0f, 8.0f, Color{255, 240, 120, 255});
    }
    particles_.Draw(neon, camera_.VisibleWorld(w, h));
    neon.End();
    EndMode2D();
}

void SandboxScene::DrawUi() {
    TextRenderer& text = app_.Gfx().Text();
    const Vector2 ui = app_.Gfx().UiSize();
    text.DrawGlow(FontId::Display, "GRIDBREAKER", {ui.x * 0.5f, 120.0f}, 120.0f, WHITE, Color{80, 200, 255, 255},
                  Align::Center, 0.08f);
    text.Draw(FontId::Body, "Neon renderer test bed - bloom, warp grid, particles", {ui.x * 0.5f, 260.0f}, 34.0f,
              Color{180, 200, 255, 255}, Align::Center);
    text.Draw(FontId::Heading, TextFormat("%d FPS", GetFPS()), {20.0f, 20.0f}, 28.0f, WHITE);
}

} // namespace gb
