#pragma once

#include <raylib.h>

// Simulation constants shared by the World implementation files
// (World.cpp, Combat.cpp). Internal: not part of the World interface.
namespace gb {

inline constexpr float kComboWindow = 3.0f;
inline constexpr int kMaxBullets = 3000;
inline constexpr int kMaxEnemyBullets = 2500;
inline constexpr int kMaxPickups = 600;
inline constexpr float kPickupCollectRadius = 16.0f;
inline constexpr float kShardAutoCollectAge = 9.0f;
inline constexpr float kNovaPerThreat = 0.0032f;     // ~300 basic kills per nova
inline constexpr float kBulletHitboxScale = 0.55f;  // forgiving: bullets must hit the ship's core
inline constexpr float kBodyHitboxScale = 0.75f;
inline constexpr Color kPlayerBulletColor{255, 240, 140, 255};

} // namespace gb
