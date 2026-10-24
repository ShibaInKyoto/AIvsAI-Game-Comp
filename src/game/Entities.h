#pragma once

#include "core/MathUtil.h"
#include "game/Types.h"

#include <raylib.h>

#include <array>
#include <vector>

namespace gb {

struct Player {
    Vector2 position{};
    Vector2 velocity{};
    float aimAngle = -kPi * 0.5f;
    float radius = 13.0f;

    int hp = 5;
    float invulnerable = 0.0f;
    float hitFlash = 0.0f;
    bool alive = true;

    float dashTime = 0.0f;  // > 0 while dashing
    Vector2 dashDirection{1.0f, 0.0f};
    int dashCharges = 1;
    float dashRecharge = 0.0f;

    float fireCooldown = 0.0f;
    float novaCharge = 0.0f;  // 0..1
    int shieldCharges = 0;
    float shieldTimer = 0.0f;
    float regenTimer = 0.0f;
    float recoil = 0.0f;      // cosmetic barrel kick
    float muzzleSide = 1.0f;  // alternates for twin-barrel feel

    bool Dashing() const { return dashTime > 0.0f; }
};

struct Enemy {
    EntityId id = kInvalidId;
    EnemyKind kind = EnemyKind::Drifter;
    Vector2 position{};
    Vector2 velocity{};
    Vector2 knockback{};
    float angle = 0.0f;
    float spin = 0.0f;
    float radius = 16.0f;
    float hp = 10.0f;
    float maxHp = 10.0f;
    float speed = 100.0f;
    float age = 0.0f;
    float spawnDelay = 0.0f;  // telegraph time remaining; inactive while > 0
    float hitFlash = 0.0f;
    float slowTimer = 0.0f;

    // Generic behaviour scratch state.
    int state = 0;
    int counter = 0;
    bool initialized = false;
    float timer = 0.0f;
    float timer2 = 0.0f;
    Vector2 target{};
    float param = 0.0f;

    // Per-damage-source hit cooldowns (continuous damage sources).
    float bladeCooldown = 0.0f;
    float lanceCooldown = 0.0f;
    float dashHitCooldown = 0.0f;

    // Linked bodies (serpent segments, boss parts).
    EntityId leader = kInvalidId;
    int segment = 0;

    bool elite = false;
    bool alive = true;
    bool invulnerable = false;   // e.g. serpent bodies absorb shots without damage
    bool contactDamage = true;
    bool dropsLoot = true;
    bool countsForClear = true;  // false for things like mines

    bool Active() const { return alive && spawnDelay <= 0.0f; }
};

struct Bullet {
    Vector2 position{};
    Vector2 velocity{};
    float radius = 5.0f;
    float damage = 10.0f;
    float life = 0.6f;
    float homing = 0.0f;
    float explosionRadius = 0.0f;
    float slowChance = 0.0f;
    int pierce = 0;
    int bounce = 0;
    bool crit = false;
    bool alive = true;
    std::array<EntityId, 4> recentHits{};
    int recentHitCursor = 0;

    bool RecentlyHit(EntityId id) const {
        for (EntityId h : recentHits) {
            if (h == id) {
                return true;
            }
        }
        return false;
    }
    void RememberHit(EntityId id) {
        recentHits[static_cast<std::size_t>(recentHitCursor)] = id;
        recentHitCursor = (recentHitCursor + 1) % static_cast<int>(recentHits.size());
    }
};

struct EnemyBullet {
    Vector2 position{};
    Vector2 velocity{};
    float radius = 8.0f;
    float life = 6.0f;
    float age = 0.0f;
    float acceleration = 0.0f;  // speeds up (or slows down) along velocity
    float turn = 0.0f;          // curving bullets (radians per second)
    EnemyBulletStyle style = EnemyBulletStyle::Orb;
    Color color{255, 80, 60, 255};
    bool alive = true;
};

// Telegraphed laser beam (enemy lancers and bosses).
struct Beam {
    Vector2 origin{};
    float angle = 0.0f;
    float length = 2600.0f;
    float width = 22.0f;
    float warmup = 1.0f;    // telegraph time remaining
    float duration = 0.45f; // firing time remaining once warm
    float sweep = 0.0f;     // radians per second while firing
    EntityId owner = kInvalidId;
    bool followOwner = true;
    bool alive = true;
    Color color{255, 60, 200, 255};

    bool Firing() const { return warmup <= 0.0f; }
};

struct Pickup {
    PickupKind kind = PickupKind::Xp;
    Vector2 position{};
    Vector2 velocity{};
    int value = 1;
    float age = 0.0f;
    float life = 30.0f;
    bool magnetized = false;
    bool alive = true;
};

// --- Module-owned objects -------------------------------------------------

struct Drone {
    Vector2 position{};
    Vector2 velocity{};
    float cooldown = 0.0f;
    float angle = 0.0f;
};

struct Missile {
    Vector2 position{};
    Vector2 velocity{};
    EntityId target = kInvalidId;
    float damage = 20.0f;
    float radius = 90.0f;
    float life = 3.0f;
    float age = 0.0f;
    bool alive = true;
};

struct PlayerMine {
    Vector2 position{};
    float damage = 40.0f;
    float radius = 120.0f;
    float armTime = 0.4f;
    float life = 10.0f;
    bool alive = true;
};

// Expanding damage ring (pulse nova module, player nova, thorns).
struct Shockwave {
    Vector2 position{};
    float radius = 0.0f;
    float maxRadius = 300.0f;
    float speed = 1400.0f;
    float damage = 20.0f;
    float knockback = 600.0f;
    bool clearsBullets = false;
    bool alive = true;
    std::vector<EntityId> hit;  // each enemy is hit at most once
};

// Visual record of a chain-lightning zap; the simulation applies damage
// instantly and keeps the points around briefly for the renderer.
struct ArcTrace {
    std::vector<Vector2> points;
    float life = 0.18f;
    float maxLife = 0.18f;
};

struct LanceTrace {
    Vector2 origin{};
    Vector2 end{};
    float width = 18.0f;
    float life = 0.25f;
    float maxLife = 0.25f;
};

struct GameEvent {
    EventType type{};
    Vector2 position{};
    Vector2 direction{};
    Color color = WHITE;
    float magnitude = 1.0f;
    int value = 0;
    EnemyKind enemy = EnemyKind::Drifter;
};

} // namespace gb
