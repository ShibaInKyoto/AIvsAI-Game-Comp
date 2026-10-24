#pragma once

#include "game/Types.h"

#include <raylib.h>

#include <string>

namespace gb {

// Static per-kind tuning data. Behaviour lives in EnemyBehaviors.cpp.
struct EnemyInfo {
    const char* name;
    float hp;
    float speed;
    float radius;
    Color color;
    int xp; // shard value dropped on death
    int score;
    float threat; // director budget cost
    float mass;   // knockback resistance (higher = heavier)
};

const EnemyInfo& GetEnemyInfo(EnemyKind kind);
// Upper-case name for UI; sub-parts read under their parent's name.
std::string EnemyDisplayName(EnemyKind kind);

} // namespace gb
