#pragma once

#include "game/World.h"

namespace gb {

// Simple potential-field autopilot. Drives automated smoke tests, balance
// runs and the attract-mode demo on the title screen.
class Bot {
public:
    PlayerInput Think(World& world, float dt);

    // Picks the most useful upgrade from a level-up offer.
    static UpgradeId ChooseUpgrade(const World& world, const std::vector<UpgradeId>& offers);
    static SectorModifier ChooseModifier(const World& world, SectorModifier a, SectorModifier b);

private:
    Vector2 wander_{};
    float wanderTimer_ = 0.0f;
    float dashCooldown_ = 0.0f;
};

} // namespace gb
