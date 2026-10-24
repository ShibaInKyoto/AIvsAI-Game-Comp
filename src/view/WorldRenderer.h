#pragma once

#include "game/World.h"

#include <raylib.h>

#include <array>
#include <vector>

namespace gb {

class NeonRenderer;
class WarpGrid;
class ParticleSystem;

struct WorldDrawContext {
    Rectangle visible{};
    Vector2 cameraCenter{};
    float time = 0.0f;
    Color gridMinor{};
    Color gridMajor{};
    Color arenaColor{};
    Vector2 reticle{};
    bool showReticle = false;
    int motif = 0;           // which sector's backdrop motif to draw (0..4)
    float motifAlpha = 1.0f; // cross-fades motifs between sectors
};

// Draws the simulation state with neon primitives. Purely a view: it never
// mutates the world.
class WorldRenderer {
public:
    void Update(const World& world, float dt);
    void Draw(NeonRenderer& neon, const World& world, const WarpGrid& grid, const ParticleSystem& particles,
              const WorldDrawContext& ctx);

private:
    void DrawBackdrop(NeonRenderer& neon, const WorldDrawContext& ctx) const;
    void DrawSectorMotif(NeonRenderer& neon, const WorldDrawContext& ctx) const;
    void DrawArena(NeonRenderer& neon, const WorldDrawContext& ctx) const;
    void DrawPickups(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;
    void DrawEnemies(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;
    void DrawBeams(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;
    void DrawPlayerBullets(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;
    void DrawModules(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;
    void DrawPlayer(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;
    void DrawEnemyBullets(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;
    void DrawEnemyBulletShadows(const World& world, const WorldDrawContext& ctx) const;
    void DrawEffectsOverlay(NeonRenderer& neon, const World& world, const WorldDrawContext& ctx) const;

    struct Star {
        Vector2 position;
        float depth; // 0 = infinitely far (moves with camera), 1 = world-locked
        float size;
        float phase;
    };
    std::vector<Star> stars_;

    struct Afterimage {
        Vector2 position;
        float angle;
        float life;
    };
    std::array<Afterimage, 24> afterimages_{};
    int afterimageCursor_ = 0;
    float afterimageTimer_ = 0.0f;
};

} // namespace gb
