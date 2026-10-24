#include "view/Shapes.h"

#include "game/EnemyCatalog.h"
#include "render/Neon.h"
#include "view/Palette.h"

#include <array>
#include <vector>

namespace gb::shapes {

namespace {

// clang-format off
constexpr std::array<Vector2, 4> kVector{{{1.0f, 0.0f}, {-0.75f, 0.68f}, {-0.35f, 0.0f}, {-0.75f, -0.68f}}};
constexpr std::array<Vector2, 8> kLancer{{{1.35f, 0.0f}, {0.15f, 0.2f}, {-0.45f, 0.62f}, {-0.3f, 0.14f},
                                          {-0.75f, 0.0f}, {-0.3f, -0.14f}, {-0.45f, -0.62f}, {0.15f, -0.2f}}};
constexpr std::array<Vector2, 6> kBruiser{{{0.95f, 0.0f}, {0.45f, 0.65f}, {-0.65f, 0.8f}, {-0.4f, 0.0f},
                                           {-0.65f, -0.8f}, {0.45f, -0.65f}}};
constexpr std::array<Vector2, 6> kWarden{{{1.05f, 0.0f}, {0.4f, 0.72f}, {-0.55f, 0.72f}, {-0.85f, 0.0f},
                                          {-0.55f, -0.72f}, {0.4f, -0.72f}}};
constexpr std::array<Vector2, 6> kSpecter{{{1.15f, 0.0f}, {-0.15f, 0.32f}, {-0.95f, 0.95f}, {-0.5f, 0.0f},
                                           {-0.95f, -0.95f}, {-0.15f, -0.32f}}};
constexpr std::array<Vector2, 6> kHivemind{{{0.95f, 0.0f}, {-0.6f, 0.85f}, {-0.3f, 0.28f}, {-0.62f, 0.0f},
                                            {-0.3f, -0.28f}, {-0.6f, -0.85f}}};
// clang-format on

// Transforms a local outline into world space in a small stack buffer.
template <std::size_t N>
std::array<Vector2, N> Transform(const std::array<Vector2, N>& local, Vector2 position, float angle, float scale) {
    std::array<Vector2, N> out{};
    for (std::size_t i = 0; i < N; ++i) {
        out[i] = position + Rotated(local[i] * scale, angle);
    }
    return out;
}

std::vector<Vector2> TransformSpan(std::span<const Vector2> local, Vector2 position, float angle, float scale) {
    std::vector<Vector2> out(local.size());
    for (std::size_t i = 0; i < local.size(); ++i) {
        out[i] = position + Rotated(local[i] * scale, angle);
    }
    return out;
}

Color Faint(Color c, float alpha) { return WithAlpha(c, alpha); }

void Star(NeonRenderer& neon, Vector2 c, int points, float outer, float inner, float rotation, float width,
          Color color) {
    std::array<Vector2, 32> pts{};
    const int n = std::min(points * 2, 32);
    for (int i = 0; i < n; ++i) {
        const float r = (i % 2 == 0) ? outer : inner;
        pts[static_cast<std::size_t>(i)] = c + FromAngle(rotation + kTau * static_cast<float>(i) / n, r);
    }
    neon.Polyline(std::span<const Vector2>(pts.data(), static_cast<std::size_t>(n)), true, width, color);
}

} // namespace

std::span<const Vector2> ShipOutline(ShipId ship) {
    switch (ship) {
    case ShipId::Vector: return kVector;
    case ShipId::Lancer: return kLancer;
    case ShipId::Bruiser: return kBruiser;
    case ShipId::Warden: return kWarden;
    case ShipId::Specter: return kSpecter;
    case ShipId::Hivemind: return kHivemind;
    case ShipId::Count: break;
    }
    return kVector;
}

void DrawShip(NeonRenderer& neon, ShipId ship, Vector2 position, float angle, float scale, Color color, float alpha,
              float thickness) {
    const auto outline = TransformSpan(ShipOutline(ship), position, angle, scale);
    neon.FillConvex(position, outline, Faint(color, 0.10f * alpha));
    neon.Polyline(outline, true, thickness, Faint(LerpColor(color, WHITE, 0.25f), alpha));
    // Cockpit spark.
    neon.Glow(position + FromAngle(angle, scale * 0.15f), scale * 0.5f, Faint(color, 0.35f * alpha));
}

void DrawEnemyBody(NeonRenderer& neon, const Enemy& e, Color c, float time) {
    const Vector2 p = e.position;
    const float r = e.radius;
    switch (e.kind) {
    case EnemyKind::Drifter: {
        const std::array<Vector2, 4> outer{p + FromAngle(e.angle, r), p + FromAngle(e.angle + kPi * 0.5f, r * 0.7f),
                                           p + FromAngle(e.angle + kPi, r),
                                           p + FromAngle(e.angle + kPi * 1.5f, r * 0.7f)};
        neon.Polyline(outer, true, 3.0f, c);
        neon.RegularPolygon(p, 4, r * 0.35f, -e.angle * 2.0f, 2.0f, Faint(c, 0.7f));
        break;
    }
    case EnemyKind::Dart: {
        const auto pts = Transform(
            std::array<Vector2, 4>{{{1.1f, 0.0f}, {-0.8f, 0.75f}, {-0.35f, 0.0f}, {-0.8f, -0.75f}}}, p, e.angle, r);
        neon.Polyline(pts, true, 3.0f, c);
        if (e.state == 1) {
            // Lock-on line: tells the player exactly where the lunge will go.
            const float t = 1.0f - Saturate(e.timer / 0.55f);
            neon.Line(p, p + FromAngle(e.angle, 700.0f), 1.6f, Faint(c, 0.15f + 0.45f * t));
            neon.Glow(p, r * 2.2f, Faint(c, 0.4f * t));
        } else if (e.state == 2) {
            neon.Streak(p - FromAngle(e.angle, r * 1.5f), FromAngle(e.angle), r * 4.0f, r * 1.2f, Faint(c, 0.5f));
        }
        break;
    }
    case EnemyKind::Pinwheel: {
        for (int i = 0; i < 4; ++i) {
            const float a = e.angle + kPi * 0.5f * static_cast<float>(i);
            const std::array<Vector2, 3> blade{p, p + FromAngle(a, r), p + FromAngle(a + 0.9f, r * 0.75f)};
            neon.Polyline(blade, true, 2.4f, c);
        }
        break;
    }
    case EnemyKind::Splitter:
    case EnemyKind::SplitterMini: {
        neon.RegularPolygon(p, 4, r, e.angle + kPi * 0.25f, e.kind == EnemyKind::Splitter ? 3.0f : 2.4f, c);
        if (e.kind == EnemyKind::Splitter) {
            neon.Line(p + FromAngle(e.angle, r * 0.6f), p - FromAngle(e.angle, r * 0.6f), 2.0f, Faint(c, 0.7f));
            neon.Line(p + FromAngle(e.angle + kPi * 0.5f, r * 0.6f), p - FromAngle(e.angle + kPi * 0.5f, r * 0.6f),
                      2.0f, Faint(c, 0.7f));
        }
        break;
    }
    case EnemyKind::Gunner: {
        neon.RegularPolygon(p, 3, r, e.angle, 3.0f, c);
        neon.Line(p, p + FromAngle(e.angle, r * 1.4f), 3.0f, c);
        if (e.state == 1) {
            neon.Glow(p + FromAngle(e.angle, r * 1.4f), r * 1.6f, Faint(WHITE, 0.7f));
        }
        break;
    }
    case EnemyKind::Swarmer: {
        const auto pts =
            Transform(std::array<Vector2, 3>{{{1.2f, 0.0f}, {-0.8f, 0.7f}, {-0.8f, -0.7f}}}, p, e.angle, r);
        neon.Polyline(pts, true, 2.2f, c);
        break;
    }
    case EnemyKind::Hive: {
        neon.RegularPolygon(p, 6, r, e.angle, 3.2f, c);
        const float pulse = 0.5f + 0.5f * std::sin(time * 4.0f + e.timer);
        neon.RegularPolygon(p, 6, r * (0.45f + 0.15f * pulse), -e.angle, 2.0f, Faint(c, 0.8f));
        for (int i = 0; i < 3; ++i) {
            neon.Glow(p + FromAngle(time * 2.0f + kTau * static_cast<float>(i) / 3.0f, r * 1.25f), 7.0f, c);
        }
        break;
    }
    case EnemyKind::Singularity: {
        const float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
        neon.Circle(p, r * (0.9f + 0.1f * pulse), 3.2f, c);
        for (int i = 0; i < 3; ++i) {
            const float a = e.angle * 2.0f + kTau * static_cast<float>(i) / 3.0f;
            neon.Arc(p, r * 1.6f, a, a + 1.4f, 2.0f, Faint(c, 0.6f));
        }
        neon.Glow(p, r * 3.5f, Faint(c, 0.12f));
        break;
    }
    case EnemyKind::Weaver: {
        neon.RegularPolygon(p, 4, r * 1.1f, e.angle, 3.2f, c);
        neon.Glow(p + FromAngle(e.angle, r * 0.3f), r * 0.6f, Faint(WHITE, 0.6f));
        break;
    }
    case EnemyKind::WeaverBody: {
        neon.Circle(p, r * 0.8f, 2.4f, Faint(c, 0.8f), 12);
        break;
    }
    case EnemyKind::Shielder: {
        neon.RegularPolygon(p, 6, r * 0.8f, e.angle, 2.6f, c);
        // The shield arc faces the enemy's heading.
        neon.Arc(p, r * 1.35f, e.angle - 1.13f, e.angle + 1.13f, 5.0f, LerpColor(c, WHITE, 0.4f), 16);
        break;
    }
    case EnemyKind::Lancer: {
        const auto pts = Transform(std::array<Vector2, 4>{{{1.4f, 0.0f}, {0.0f, 0.45f}, {-1.0f, 0.0f}, {0.0f, -0.45f}}},
                                   p, e.angle, r);
        neon.Polyline(pts, true, 3.0f, c);
        if (e.state == 1) {
            neon.Glow(p + FromAngle(e.angle, r * 1.4f), r * (1.0f + 1.5f * Saturate(1.75f - e.timer)), Faint(c, 0.6f));
        }
        break;
    }
    case EnemyKind::Bomber: {
        const auto pts = Transform(
            std::array<Vector2, 6>{
                {{1.0f, 0.0f}, {0.3f, 0.9f}, {-0.5f, 0.4f}, {-1.0f, 0.0f}, {-0.5f, -0.4f}, {0.3f, -0.9f}}},
            p, e.angle, r);
        neon.Polyline(pts, true, 3.0f, c);
        neon.Circle(p, r * 0.3f, 2.0f, c, 10);
        break;
    }
    case EnemyKind::Mine: {
        const bool fused = e.state == 1;
        const bool blink =
            fused ? std::fmod(time * 16.0f, 1.0f) < 0.5f : std::fmod(time * 2.0f + e.timer, 1.0f) < 0.15f;
        const Color mc = fused ? palette::kDanger : c;
        Star(neon, p, 6, r, r * 0.55f, e.angle, 2.0f, Faint(mc, 0.9f));
        if (blink) {
            neon.Glow(p, r * 1.2f, mc);
        }
        if (fused) {
            neon.Circle(p, 115.0f, 1.5f, Faint(palette::kDanger, 0.35f));
        }
        break;
    }
    case EnemyKind::BossMinion: {
        neon.RegularPolygon(p, 5, r, e.angle, 3.0f, c);
        neon.Line(p, p + FromAngle(e.angle, r * 1.5f), 3.0f, c);
        neon.Glow(p, r * 0.6f, Faint(c, 0.6f));
        break;
    }
    case EnemyKind::BossWarden: {
        neon.RegularPolygon(p, 8, r, e.angle, 4.5f, c);
        neon.RegularPolygon(p, 4, r * 0.7f, -e.angle * 1.5f, 3.5f, LerpColor(c, WHITE, 0.3f));
        neon.RegularPolygon(p, 4, r * 0.7f, -e.angle * 1.5f + kPi * 0.25f, 3.5f, LerpColor(c, WHITE, 0.3f));
        for (int i = 0; i < 8; ++i) {
            const float a = e.angle + kTau * static_cast<float>(i) / 8.0f;
            neon.Line(p + FromAngle(a, r), p + FromAngle(a, r * 1.25f), 3.0f, c);
        }
        neon.Glow(p, r * 0.5f, Faint(WHITE, 0.6f));
        break;
    }
    case EnemyKind::BossHydra: {
        const auto pts = Transform(std::array<Vector2, 7>{{{1.5f, 0.0f},
                                                           {0.6f, 0.55f},
                                                           {0.2f, 1.1f},
                                                           {-0.9f, 0.6f},
                                                           {-0.9f, -0.6f},
                                                           {0.2f, -1.1f},
                                                           {0.6f, -0.55f}}},
                                   p, e.angle, r);
        neon.Polyline(pts, true, 4.0f, c);
        neon.Glow(p + FromAngle(e.angle + 0.5f, r * 0.7f), 9.0f, palette::kDanger);
        neon.Glow(p + FromAngle(e.angle - 0.5f, r * 0.7f), 9.0f, palette::kDanger);
        break;
    }
    case EnemyKind::BossSingularity: {
        const float pulse = 0.5f + 0.5f * std::sin(time * 3.0f);
        neon.Glow(p, r * 6.0f, Faint(c, 0.10f));
        neon.Circle(p, r * (0.95f + 0.06f * pulse), 5.0f, c);
        for (int ring = 0; ring < 3; ++ring) {
            const float rr = r * (1.4f + 0.45f * static_cast<float>(ring));
            const float dir = ring % 2 == 0 ? 1.0f : -1.0f;
            for (int i = 0; i < 3; ++i) {
                const float a = e.angle * dir * (1.0f + 0.3f * ring) + kTau * static_cast<float>(i) / 3.0f;
                neon.Arc(p, rr, a, a + 1.2f, 3.0f - 0.6f * ring, Faint(c, 0.7f - 0.15f * ring));
            }
        }
        break;
    }
    case EnemyKind::BossArchitect: {
        neon.RegularPolygon(p, 3, r, e.angle, 4.0f, c);
        neon.RegularPolygon(p, 3, r, e.angle + kPi, 4.0f, c);
        neon.Circle(p, r * 0.45f, 3.0f, LerpColor(c, WHITE, 0.4f), 20);
        neon.Glow(p, r * 0.35f, Faint(WHITE, 0.7f));
        break;
    }
    case EnemyKind::BossCore: {
        const float pulse = 0.5f + 0.5f * std::sin(time * 2.5f);
        Color coreColor = e.invulnerable ? Color{140, 200, 255, 255} : Color{255, 120, 140, 255};
        neon.Glow(p, r * 4.0f, Faint(coreColor, 0.12f));
        neon.RegularPolygon(p, 12, r, e.angle, 4.5f, c);
        Star(neon, p, 6, r * 0.75f, r * 0.4f, -e.angle * 2.0f, 3.0f, coreColor);
        neon.Glow(p, r * (0.4f + 0.1f * pulse), Faint(WHITE, 0.8f));
        if (e.invulnerable) {
            neon.Circle(p, r * 1.3f, 3.0f, Faint(Color{140, 200, 255, 255}, 0.5f + 0.3f * pulse));
        }
        break;
    }
    case EnemyKind::Count: break;
    }
}

void DrawEnemyPreview(NeonRenderer& neon, EnemyKind kind, Vector2 center, float scale, float time) {
    Enemy e;
    e.kind = kind;
    e.position = center;
    e.radius = GetEnemyInfo(kind).radius * scale;
    e.angle = time * 0.8f;
    DrawEnemyBody(neon, e, GetEnemyInfo(kind).color, time);
}

void DrawUpgradeIcon(NeonRenderer& neon, UpgradeId id, Vector2 c, float s, Color color, float time) {
    const float w = std::max(2.0f, s * 0.07f);
    auto line = [&](Vector2 a, Vector2 b) { neon.Line(c + a * s, c + b * s, w, color); };
    auto ring = [&](float r) { neon.Circle(c, r * s, w, color, 24); };
    auto poly = [&](int sides, float r, float rot) { neon.RegularPolygon(c, sides, r * s, rot, w, color); };
    auto dot = [&](Vector2 p, float r) { neon.Glow(c + p * s, r * s, color); };
    auto arrow = [&](Vector2 from, Vector2 to) {
        line(from, to);
        const Vector2 dir = Normalized(to - from);
        line(to, to - Rotated(dir, 0.6f) * 0.22f);
        line(to, to - Rotated(dir, -0.6f) * 0.22f);
    };

    switch (id) {
    case UpgradeId::Overclock:
        ring(0.38f);
        line({0.0f, 0.0f}, FromAngle(time * 3.0f, 0.3f));
        line({0.0f, 0.0f}, {0.0f, -0.2f});
        break;
    case UpgradeId::HighVoltage:
        neon.Polyline(std::array<Vector2, 6>{c + Vector2{0.12f, -0.45f} * s, c + Vector2{-0.2f, 0.02f} * s,
                                             c + Vector2{0.05f, 0.02f} * s, c + Vector2{-0.12f, 0.45f} * s,
                                             c + Vector2{0.22f, -0.05f} * s, c + Vector2{-0.02f, -0.05f} * s},
                      true, w, color);
        break;
    case UpgradeId::SplitShot:
        arrow({-0.35f, 0.2f}, {0.35f, -0.35f});
        arrow({-0.35f, 0.2f}, {0.4f, 0.0f});
        arrow({-0.35f, 0.2f}, {0.35f, 0.35f});
        break;
    case UpgradeId::PiercingRounds:
        poly(4, 0.18f, 0.0f);
        arrow({-0.45f, 0.0f}, {0.45f, 0.0f});
        break;
    case UpgradeId::Ricochet:
        neon.Polyline(std::array<Vector2, 3>{c + Vector2{-0.4f, -0.3f} * s, c + Vector2{0.0f, 0.3f} * s,
                                             c + Vector2{0.4f, -0.3f} * s},
                      false, w, color);
        dot({0.4f, -0.3f}, 0.1f);
        break;
    case UpgradeId::Velocity:
        arrow({-0.4f, -0.15f}, {0.4f, -0.15f});
        line({-0.3f, 0.12f}, {0.2f, 0.12f});
        line({-0.2f, 0.3f}, {0.05f, 0.3f});
        break;
    case UpgradeId::HeavyCaliber:
        neon.Glow(c, s * 0.3f, color);
        ring(0.3f);
        break;
    case UpgradeId::CriticalCode:
        ring(0.36f);
        line({-0.45f, 0.0f}, {-0.2f, 0.0f});
        line({0.2f, 0.0f}, {0.45f, 0.0f});
        line({0.0f, -0.45f}, {0.0f, -0.2f});
        line({0.0f, 0.2f}, {0.0f, 0.45f});
        dot({0.0f, 0.0f}, 0.08f);
        break;
    case UpgradeId::Exploit: Star(neon, c, 4, s * 0.45f, s * 0.14f, time, w, color); break;
    case UpgradeId::SeekerRounds:
        neon.Arc(c + Vector2{-0.1f, 0.25f} * s, s * 0.4f, -kPi * 0.9f, -kPi * 0.2f, w, color);
        poly(4, 0.12f, time);
        break;
    case UpgradeId::FragRounds: Star(neon, c, 8, s * 0.42f, s * 0.2f, time * 0.5f, w, color); break;
    case UpgradeId::CryoRounds:
        for (int i = 0; i < 3; ++i) {
            const Vector2 d = FromAngle(kPi * static_cast<float>(i) / 3.0f, 0.42f);
            line(d * -1.0f, d);
        }
        break;
    case UpgradeId::RearGuns:
        poly(3, 0.25f, 0.0f);
        arrow({-0.1f, 0.0f}, {-0.48f, 0.0f});
        break;
    case UpgradeId::SideCannons:
        poly(3, 0.22f, 0.0f);
        arrow({0.0f, 0.0f}, {0.0f, -0.48f});
        arrow({0.0f, 0.0f}, {0.0f, 0.48f});
        break;
    case UpgradeId::HullPlating:
        poly(6, 0.4f, kPi / 6.0f);
        poly(6, 0.22f, kPi / 6.0f);
        break;
    case UpgradeId::NanoRepair:
        line({-0.35f, 0.0f}, {0.35f, 0.0f});
        line({0.0f, -0.35f}, {0.0f, 0.35f});
        break;
    case UpgradeId::Deflector:
        neon.Arc(c, s * 0.4f, -kPi * 0.8f, -kPi * 0.2f, w * 1.5f, color);
        poly(3, 0.15f, 0.0f);
        break;
    case UpgradeId::AutoRepair:
        neon.Arc(c, s * 0.38f, 0.0f, kPi * 1.6f, w, color);
        line({-0.15f, 0.0f}, {0.15f, 0.0f});
        line({0.0f, -0.15f}, {0.0f, 0.15f});
        break;
    case UpgradeId::ReactiveArmor:
        poly(6, 0.25f, 0.0f);
        ring(0.42f);
        break;
    case UpgradeId::Thrusters:
        poly(3, 0.25f, -kPi * 0.5f);
        line({-0.12f, 0.2f}, {-0.12f, 0.45f});
        line({0.12f, 0.2f}, {0.12f, 0.45f});
        break;
    case UpgradeId::Reflex:
        arrow({-0.4f, 0.3f}, {0.4f, -0.3f});
        ring(0.12f);
        break;
    case UpgradeId::ExtraDash:
        arrow({-0.4f, -0.15f}, {0.3f, -0.15f});
        arrow({-0.4f, 0.2f}, {0.3f, 0.2f});
        break;
    case UpgradeId::PhaseBlade:
        neon.Polyline(std::array<Vector2, 3>{c + Vector2{-0.4f, 0.35f} * s, c + Vector2{0.4f, -0.35f} * s,
                                             c + Vector2{0.15f, 0.1f} * s},
                      true, w, color);
        break;
    case UpgradeId::Magnet:
        neon.Arc(c, s * 0.3f, 0.0f, kPi, w * 1.4f, color);
        line({-0.3f, 0.0f}, {-0.3f, -0.35f});
        line({0.3f, 0.0f}, {0.3f, -0.35f});
        break;
    case UpgradeId::DataMiner:
        poly(4, 0.3f, time);
        poly(4, 0.15f, -time);
        break;
    case UpgradeId::Capacitor:
        ring(0.38f);
        neon.Arc(c, s * 0.38f, -kPi * 0.5f, -kPi * 0.5f + kTau * 0.7f, w * 2.0f, color);
        break;
    case UpgradeId::LuckyBreak: Star(neon, c, 5, s * 0.42f, s * 0.18f, -kPi * 0.5f, w, color); break;
    case UpgradeId::BackupDrive:
        neon.Arc(c, s * 0.35f, 0.3f, kTau - 0.3f, w, color);
        arrow({0.33f, -0.1f}, {0.33f, 0.12f});
        break;
    case UpgradeId::OrbitBlades:
        neon.Circle(c, s * 0.38f, w * 0.6f, WithAlpha(color, 0.5f), 24);
        for (int i = 0; i < 3; ++i) {
            const float a = time * 2.0f + kTau * static_cast<float>(i) / 3.0f;
            neon.Line(c + FromAngle(a, s * 0.28f), c + FromAngle(a + 0.35f, s * 0.46f), w * 1.3f, color);
        }
        break;
    case UpgradeId::Drones:
        poly(3, 0.18f, 0.0f);
        poly(4, 0.1f, time);
        neon.RegularPolygon(c + Vector2{-0.3f, -0.3f} * s, 4, s * 0.1f, time, w, color);
        neon.RegularPolygon(c + Vector2{-0.3f, 0.3f} * s, 4, s * 0.1f, time, w, color);
        break;
    case UpgradeId::ArcCoil:
        neon.Polyline(std::array<Vector2, 5>{c + Vector2{-0.45f, 0.1f} * s, c + Vector2{-0.15f, -0.2f} * s,
                                             c + Vector2{0.0f, 0.15f} * s, c + Vector2{0.2f, -0.15f} * s,
                                             c + Vector2{0.45f, 0.05f} * s},
                      false, w, color);
        dot({-0.45f, 0.1f}, 0.08f);
        dot({0.45f, 0.05f}, 0.08f);
        break;
    case UpgradeId::SwarmMissiles:
        arrow({-0.4f, 0.3f}, {0.35f, -0.25f});
        arrow({-0.4f, 0.0f}, {0.2f, -0.4f});
        break;
    case UpgradeId::PulseNova:
        ring(0.15f);
        ring(0.3f);
        neon.Circle(c, s * 0.45f, w, WithAlpha(color, 0.5f), 24);
        break;
    case UpgradeId::ProxMines:
        Star(neon, c, 6, s * 0.35f, s * 0.2f, time, w, color);
        dot({0.0f, 0.0f}, 0.1f);
        break;
    case UpgradeId::PrismLance:
        poly(3, 0.22f, -kPi * 0.5f);
        line({0.0f, 0.0f}, {0.48f, -0.1f});
        line({0.0f, 0.0f}, {0.48f, 0.1f});
        break;
    case UpgradeId::GlassCannon:
        poly(4, 0.4f, 0.0f);
        line({-0.2f, -0.2f}, {0.05f, 0.05f});
        line({0.05f, 0.05f}, {0.0f, 0.3f});
        break;
    case UpgradeId::BulletStorm:
        for (int i = 0; i < 5; ++i) {
            const float y = -0.36f + 0.18f * static_cast<float>(i);
            line({-0.4f + 0.08f * static_cast<float>(i % 2), y}, {0.35f, y});
        }
        break;
    case UpgradeId::OverdriveCore:
        Star(neon, c, 8, s * 0.45f, s * 0.25f, time, w, color);
        neon.Glow(c, s * 0.25f, color);
        break;
    case UpgradeId::Overcharge:
        arrow({0.0f, 0.4f}, {0.0f, -0.42f});
        arrow({-0.3f, 0.4f}, {-0.3f, -0.15f});
        arrow({0.3f, 0.4f}, {0.3f, -0.15f});
        break;
    case UpgradeId::SawStorm:
    case UpgradeId::HiveSwarm:
    case UpgradeId::StormCore:
    case UpgradeId::ClusterBarrage:
    case UpgradeId::ShockReactor:
    case UpgradeId::GravityMines:
    case UpgradeId::SpectrumCannon:
        // Evolutions: the base module glyph inside a radiant star.
        for (const EvolutionRecipe& recipe : AllEvolutions()) {
            if (recipe.evolution == id) {
                DrawUpgradeIcon(neon, recipe.module, c, s * 0.72f, color, time);
            }
        }
        Star(neon, c, 8, s * 0.52f, s * 0.44f, time * 0.4f, w * 0.8f, WithAlpha(color, 0.8f));
        break;
    case UpgradeId::Count: break;
    }
}

} // namespace gb::shapes
