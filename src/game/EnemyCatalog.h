#pragma once

#include "game/Types.h"

#include <raylib.h>

namespace gb {

// Static per-kind tuning data. Behaviour lives in EnemyBehaviors.cpp.
struct EnemyInfo {
    const char* name;
    float hp;
    float speed;
    float radius;
    Color color;
    int xp;         // shard value dropped on death
    int score;
    float threat;   // director budget cost
    float mass;     // knockback resistance (higher = heavier)
};

const EnemyInfo& GetEnemyInfo(EnemyKind kind);

} // namespace gb
