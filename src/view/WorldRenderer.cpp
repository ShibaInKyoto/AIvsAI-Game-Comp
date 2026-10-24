#include "view/WorldRenderer.h"

#include "core/Random.h"
#include "game/EnemyCatalog.h"
#include "render/Neon.h"
#include "render/Particles.h"
#include "render/WarpGrid.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>

namespace gb {

namespace {

constexpr float kEntityBoost = 1.8f;

bool Visible(Vector2 p, float radius, const Rectangle& view) {
    return p.x + radius >= view.x && p.x - radius <= view.x + view.width && p.y + radius >= view.y &&
           p.y - radius <= view.y + view.height;
}

Color XpColor(int value) {
    if (value >= 25) return palette::kXpLarge;
    if (value >= 5) return palette::kXpMedium;
    return palette::kXpSmall;
}

} // namespace

void WorldRenderer::Update(const World& world, float dt) {
    if (stars_.empty()) {
        // Deterministic starfield spread wider than the arena for parallax.
        Rng rng(0x57A25u);
        stars_.reserve(260);
        for (int i = 0; i < 260; ++i) {
            const float depth = rng.Range(0.05f, 0.45f);
            stars_.push_back({{rng.Range(-2600.0f, 2600.0f), rng.Range(-1800.0f, 1800.0f)}, depth,
                              rng.Range(1.5f, 3.5f) * (0.5f + depth), rng.Angle()});
        }
    }
    for (Afterimage& a : afterimages_) {
        a.life = std::max(0.0f, a.life - dt * 3.0f);
    }
    const Player& p = world.GetPlayer();
    afterimageTimer_ -= dt;
    if (p.alive && p.Dashing() && afterimageTimer_ <= 0.0f) {
        afterimageTimer_ = 0.018f;
        afterimages_[static_cast<std::size_t>(afterimageCursor_)] = {p.position, p.aimAngle, 1.0f};
        afterimageCursor_ = (afterimageCursor_ + 1) % static_cast<int>(afterimages_.size());
    }
}

void WorldRenderer::Draw(NeonRenderer& neon, const World& world, const WarpGrid& grid, const ParticleSystem& particles,
                         const WorldDrawContext& ctx) {
    // The grid is a backdrop: drawn dimmer than gameplay entities, which get
    // an HDR boost so they punch through the bloom.
    neon.Begin(1.6f, 0.15f);
    DrawBackdrop(neon, ctx);
    grid.Draw(neon, ctx.visible, ctx.gridMinor, ctx.gridMajor);
    neon.End();
    neon.Begin(kEntityBoost);
    DrawArena(neon, ctx);
    DrawPickups(neon, world, ctx);
    DrawEnemies(neon, world, ctx);
    DrawBeams(neon, world, ctx);
    DrawPlayerBullets(neon, world, ctx);
    DrawModules(neon, world, ctx);
    DrawPlayer(neon, world, ctx);
    particles.Draw(neon, ctx.visible);
    DrawEffectsOverlay(neon, world, ctx);
    neon.End();

    // Enemy bullets get a dark halo underneath so they stay readable on top
    // of explosions and the bright grid.
    DrawEnemyBulletShadows(world, ctx);
    neon.Begin(kEntityBoost);
    DrawEnemyBullets(neon, world, ctx);
    if (ctx.showReticle) {
        const Vector2 r = ctx.reticle;
        const Color c = WithAlpha(WHITE, 0.75f);
        neon.Circle(r, 13.0f, 1.8f, c, 20);
        for (int i = 0; i < 4; ++i) {
            const Vector2 d = FromAngle(kPi * 0.5f * static_cast<float>(i));
            neon.Line(r + d * 18.0f, r + d * 26.0f, 1.8f, c);
        }
        neon.Glow(r, 3.0f, WHITE);
    }
    neon.End();
}

void WorldRenderer::DrawBackdrop(NeonRenderer& neon, const WorldDrawContext& ctx) const {
    // Huge, faint nebula glows tinted by the sector colour sit far behind the grid.
    const Color nebula = ctx.arenaColor;
    static constexpr std::array<Vector3, 6> kNebulae{{{-900.0f, -500.0f, 900.0f},
                                                     {1100.0f, -300.0f, 700.0f},
                                                     {200.0f, 700.0f, 1000.0f},
                                                     {-1400.0f, 900.0f, 650.0f},
                                                     {1500.0f, 1000.0f, 800.0f},
                                                     {-200.0f, -1200.0f, 750.0f}}};
    for (std::size_t i = 0; i < kNebulae.size(); ++i) {
        const Vector3 n = kNebulae[i];
        const Vector2 p = Vector2{n.x, n.y} + ctx.cameraCenter * 0.8f;
        const float pulse = 0.8f + 0.2f * std::sin(ctx.time * 0.15f + static_cast<float>(i));
        const Color tint = i % 2 == 0 ? nebula : LerpColor(nebula, Color{120, 60, 255, 255}, 0.5f);
        neon.Glow(p, n.z, WithAlpha(tint, 0.055f * pulse));
    }
    for (const Star& star : stars_) {
        const Vector2 p = star.position + ctx.cameraCenter * (1.0f - star.depth);
        if (!Visible(p, 8.0f, ctx.visible)) {
            continue;
        }
        const float twinkle = 0.6f + 0.4f * std::sin(ctx.time * (0.8f + star.depth * 2.0f) + star.phase);
        neon.Glow(p, star.size * 2.0f, WithAlpha(Color{180, 200, 255, 255}, 0.25f * twinkle * (0.4f + star.depth)));
    }
}

void WorldRenderer::DrawArena(NeonRenderer& neon, const WorldDrawContext& ctx) const {
    const Rectangle a = World::kArena;
    const std::array<Vector2, 4> corners{Vector2{a.x, a.y}, Vector2{a.x + a.width, a.y},
                                         Vector2{a.x + a.width, a.y + a.height}, Vector2{a.x, a.y + a.height}};
    neon.Polyline(corners, true, 4.5f, WithAlpha(ctx.arenaColor, 0.85f));
    // Bright corner brackets.
    const float len = 90.0f;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const Vector2 c = corners[i];
        const Vector2 toNext = Normalized(corners[(i + 1) % 4] - c);
        const Vector2 toPrev = Normalized(corners[(i + 3) % 4] - c);
        neon.Line(c, c + toNext * len, 7.0f, LerpColor(ctx.arenaColor, WHITE, 0.5f));
        neon.Line(c, c + toPrev * len, 7.0f, LerpColor(ctx.arenaColor, WHITE, 0.5f));
    }
}

