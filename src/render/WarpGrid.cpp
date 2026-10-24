#include "render/WarpGrid.h"

#include "render/Neon.h"

#include <algorithm>
#include <cmath>

namespace gb {

namespace {

constexpr float kStepTime = 1.0f / 60.0f;
constexpr int kMaxStepsPerUpdate = 4;
constexpr float kDefaultDamping = 0.98f;
constexpr float kPerspectiveDepth = 2000.0f;
constexpr int kMajorLineEvery = 4;

Vector3 Sub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 Add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 Scale(Vector3 v, float s) { return {v.x * s, v.y * s, v.z * s}; }
float LenSq(Vector3 v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

} // namespace

void WarpGrid::Build(Rectangle bounds, float spacing) {
    bounds_ = bounds;
    spacing_ = spacing;
    cols_ = static_cast<int>(bounds.width / spacing) + 1;
    rows_ = static_cast<int>(bounds.height / spacing) + 1;

    points_.assign(static_cast<std::size_t>(cols_ * rows_), PointMass{});
    restPositions_.assign(points_.size(), Vector3{});
    springs_.clear();
    springs_.reserve(points_.size() * 3);

    for (int y = 0; y < rows_; ++y) {
        for (int x = 0; x < cols_; ++x) {
            const Vector3 rest{bounds.x + static_cast<float>(x) * spacing, bounds.y + static_cast<float>(y) * spacing,
                               0.0f};
            const int i = Index(x, y);
            restPositions_[static_cast<std::size_t>(i)] = rest;
            points_[static_cast<std::size_t>(i)] = PointMass{rest, {}, {}, 1.0f, kDefaultDamping};
        }
    }

    for (int y = 0; y < rows_; ++y) {
        for (int x = 0; x < cols_; ++x) {
            const int i = Index(x, y);
            const bool border = x == 0 || y == 0 || x == cols_ - 1 || y == rows_ - 1;
            if (border) {
                springs_.push_back({i, -1, 0.0f, 0.1f, 0.1f});
            } else if (x % 2 == 0 && y % 2 == 0) {
                // Loose anchors pull the mesh back to rest after big impacts.
                springs_.push_back({i, -1, 0.0f, 0.006f, 0.03f});
            }
            // Slightly shorter than rest length keeps the mesh taut.
            if (x > 0) {
                springs_.push_back({Index(x - 1, y), i, spacing * 0.95f, 0.28f, 0.06f});
            }
            if (y > 0) {
                springs_.push_back({Index(x, y - 1), i, spacing * 0.95f, 0.28f, 0.06f});
            }
        }
    }
    accumulator_ = 0.0f;
}

void WarpGrid::Update(float dt) {
    accumulator_ = std::min(accumulator_ + dt, kStepTime * kMaxStepsPerUpdate);
    while (accumulator_ >= kStepTime) {
        Step();
        accumulator_ -= kStepTime;
    }
}

void WarpGrid::Step() {
    for (const Spring& s : springs_) {
        PointMass& a = points_[static_cast<std::size_t>(s.a)];
        const bool anchored = s.b < 0;
        const Vector3 bPos = anchored ? restPositions_[static_cast<std::size_t>(s.a)]
                                      : points_[static_cast<std::size_t>(s.b)].position;
        const Vector3 bVel = anchored ? Vector3{} : points_[static_cast<std::size_t>(s.b)].velocity;

        Vector3 x = Sub(a.position, bPos);
        const float lengthSq = LenSq(x);
        if (lengthSq <= s.targetLength * s.targetLength || lengthSq < 1e-8f) {
            continue;
        }
        const float length = std::sqrt(lengthSq);
        x = Scale(x, (length - s.targetLength) / length);
        const Vector3 dv = Sub(bVel, a.velocity);
        const Vector3 force = Sub(Scale(x, s.stiffness), Scale(dv, s.damping));

        a.acceleration = Sub(a.acceleration, Scale(force, a.inverseMass));
        if (!anchored) {
            PointMass& b = points_[static_cast<std::size_t>(s.b)];
            b.acceleration = Add(b.acceleration, Scale(force, b.inverseMass));
        }
    }

    for (PointMass& p : points_) {
        p.velocity = Add(p.velocity, p.acceleration);
        p.position = Add(p.position, p.velocity);
        p.acceleration = {};
        if (LenSq(p.velocity) < 1e-6f) {
            p.velocity = {};
        }
        p.velocity = Scale(p.velocity, p.damping);
        p.damping = kDefaultDamping;
    }
}

bool WarpGrid::IndexRange(Vector2 position, float radius, int& x0, int& y0, int& x1, int& y1) const {
    if (cols_ == 0) {
        return false;
    }
    x0 = std::max(0, static_cast<int>(std::floor((position.x - radius - bounds_.x) / spacing_)));
    y0 = std::max(0, static_cast<int>(std::floor((position.y - radius - bounds_.y) / spacing_)));
    x1 = std::min(cols_ - 1, static_cast<int>(std::ceil((position.x + radius - bounds_.x) / spacing_)));
    y1 = std::min(rows_ - 1, static_cast<int>(std::ceil((position.y + radius - bounds_.y) / spacing_)));
    return x0 <= x1 && y0 <= y1;
}

void WarpGrid::ApplyDirectedForce(Vector2 force, Vector2 position, float radius) {
    int x0, y0, x1, y1;
    if (!IndexRange(position, radius, x0, y0, x1, y1)) {
        return;
    }
    const float radiusSq = radius * radius;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            PointMass& p = points_[static_cast<std::size_t>(Index(x, y))];
            const float dx = p.position.x - position.x;
            const float dy = p.position.y - position.y;
            const float distSq = dx * dx + dy * dy;
            if (distSq < radiusSq) {
                const float k = 10.0f * p.inverseMass / (10.0f + std::sqrt(distSq));
                p.acceleration.x += force.x * k;
                p.acceleration.y += force.y * k;
            }
        }
    }
}

