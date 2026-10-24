#pragma once

#include "game/Types.h"

#include <array>

namespace gb {

// Fully derived player stats. Rebuilt from the ship's base values plus every
// owned upgrade whenever the build changes, so upgrades never drift.
struct PlayerStats {
    // Movement
    float moveSpeed = 430.0f;
    float dashSpeed = 1500.0f;
    float dashDuration = 0.15f;
    float dashCooldown = 1.0f;
    int dashCharges = 1;
    float dashDamage = 0.0f;  // damage dealt to enemies dashed through (0 = none)

    // Survivability
    int maxHp = 5;
    float invulnerability = 1.25f;
    int shieldCharges = 0;
    float shieldRecharge = 14.0f;
    float regenInterval = 0.0f;  // seconds per 1 HP (0 = none)
    float thornsDamage = 0.0f;   // shockwave damage released when hit

    // Main gun
    float fireRate = 7.5f;      // shots per second
    float damage = 10.0f;
    int projectiles = 1;
    float spread = 0.11f;       // radians between parallel projectiles
    float inaccuracy = 0.025f;  // random jitter in radians
    float bulletSpeed = 1450.0f;
    float bulletLifetime = 0.62f;
    float bulletSize = 1.0f;
    int pierce = 0;
    int bounce = 0;
    float homing = 0.0f;        // turn rate in radians per second
    float critChance = 0.05f;
    float critMultiplier = 2.0f;
    float explosionRadius = 0.0f;
    float slowChance = 0.0f;
    int rearGuns = 0;           // extra shots fired backwards
    int sideGuns = 0;           // extra pairs of shots fired sideways

    // Economy
    float magnetRadius = 150.0f;
    float xpMultiplier = 1.0f;
    float novaChargeRate = 1.0f;
    float luck = 0.0f;
    int rerolls = 1;

    // Global damage multiplier applied to modules as well.
    float powerMultiplier = 1.0f;

    std::array<int, static_cast<std::size_t>(ModuleId::Count)> moduleLevel{};

    int Module(ModuleId id) const { return moduleLevel[static_cast<std::size_t>(id)]; }
    void SetModule(ModuleId id, int level) { moduleLevel[static_cast<std::size_t>(id)] = level; }
};

} // namespace gb
