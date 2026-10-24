#pragma once

#include "core/MathUtil.h"

#include <raylib.h>

#include <vector>

namespace gb {

class NeonRenderer;

// Spring-mass background grid that ripples in response to forces
// (explosions, gravity wells, bullets). Simulated at a fixed 60 Hz so its
// feel is independent of the display refresh rate.
class WarpGrid {
public:
    void Build(Rectangle bounds, float spacing);
    void Update(float dt);
    void Draw(NeonRenderer& neon, Rectangle visibleArea, Color minorColor, Color majorColor) const;

    // Pushes points in a direction (bullets, dashes).
    void ApplyDirectedForce(Vector2 force, Vector2 position, float radius);
    // Pushes points away from `position` (explosions).
    void ApplyExplosiveForce(float force, Vector2 position, float radius);
    // Pulls points toward `position` (gravity wells).
    void ApplyImplosiveForce(float force, Vector2 position, float radius);

    int Columns() const { return cols_; }
    int Rows() const { return rows_; }
    Rectangle Bounds() const { return bounds_; }

private:
    struct PointMass {
        Vector3 position;
        Vector3 velocity;
        Vector3 acceleration;
        float inverseMass;
        float damping;
    };

    struct Spring {
        int a;
        int b; // -1 when anchored to a fixed rest position
        float targetLength;
        float stiffness;
        float damping;
    };

    void Step();
    int Index(int x, int y) const { return y * cols_ + x; }
    Vector2 Project(const PointMass& p) const;
    // Clamps a world-space circle to the range of affected grid indices.
    bool IndexRange(Vector2 position, float radius, int& x0, int& y0, int& x1, int& y1) const;

    Rectangle bounds_{};
    float spacing_ = 40.0f;
    int cols_ = 0;
    int rows_ = 0;
    std::vector<PointMass> points_;
    std::vector<Vector3> restPositions_;
    std::vector<Spring> springs_;
    float accumulator_ = 0.0f;
};

} // namespace gb