void WorldRenderer::DrawPickups(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    for (const Pickup& p : world.Pickups()) {
        if (!p.alive || !Visible(p.position, 30.0f, ctx.visible)) {
            continue;
        }
        // Blink when about to expire.
        if (p.life < 3.0f && std::fmod(p.life * 6.0f, 1.0f) < 0.4f) {
            continue;
        }
        const float spin = ctx.time * 3.0f + p.position.x * 0.01f;
        switch (p.kind) {
        case PickupKind::Xp: {
            const Color c = XpColor(p.value);
            const float size = p.value >= 25 ? 12.0f : (p.value >= 5 ? 9.0f : 6.0f);
            const std::array<Vector2, 4> diamond{p.position + FromAngle(spin, size),
                                                 p.position + FromAngle(spin + kPi * 0.5f, size * 0.6f),
                                                 p.position + FromAngle(spin + kPi, size),
                                                 p.position + FromAngle(spin + kPi * 1.5f, size * 0.6f)};
            neon.Polyline(diamond, true, 2.0f, c);
            neon.Glow(p.position, size * 2.2f, WithAlpha(c, 0.35f));
            break;
        }
        case PickupKind::Health: {
            const float pulse = 1.0f + 0.15f * std::sin(ctx.time * 6.0f);
            const Color c = palette::kHealth;
            neon.Line(p.position - Vector2{12.0f * pulse, 0.0f}, p.position + Vector2{12.0f * pulse, 0.0f}, 4.0f, c);
            neon.Line(p.position - Vector2{0.0f, 12.0f * pulse}, p.position + Vector2{0.0f, 12.0f * pulse}, 4.0f, c);
            neon.Glow(p.position, 34.0f, WithAlpha(c, 0.4f));
            break;
        }
        case PickupKind::Cache: {
            const Color c = palette::kCache;
            neon.RegularPolygon(p.position, 4, 16.0f, spin * 0.5f, 3.0f, c);
            neon.RegularPolygon(p.position, 4, 9.0f, -spin, 2.0f, LerpColor(c, WHITE, 0.4f));
            const float ring = std::fmod(ctx.time * 1.2f, 1.0f);
            neon.Circle(p.position, 16.0f + ring * 30.0f, 2.0f, WithAlpha(c, 1.0f - ring), 24);
            neon.Glow(p.position, 50.0f, WithAlpha(c, 0.35f));
            break;
        }
        case PickupKind::Magnet: {
            const Color c = palette::kMagnet;
            neon.Arc(p.position + Vector2{0.0f, 2.0f}, 11.0f, 0.0f, kPi, 3.5f, c, 12);
            neon.Line(p.position + Vector2{-11.0f, 2.0f}, p.position + Vector2{-11.0f, -10.0f}, 3.5f, c);
            neon.Line(p.position + Vector2{11.0f, 2.0f}, p.position + Vector2{11.0f, -10.0f}, 3.5f, c);
            neon.Glow(p.position, 40.0f, WithAlpha(c, 0.35f));
            break;
        }
        }
    }
}

