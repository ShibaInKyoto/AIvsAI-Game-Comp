#include "view/MenuBackdrop.h"

#include "core/App.h"

namespace gb {

namespace {

constexpr Rectangle kArea{-1300.0f, -760.0f, 2600.0f, 1520.0f};

} // namespace

MenuBackdrop::MenuBackdrop(Color accent) : accent_(accent), targetAccent_(accent) { grid_.Build(kArea, 48.0f); }

void MenuBackdrop::Pulse(Vector2 position, float strength) {
    grid_.ApplyExplosiveForce(strength, position, 300.0f + strength);
    particles_.ShockRing(position, 10.0f, 200.0f + strength, 0.6f, accent_);
}

void MenuBackdrop::Update(float dt) {
    time_ += dt;
    accent_ = LerpColor(accent_, targetAccent_, 1.0f - std::exp2(-dt / 0.4f));
    pulseTimer_ -= dt;
    Rng& rng = particles_.Random();
    if (pulseTimer_ <= 0.0f) {
        pulseTimer_ = rng.Range(1.2f, 2.6f);
        const Vector2 p{rng.Range(kArea.x + 200.0f, kArea.x + kArea.width - 200.0f),
                        rng.Range(kArea.y + 200.0f, kArea.y + kArea.height - 200.0f)};
        grid_.ApplyExplosiveForce(rng.Range(30.0f, 70.0f), p, 260.0f);
        particles_.ShockRing(p, 10.0f, rng.Range(120.0f, 260.0f), 0.9f, WithAlpha(accent_, 0.6f));
    }
    // Slow wandering gravity well keeps the grid alive.
    grid_.ApplyImplosiveForce(6.0f, {std::cos(time_ * 0.21f) * 700.0f, std::sin(time_ * 0.17f) * 380.0f}, 300.0f);
    if (rng.Chance(dt * 12.0f)) {
        Particle p;
        p.position = {rng.Range(kArea.x, kArea.x + kArea.width), kArea.y + kArea.height + 20.0f};
        p.velocity = {rng.Range(-20.0f, 20.0f), rng.Range(-120.0f, -40.0f)};
        p.color = WithAlpha(accent_, rng.Range(0.3f, 0.7f));
        p.maxLife = p.life = rng.Range(6.0f, 12.0f);
        p.size = rng.Range(8.0f, 18.0f);
        p.width = 2.0f;
        p.drag = 0.0f;
        p.spin = rng.Range(-1.0f, 1.0f);
        p.kind = ParticleKind::Shard;
        particles_.Emit(p);
    }
    grid_.Update(dt);
    particles_.Update(dt, {kArea.x - 400.0f, kArea.y - 400.0f, kArea.width + 800.0f, kArea.height + 800.0f});
}

void MenuBackdrop::Draw(App& app) {
    const Vector2 scene = app.Gfx().SceneSize();
    const float w = scene.x;
    const float h = scene.y;
    Camera2D camera{};
    camera.offset = {w * 0.5f, h * 0.5f};
    camera.target = {std::sin(time_ * 0.05f) * 60.0f, std::cos(time_ * 0.04f) * 30.0f};
    camera.zoom = h / 1150.0f;
    const Rectangle visible{camera.target.x - w / camera.zoom * 0.5f - 50.0f,
                            camera.target.y - h / camera.zoom * 0.5f - 50.0f, w / camera.zoom + 100.0f,
                            h / camera.zoom + 100.0f};
    BeginMode2D(camera);
    NeonRenderer& neon = app.Gfx().Neon();
    neon.Begin(1.6f, 0.15f);
    grid_.Draw(neon, visible, WithAlpha(accent_, 0.045f), WithAlpha(accent_, 0.09f));
    neon.End();
    neon.Begin();
    particles_.Draw(neon, visible);
    neon.End();
    EndMode2D();
}

} // namespace gb
