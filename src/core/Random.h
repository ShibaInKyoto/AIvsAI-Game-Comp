#pragma once

#include "core/MathUtil.h"

#include <cstdint>
#include <span>

namespace gb {

// PCG32 (O'Neill, pcg-random.org): small, fast and statistically solid.
// Used for everything gameplay related so runs can be reproduced from a seed.
class Rng {
public:
    Rng() : Rng(0x853c49e6748fea9bULL) {}
    explicit Rng(std::uint64_t seed, std::uint64_t stream = 0xda3e39cb94b95bdbULL) { Seed(seed, stream); }

    void Seed(std::uint64_t seed, std::uint64_t stream = 0xda3e39cb94b95bdbULL) {
        state_ = 0;
        inc_ = (stream << 1u) | 1u;
        Next();
        state_ += seed;
        Next();
    }

    std::uint32_t Next() {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const auto xorShifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorShifted >> rot) | (xorShifted << ((32u - rot) & 31u));
    }

    // Uniform float in [0, 1).
    float Float() { return static_cast<float>(Next() >> 8) * (1.0f / 16777216.0f); }

    float Range(float lo, float hi) { return lo + (hi - lo) * Float(); }

    // Uniform integer in [lo, hi] (inclusive), using Lemire's unbiased method.
    int RangeInt(int lo, int hi) {
        if (hi <= lo) {
            return lo;
        }
        const auto span = static_cast<std::uint32_t>(hi - lo) + 1u;
        std::uint64_t m = static_cast<std::uint64_t>(Next()) * span;
        auto low = static_cast<std::uint32_t>(m);
        if (low < span) {
            const std::uint32_t threshold = (0u - span) % span;
            while (low < threshold) {
                m = static_cast<std::uint64_t>(Next()) * span;
                low = static_cast<std::uint32_t>(m);
            }
        }
        return lo + static_cast<int>(m >> 32u);
    }

    bool Chance(float probability) { return Float() < probability; }

    float Sign() { return (Next() & 1u) != 0u ? 1.0f : -1.0f; }

    float Angle() { return Float() * kTau; }

    Vector2 UnitVector() { return FromAngle(Angle()); }

    Vector2 InCircle(float radius) {
        const float r = radius * std::sqrt(Float());
        return FromAngle(Angle(), r);
    }

    // Picks an index with probability proportional to its weight.
    // Returns -1 when every weight is zero.
    int WeightedIndex(std::span<const float> weights) {
        float total = 0.0f;
        for (float w : weights) {
            total += std::max(w, 0.0f);
        }
        if (total <= 0.0f) {
            return -1;
        }
        float pick = Float() * total;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            const float w = std::max(weights[i], 0.0f);
            if (pick < w) {
                return static_cast<int>(i);
            }
            pick -= w;
        }
        // Floating point slack: return the last positive weight.
        for (std::size_t i = weights.size(); i-- > 0;) {
            if (weights[i] > 0.0f) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::uint64_t State() const { return state_; }

private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 0;
};

// Stateless hash, handy for deterministic per-entity variation.
inline std::uint32_t HashU32(std::uint32_t x) {
    x ^= x >> 16u;
    x *= 0x7feb352dU;
    x ^= x >> 15u;
    x *= 0x846ca68bU;
    x ^= x >> 16u;
    return x;
}

inline float HashFloat(std::uint32_t x) { return static_cast<float>(HashU32(x) >> 8) * (1.0f / 16777216.0f); }

} // namespace gb