void WorldRenderer::DrawEnemies(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    for (const Enemy& e : world.Enemies()) {
        if (!e.alive || !Visible(e.position, e.radius * 4.0f, ctx.visible)) {
            continue;
        }
        const Color base = GetEnemyInfo(e.kind).color;
        if (e.spawnDelay > 0.0f) {
            // Telegraph: a contracting ring and a ghost of the incoming shape.
            const float t = Saturate(1.0f - e.spawnDelay / 1.2f);
            const float radius = e.radius * (1.2f + 2.2f * (1.0f - t));
            const float flicker = 0.6f + 0.4f * std::sin(ctx.time * 30.0f);
            neon.Circle(e.position, radius, 2.0f, WithAlpha(base, (0.25f + 0.5f * t) * flicker), 24);
            const float a = ctx.time * 2.0f;
            for (int i = 0; i < 4; ++i) {
                const Vector2 d = FromAngle(a + kPi * 0.5f * static_cast<float>(i));
                neon.Line(e.position + d * (radius + 4.0f), e.position + d * (radius + 14.0f), 2.0f,
                          WithAlpha(base, 0.6f * t));
            }
            if (e.kind != EnemyKind::WeaverBody) {
                Enemy ghost = e;
                ghost.state = 0;
                shapes::DrawEnemyBody(neon, ghost, WithAlpha(base, 0.25f * t), ctx.time);
            }
            continue;
        }

        Color c = LerpColor(base, WHITE, e.hitFlash * 0.8f);
        if (e.slowTimer > 0.0f) {
            c = LerpColor(c, Color{170, 230, 255, 255}, 0.5f);
        }
        if (e.invulnerable && IsBoss(e.kind) && e.kind == EnemyKind::BossArchitect) {
            c = WithAlpha(c, 0.25f);  // teleporting
        }
        shapes::DrawEnemyBody(neon, e, c, ctx.time);
        neon.Glow(e.position, e.radius * 2.0f, WithAlpha(base, IsBoss(e.kind) ? 0.22f : 0.14f));
        if (e.telegraph > 0.0f) {
            // Charge-up warning: a widening, flickering lane toward the target.
            const Vector2 dir = Normalized(e.telegraphTarget - e.position);
            const Vector2 end = e.position + dir * (Distance(e.position, e.telegraphTarget) + 400.0f);
            const float flicker = 0.6f + 0.4f * std::sin(ctx.time * 35.0f);
            neon.Line(e.position, end, 2.0f + e.radius * 0.6f * e.telegraph,
                      WithAlpha(palette::kDanger, (0.15f + 0.35f * e.telegraph) * flicker));
        }

        if (e.elite) {
            const float a = ctx.time * 1.5f;
            for (int i = 0; i < 3; ++i) {
                const float start = a + kTau * static_cast<float>(i) / 3.0f;
                neon.Arc(e.position, e.radius * 1.6f, start, start + 1.4f, 2.5f, palette::kElite, 10);
            }
            neon.Glow(e.position, e.radius * 3.0f, WithAlpha(palette::kElite, 0.15f));
        }
        if (!IsBoss(e.kind) && e.maxHp > 60.0f && e.hp < e.maxHp && e.kind != EnemyKind::WeaverBody) {
            // Small health arc for tanky enemies.
            const float frac = Saturate(e.hp / e.maxHp);
            neon.Arc(e.position, e.radius + 9.0f, -kPi * 0.5f, -kPi * 0.5f + kTau * frac, 2.0f,
                     WithAlpha(base, 0.6f), 20);
        }
    }
}

