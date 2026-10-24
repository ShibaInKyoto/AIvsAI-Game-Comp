#include "render/Particles.h"

#include "render/Neon.h"

#include <algorithm>

namespace gb {

ParticleSystem::ParticleSystem(std::size_t capacity) : capacity_(capacity) { particles_.reserve(capacity); }

void ParticleSystem::Emit(const Particle& particle) {
    if (particles_.size() >= capacity_) {
        // Recycle a random existing particle rather than dropping the new one:
        // fresh effects are usually the important ones.
        particles_[rng_.Next() % particles_.size()] = particle;
        return;
    }
    particles_.push_back(particle);
}

void ParticleSystem::Burst(Vector2 position, int count, Color color, float minSpeed, float maxSpeed, float minLife,
                           float maxLife, float length, float width) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = position;
        p.velocity = rng_.UnitVector() * rng_.Range(minSpeed, maxSpeed);
        p.color = color;
        p.maxLife = p.life = rng_.Range(minLife, maxLife);
        p.size = length * rng_.Range(0.6f, 1.3f);
        p.width = width;
        p.drag = rng_.Range(2.0f, 4.0f);
        p.kind = ParticleKind::Spark;
        Emit(p);
    }
}

void ParticleSystem::Spray(Vector2 position, Vector2 direction, float spread, int count, Color color, float minSpeed,
                           float maxSpeed, float minLife, float maxLife, float length, float width) {
    const float baseAngle = AngleOf(direction);
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = position;
        p.velocity = FromAngle(baseAngle + rng_.Range(-spread, spread), rng_.Range(minSpeed, maxSpeed));
        p.color = color;
        p.maxLife = p.life = rng_.Range(minLife, maxLife);
        p.size = length * rng_.Range(0.6f, 1.2f);
        p.width = width;
        p.drag = rng_.Range(3.0f, 5.0f);
        p.kind = ParticleKind::Spark;
        Emit(p);
    }
}

void ParticleSystem::ShockRing(Vector2 position, float startRadius, float endRadius, float duration, Color color) {
    Particle p;
    p.position = position;
    p.color = color;
    p.maxLife = p.life = duration;
    p.size = startRadius;
    p.growth = (endRadius - startRadius) / std::max(duration, 0.01f);
    p.drag = 0.0f;
    p.kind = ParticleKind::Ring;
    Emit(p);
}

void ParticleSystem::GlowPuff(Vector2 position, float radius, float duration, Color color, Vector2 velocity) {
    Particle p;
    p.position = position;
    p.velocity = velocity;
    p.color = color;
    p.maxLife = p.life = duration;
    p.size = radius;
    p.growth = radius * 0.6f;
    p.drag = 2.0f;
    p.kind = ParticleKind::Glow;
    Emit(p);
}

void ParticleSystem::Debris(Vector2 position, int count, Color color, float speed, float size) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = position + rng_.InCircle(size * 0.5f);
        p.velocity = rng_.UnitVector() * speed * rng_.Range(0.4f, 1.0f);
        p.color = color;
        p.maxLife = p.life = rng_.Range(0.5f, 1.1f);
        p.size = size * rng_.Range(0.5f, 1.0f);
        p.width = 2.5f;
        p.drag = 2.2f;
        p.rotation = rng_.Angle();
        p.spin = rng_.Range(-12.0f, 12.0f);
        p.kind = ParticleKind::Shard;
        Emit(p);
    }
}

void ParticleSystem::Update(float dt, Rectangle bounds) {
    const float right = bounds.x + bounds.width;
    const float bottom = bounds.y + bounds.height;
    for (std::size_t i = 0; i < particles_.size();) {
        Particle& p = particles_[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            particles_[i] = particles_.back();
            particles_.pop_back();
            continue;
        }
        p.velocity *= std::exp(-p.drag * dt);
        p.position += p.velocity * dt;
        p.size = std::max(0.0f, p.size + p.growth * dt);
        p.rotation += p.spin * dt;

        // Sparks bounce off the arena walls, which reads nicely at the edges.
        if (p.kind == ParticleKind::Spark || p.kind == ParticleKind::Shard) {
            if (p.position.x < bounds.x || p.position.x > right) {
                p.velocity.x = -p.velocity.x * 0.6f;
                p.position.x = std::clamp(p.position.x, bounds.x, right);
            }
            if (p.position.y < bounds.y || p.position.y > bottom) {
                p.velocity.y = -p.velocity.y * 0.6f;
                p.position.y = std::clamp(p.position.y, bounds.y, bottom);
            }
        }
        ++i;
    }
}

void ParticleSystem::Draw(NeonRenderer& neon, Rectangle visible) const {
    const float margin = 64.0f;
    const float minX = visible.x - margin;
    const float minY = visible.y - margin;
    const float maxX = visible.x + visible.width + margin;
    const float maxY = visible.y + visible.height + margin;

    for (const Particle& p : particles_) {
        if (p.position.x < minX || p.position.x > maxX || p.position.y < minY || p.position.y > maxY) {
            if (p.kind != ParticleKind::Ring) {
                continue;
            }
        }
        const float t = p.life / p.maxLife;  // 1 -> 0 over lifetime
        switch (p.kind) {
        case ParticleKind::Spark: {
            const float speed = Length(p.velocity);
            const float length = std::min(p.size * (0.4f + speed / 300.0f), p.size * 3.0f) * (0.3f + 0.7f * t);
            Color c = WithAlpha(p.color, (static_cast<float>(p.color.a) / 255.0f) * std::min(1.0f, t * 1.6f));
            neon.Streak(p.position, p.velocity, length, p.width * (0.5f + 0.5f * t) * 2.2f, c);
            break;
        }
        case ParticleKind::Glow: {
            Color c = WithAlpha(p.color, (static_cast<float>(p.color.a) / 255.0f) * t * t);
            neon.Glow(p.position, p.size, c);
            break;
        }
        case ParticleKind::Ring: {
            Color c = WithAlpha(p.color, (static_cast<float>(p.color.a) / 255.0f) * t);
            neon.Ring(p.position, p.size, c);
            break;
        }
        case ParticleKind::Shard: {
            const Vector2 half = FromAngle(p.rotation, p.size * 0.5f);
            Color c = WithAlpha(p.color, (static_cast<float>(p.color.a) / 255.0f) * std::min(1.0f, t * 2.0f));
            neon.Line(p.position - half, p.position + half, p.width, c);
            break;
        }
        }
    }
}

} // namespace gb
