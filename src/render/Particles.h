#pragma once

#include "core/MathUtil.h"
#include "core/Random.h"

#include <raylib.h>

#include <vector>

namespace gb {

class NeonRenderer;

enum class ParticleKind : unsigned char {
    Spark, // velocity-aligned streak
    Glow,  // soft round blob
    Ring,  // expanding shockwave ring
    Shard, // spinning line fragment (debris)
};

struct Particle {
    Vector2 position{};
    Vector2 velocity{};
    Color color = WHITE;
    float life = 0.0f;
    float maxLife = 1.0f;
    float size = 8.0f; // length for sparks/shards, radius for glows/rings
    float width = 3.0f;
    float drag = 3.0f;   // exponential velocity decay per second
    float growth = 0.0f; // size change per second (rings expand)
    float rotation = 0.0f;
    float spin = 0.0f;
    ParticleKind kind = ParticleKind::Spark;
};

// Fixed-capacity pool of short-lived cosmetic particles. Purely visual: the
// simulation never reads particle state.
class ParticleSystem {
public:
    explicit ParticleSystem(std::size_t capacity = 16000);

    void Emit(const Particle& particle);

    // Radial burst of sparks: the bread and butter of every explosion.
    void Burst(Vector2 position, int count, Color color, float minSpeed, float maxSpeed, float minLife, float maxLife,
               float length = 14.0f, float width = 3.0f);
    // Sparks thrown inside a cone around `direction`.
    void Spray(Vector2 position, Vector2 direction, float spread, int count, Color color, float minSpeed,
               float maxSpeed, float minLife, float maxLife, float length = 12.0f, float width = 2.5f);
    void ShockRing(Vector2 position, float startRadius, float endRadius, float duration, Color color);
    void GlowPuff(Vector2 position, float radius, float duration, Color color, Vector2 velocity = {});
    void Debris(Vector2 position, int count, Color color, float speed, float size);

    void Update(float dt, Rectangle bounds);
    void Draw(NeonRenderer& neon, Rectangle visible) const;

    void Clear() { particles_.clear(); }
    std::size_t Count() const { return particles_.size(); }
    std::size_t Capacity() const { return capacity_; }
    Rng& Random() { return rng_; }

private:
    std::vector<Particle> particles_;
    std::size_t capacity_;
    Rng rng_{0x5eed1234u};
};

} // namespace gb