void WorldRenderer::DrawBeams(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    for (const Beam& b : world.Beams()) {
        if (!b.alive) {
            continue;
        }
        const Vector2 dir = FromAngle(b.angle);
        const Vector2 end = b.origin + dir * b.length;
        if (!b.Firing()) {
            const float t = Saturate(1.0f - b.warmup / 1.3f);
            const float flicker = 0.55f + 0.45f * std::sin(ctx.time * 40.0f);
            neon.Line(b.origin, end, 1.5f + 2.5f * t, WithAlpha(b.color, (0.2f + 0.5f * t) * flicker));
            // Edge guides show the full width that will be lethal.
            const Vector2 side = Perp(dir) * (b.width * 0.5f);
            neon.Line(b.origin + side, end + side, 1.0f, WithAlpha(b.color, 0.15f * t));
            neon.Line(b.origin - side, end - side, 1.0f, WithAlpha(b.color, 0.15f * t));
            neon.Glow(b.origin, 20.0f + 40.0f * t, WithAlpha(b.color, 0.5f * t));
        } else {
            const float wobble = 1.0f + 0.12f * std::sin(ctx.time * 60.0f);
            neon.Line(b.origin, end, b.width * 0.6f * wobble, b.color);
            neon.Line(b.origin, end, b.width * 0.2f, WHITE);
            neon.Glow(b.origin, b.width * 3.0f, b.color);
        }
    }
}

void WorldRenderer::DrawPlayerBullets(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    for (const Bullet& b : world.Bullets()) {
        if (!b.alive || !Visible(b.position, 40.0f, ctx.visible)) {
            continue;
        }
        const Color c = b.crit ? palette::kPlayerBulletCrit : palette::kPlayerBullet;
        const float size = b.radius / 5.0f;
        neon.Streak(b.position, b.velocity, (b.crit ? 34.0f : 26.0f) * size, 8.0f * size, c);
        neon.Streak(b.position, b.velocity, 14.0f * size, 3.5f * size, WHITE);
    }
}

