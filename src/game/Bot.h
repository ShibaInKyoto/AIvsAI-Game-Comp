#pragma once

#include "game/World.h"

namespace gb {

// Simple potential-field autopilot. Drives automated smoke tests, balance
// runs and the attract-mode demo on the title screen.
class Bot {
public:
    // skill in [0, 1]: lower skill reacts more slowly, approximating a human.
    explicit Bot(float skill = 1.0f) : skill_(skill) {}

    PlayerInput Think(World& world, float dt);

    // Picks the most useful upgrade from a level-up offer.
    static UpgradeId ChooseUpgrade(const World& world, const std::vector<UpgradeId>& offers);
    static SectorModifier ChooseModifier(const World& world, SectorModifier a, SectorModifier b);

private:
    Vector2 Steer(World& world, bool& emergency);

    float skill_ = 1.0f;
    float reactionTimer_ = 0.0f;
    Vector2 move_{};
    bool emergency_ = false;
    Vector2 wander_{};
    float wanderTimer_ = 0.0f;
    float dashCooldown_ = 0.0f;
};

} // namespace gb