void WarpGrid::ApplyExplosiveForce(float force, Vector2 position, float radius) {
    int x0, y0, x1, y1;
    if (!IndexRange(position, radius, x0, y0, x1, y1)) {
        return;
    }
    const float radiusSq = radius * radius;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            PointMass& p = points_[static_cast<std::size_t>(Index(x, y))];
            const float dx = p.position.x - position.x;
            const float dy = p.position.y - position.y;
            const float distSq = dx * dx + dy * dy;
            if (distSq < radiusSq) {
                const float k = 100.0f * force * p.inverseMass / (10000.0f + distSq);
                p.acceleration.x += dx * k;
                p.acceleration.y += dy * k;
                p.damping *= 0.6f;
            }
        }
    }
}

void WarpGrid::ApplyImplosiveForce(float force, Vector2 position, float radius) {
    int x0, y0, x1, y1;
    if (!IndexRange(position, radius, x0, y0, x1, y1)) {
        return;
    }
    const float radiusSq = radius * radius;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            PointMass& p = points_[static_cast<std::size_t>(Index(x, y))];
            const float dx = position.x - p.position.x;
            const float dy = position.y - p.position.y;
            const float distSq = dx * dx + dy * dy;
            if (distSq < radiusSq) {
                const float k = 10.0f * force * p.inverseMass / (100.0f + distSq);
                p.acceleration.x += dx * k;
                p.acceleration.y += dy * k;
                p.damping *= 0.6f;
            }
        }
    }
}

Vector2 WarpGrid::Project(const PointMass& p) const {
    // Simple perspective around the arena centre: positive z moves points
    // toward the viewer, which spreads them outward.
    const Vector2 center{bounds_.x + bounds_.width * 0.5f, bounds_.y + bounds_.height * 0.5f};
    const float factor = (p.position.z + kPerspectiveDepth) / kPerspectiveDepth;
    return center + (Vector2{p.position.x, p.position.y} - center) * factor;
}

void WarpGrid::Draw(NeonRenderer& neon, Rectangle visible, Color minorColor, Color majorColor) const {
    if (cols_ < 2 || rows_ < 2) {
        return;
    }
    const float margin = spacing_ * 2.0f;
    const int x0 = std::max(0, static_cast<int>((visible.x - margin - bounds_.x) / spacing_));
    const int y0 = std::max(0, static_cast<int>((visible.y - margin - bounds_.y) / spacing_));
    const int x1 = std::min(cols_ - 1, static_cast<int>((visible.x + visible.width + margin - bounds_.x) / spacing_) + 1);
    const int y1 = std::min(rows_ - 1, static_cast<int>((visible.y + visible.height + margin - bounds_.y) / spacing_) + 1);

    auto displacement = [&](int i) {
        const Vector3 d = Sub(points_[static_cast<std::size_t>(i)].position, restPositions_[static_cast<std::size_t>(i)]);
        return std::sqrt(LenSq(d));
    };

    auto drawSegment = [&](int ia, int ib, bool major) {
        const Vector2 a = Project(points_[static_cast<std::size_t>(ia)]);
        const Vector2 b = Project(points_[static_cast<std::size_t>(ib)]);
        // Lines light up as they are stretched by forces.
        const float energy = std::min(1.0f, (displacement(ia) + displacement(ib)) * 0.02f);
        Color c = major ? majorColor : minorColor;
        const float alpha = static_cast<float>(c.a) / 255.0f * (1.0f + energy * 2.5f);
        c = WithAlpha(c, alpha);
        neon.Line(a, b, major ? 2.2f : 1.3f, c);
    };

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const int i = Index(x, y);
            if (x > x0) {
                drawSegment(Index(x - 1, y), i, y % kMajorLineEvery == 0 || y == rows_ - 1);
            }
            if (y > y0) {
                drawSegment(Index(x, y - 1), i, x % kMajorLineEvery == 0 || x == cols_ - 1);
            }
        }
    }
}

} // namespace gb