void WorldRenderer::DrawModules(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    const Player& p = world.GetPlayer();
    if (!p.alive) {
        return;
    }
    const int blades = world.OrbitBladeCount();
    if (blades > 0) {
        const float radius = world.OrbitRadius();
        const Color c{140, 255, 200, 255};
        neon.Circle(p.position, radius, 1.2f, WithAlpha(c, 0.12f), 48);
        for (int i = 0; i < blades; ++i) {
            const float a = world.OrbitAngle() + kTau * static_cast<float>(i) / blades;
            const Vector2 pos = p.position + FromAngle(a, radius);
            const Vector2 tangent = FromAngle(a + kPi * 0.5f);
            const std::array<Vector2, 3> blade{pos + tangent * 16.0f, pos + FromAngle(a, 7.0f) - tangent * 8.0f,
                                               pos - FromAngle(a, 7.0f) - tangent * 8.0f};
            neon.Polyline(blade, true, 2.6f, c);
            neon.Arc(p.position, radius, a - 0.5f, a, 3.0f, WithAlpha(c, 0.25f), 6);
            neon.Glow(pos, 22.0f, WithAlpha(c, 0.35f));
        }
    }
    for (const Drone& d : world.Drones()) {
        const Color c{120, 255, 180, 255};
        neon.RegularPolygon(d.position, 3, 9.0f, d.angle, 2.2f, c);
        neon.Glow(d.position, 16.0f, WithAlpha(c, 0.4f));
    }
    for (const Missile& m : world.Missiles()) {
        if (!m.alive) {
            continue;
        }
        const Color c{255, 160, 80, 255};
        const float a = AngleOf(m.velocity);
        const std::array<Vector2, 3> tri{m.position + FromAngle(a, 9.0f), m.position + FromAngle(a + 2.5f, 6.0f),
                                         m.position + FromAngle(a - 2.5f, 6.0f)};
        neon.Polyline(tri, true, 2.0f, c);
        neon.Glow(m.position - FromAngle(a, 8.0f), 14.0f, WithAlpha(c, 0.6f));
    }
    for (const PlayerMine& mine : world.Mines()) {
        if (!mine.alive) {
            continue;
        }
        const Color c{255, 220, 90, 255};
        neon.Circle(mine.position, 9.0f, 2.0f, c, 12);
        if (mine.armTime <= 0.0f && std::fmod(ctx.time * 3.0f + mine.position.x, 1.0f) < 0.5f) {
            neon.Glow(mine.position, 10.0f, c);
        }
    }
}

void WorldRenderer::DrawPlayer(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    const Player& p = world.GetPlayer();
    if (!p.alive) {
        return;
    }
    const ShipInfo& ship = GetShipInfo(world.Config().ship);
    for (const Afterimage& a : afterimages_) {
        if (a.life > 0.0f) {
            shapes::DrawShip(neon, ship.id, a.position, a.angle, 20.0f, ship.color, a.life * 0.35f, 2.0f);
        }
    }

    const bool blink = p.invulnerable > 0.0f && !p.Dashing() && std::fmod(ctx.time * 14.0f, 1.0f) < 0.45f;
    const float alpha = blink ? 0.3f : 1.0f;
    const Color color = LerpColor(ship.color, palette::kDanger, p.hitFlash);

    // Engine glow behind the direction of travel.
    const float speed = Length(p.velocity);
    if (speed > 30.0f) {
        const Vector2 back = Normalized(p.velocity) * -1.0f;
        const float flicker = 0.8f + 0.2f * std::sin(ctx.time * 50.0f);
        neon.Streak(p.position + back * 16.0f, back, 18.0f + speed * 0.03f * flicker, 10.0f,
                    WithAlpha(ship.color, 0.6f * alpha));
    }
    const Vector2 recoil = FromAngle(p.aimAngle, -p.recoil * 3.0f);
    shapes::DrawShip(neon, ship.id, p.position + recoil, p.aimAngle, 20.0f, color, alpha);
    neon.Glow(p.position, 46.0f, WithAlpha(ship.color, 0.18f * alpha));

    if (p.shieldCharges > 0) {
        const float pulse = 0.5f + 0.5f * std::sin(ctx.time * 3.0f);
        neon.Circle(p.position, 30.0f, 1.8f, WithAlpha(palette::kShield, 0.35f + 0.2f * pulse), 28);
        for (int i = 0; i < 3; ++i) {
            const float a = ctx.time * 1.4f + kTau * static_cast<float>(i) / 3.0f;
            neon.Arc(p.position, 30.0f, a, a + 0.7f, 2.6f, WithAlpha(palette::kShield, 0.7f), 8);
        }
    }
}

