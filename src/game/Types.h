#pragma once

#include <cstdint>

namespace gb {

using EntityId = std::uint32_t;
inline constexpr EntityId kInvalidId = 0;

enum class Difficulty : std::uint8_t { Relaxed, Standard, Overclocked, Count };

enum class GameMode : std::uint8_t {
    Campaign,  // five sectors and the final boss, then optional endless
    Endless,   // sector loop with ever-rising threat
    Daily,     // fixed seed + ship derived from the date
    Count
};

enum class EnemyKind : std::uint8_t {
    Drifter,     // slow homing diamond
    Dart,        // aims, telegraphs, then lunges in a straight line
    Pinwheel,    // erratic wanderer that bounces off walls
    Splitter,    // splits into minis on death
    SplitterMini,
    Gunner,      // keeps its distance and fires aimed shots
    Swarmer,     // tiny flocking chaser
    Hive,        // slow, spawns swarmers
    Singularity, // gravity well: pulls the player and bullets
    Weaver,      // serpent head; body segments trail behind
    WeaverBody,
    Shielder,    // frontal shield blocks shots
    Lancer,      // charges and fires a laser beam
    Bomber,      // crosses the arena dropping mines
    Mine,
    // Bosses
    BossWarden,
    BossHydra,
    BossSingularity,
    BossArchitect,
    BossCore,
    BossMinion,  // generic boss-spawned part/turret
    Count
};

inline constexpr bool IsBoss(EnemyKind kind) {
    return kind >= EnemyKind::BossWarden && kind <= EnemyKind::BossCore;
}

// Pieces spawned by another enemy; they share their parent's codex entry.
inline constexpr bool IsSubPart(EnemyKind kind) {
    return kind == EnemyKind::SplitterMini || kind == EnemyKind::WeaverBody || kind == EnemyKind::Mine ||
           kind == EnemyKind::BossMinion;
}

enum class EnemyBulletStyle : std::uint8_t { Orb, Needle, BigOrb };

enum class PickupKind : std::uint8_t { Xp, Health, Cache, Magnet };

// Secondary weapon systems that level up independently of the main gun.
enum class ModuleId : std::uint8_t {
    OrbitBlades,
    Drones,
    ArcCoil,
    Missiles,
    PulseNova,
    Mines,
    PrismLance,
    Count
};

enum class Rarity : std::uint8_t { Common, Rare, Epic, Legendary, Count };

// Simulation -> presentation notifications (particles, sound, shake...).
enum class EventType : std::uint8_t {
    PlayerShot,
    PlayerHit,
    PlayerDeath,
    PlayerDash,
    ShieldBlock,
    NovaFired,
    LevelUp,
    EnemySpawnWarning,
    EnemyHit,
    EnemyKilled,
    EnemyShot,
    EnemyShieldBlock,
    BulletWallHit,
    Explosion,
    PickupXp,
    PickupHealth,
    PickupCache,
    PickupMagnet,
    LaserCharge,
    LaserFire,
    ArcZap,
    PlayerLance,
    MissileLaunch,
    PulseNova,
    MineArmed,
    BossSpawn,
    BossPhase,
    BossDefeated,
    SectorCleared,
    ComboMilestone,
};

} // namespace gb