void WorldRenderer::DrawEnemyBulletShadows(const World& world, const WorldDrawContext& ctx) const {
    for (const EnemyBullet& b : world.EnemyBullets()) {
        if (b.alive && Visible(b.position, b.radius * 3.0f, ctx.visible)) {
            DrawCircleV(b.position, b.radius * 1.9f, Color{0, 0, 0, 150});
        }
    }
}

void WorldRenderer::DrawEnemyBullets(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    for (const EnemyBullet& b : world.EnemyBullets()) {
        if (!b.alive || !Visible(b.position, b.radius * 3.0f, ctx.visible)) {
            continue;
        }
        // Fade in over the first moments so newly fired bullets "materialise".
        const float appear = Saturate(b.age * 12.0f);
        switch (b.style) {
        case EnemyBulletStyle::Orb:
        case EnemyBulletStyle::BigOrb: {
            const float r = b.radius * (0.6f + 0.4f * appear);
            neon.Glow(b.position, r * 2.8f, WithAlpha(b.color, 0.8f));
            neon.Circle(b.position, r, 2.2f, b.color, 12);
            neon.Glow(b.position, r * 0.9f, WHITE);
            break;
        }
        case EnemyBulletStyle::Needle:
            neon.Streak(b.position, b.velocity, b.radius * 4.0f, b.radius * 1.8f, b.color);
            neon.Streak(b.position, b.velocity, b.radius * 2.2f, b.radius * 0.8f, WHITE);
            break;
        }
    }
}

void WorldRenderer::DrawEffectsOverlay(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const {
    for (const Shockwave& s : world.Shockwaves()) {
        const float t = Saturate(s.radius / std::max(s.maxRadius, 1.0f));
        const Color c = s.clearsBullets ? Color{200, 240, 255, 255} : Color{120, 230, 255, 255};
        neon.Circle(s.position, s.radius, 4.0f * (1.0f - t) + 1.0f, WithAlpha(c, 1.0f - t * 0.8f));
        neon.Ring(s.position, s.radius * 1.12f, WithAlpha(c, 0.5f * (1.0f - t)));
    }

    // Lightning: jagged and re-randomised every frame so it crackles.
    Rng jitter(static_cast<std::uint64_t>(ctx.time * 60.0f));
    for (const ArcTrace& arc : world.Arcs()) {
        const float alpha = arc.life / arc.maxLife;
        for (std::size_t i = 0; i + 1 < arc.points.size(); ++i) {
            const Vector2 a = arc.points[i];
            const Vector2 b = arc.points[i + 1];
            std::array<Vector2, 7> pts{};
            const Vector2 n = Perp(Normalized(b - a));
            const float len = Distance(a, b);
            for (std::size_t k = 0; k < pts.size(); ++k) {
                const float t = static_cast<float>(k) / static_cast<float>(pts.size() - 1);
                const float offset = (k == 0 || k == pts.size() - 1) ? 0.0f : jitter.Range(-1.0f, 1.0f) * len * 0.12f;
                pts[k] = LerpV(a, b, t) + n * offset;
            }
            neon.Polyline(pts, false, 3.0f, WithAlpha(Color{150, 200, 255, 255}, alpha));
            neon.Polyline(pts, false, 1.2f, WithAlpha(WHITE, alpha));
        }
        for (const Vector2& p : arc.points) {
            neon.Glow(p, 24.0f, WithAlpha(Color{150, 200, 255, 255}, 0.6f * alpha));
        }
    }

    for (const LanceTrace& l : world.Lances()) {
        const float t = l.life / l.maxLife;
        const Color c{255, 110, 230, 255};
        neon.Line(l.origin, l.end, l.width * t, c);
        neon.Line(l.origin, l.end, l.width * 0.3f * t, WHITE);
    }
}

} // namespace gb
