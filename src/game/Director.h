#pragma once

#include "game/Types.h"

#include <raylib.h>

#include <array>

namespace gb {

class World;

enum class SectorPhase : std::uint8_t {
    Intro,        // title card, no spawns yet
    Waves,        // timed spawning
    Purge,        // remaining enemies dissolve
    BossWarning,  // siren before the boss arrives
    Boss,         // boss fight in progress
    Cleared,      // boss down; waiting for the sector modifier choice
    Victory,      // campaign complete
};

struct SectorInfo {
    const char* name;
    const char* subtitle;
    Color gridColor;
    Color accentColor;
    float duration;
    float threatRate;  // director budget per second at the start of the sector
    EnemyKind boss;
    // Spawn weights per enemy kind (zero = never spawns in this sector).
    std::array<float, static_cast<std::size_t>(EnemyKind::Count)> weights;
};

enum class SectorModifier : std::uint8_t {
    Swarm,     // +30% enemies, +30% xp
    Hardened,  // +25% enemy hp, +40% score
    Bounty,    // elites twice as often, +1 cache
    Armory,    // immediate extra upgrade, enemies faster
    Respite,   // repair 2 integrity
    Count
};

struct SectorModifierInfo {
    const char* name;
    const char* description;
    Color color;
};

const SectorModifierInfo& GetSectorModifierInfo(SectorModifier modifier);

// Paces a run: spends a growing threat budget on telegraphed enemy groups,
// throws surges and elites, then stages the sector boss.
class Director {
public:
    static constexpr int kCampaignSectors = 5;

    void Start(int sector);
    void Update(World& world, float dt);

    SectorPhase Phase() const { return phase_; }
    int Sector() const { return sector_; }
    int Loop() const { return sector_ / kCampaignSectors; }
    const SectorInfo& Info() const;
    float PhaseTime() const { return phaseTime_; }
    float WaveTime() const { return waveTime_; }
    float WaveProgress() const;
    EntityId BossId() const { return bossId_; }

    // Called (via World) once the player picked a sector modifier.
    void AdvanceSector(SectorModifier modifier);
    void ContinueEndless();

    // Debug/testing: jump to a sector, optionally straight to its boss.
    void DebugJump(int sector, bool boss);
    // Starts a sector already part-way through its waves (attract mode).
    void JumpToWaves(int sector, float waveTime);

    float EnemyHpMultiplier() const;
    float EnemySpeedMultiplier() const;
    float XpMultiplier() const { return xpMultiplier_; }
    float ScoreMultiplier() const { return scoreMultiplier_; }

private:
    void SetPhase(SectorPhase phase);
    void UpdateWaves(World& world, float dt);
    void SpawnGroup(World& world, EnemyKind kind, bool elite);
    void SpawnSurge(World& world);
    void SpawnBoss(World& world);
    EnemyKind PickKind(World& world) const;
    float CurrentThreatRate() const;
    int EnemyCap() const;

    SectorPhase phase_ = SectorPhase::Intro;
    int sector_ = 0;
    float phaseTime_ = 0.0f;
    float waveTime_ = 0.0f;
    float budget_ = 0.0f;
    float surgeTimer_ = 0.0f;
    float eliteTimer_ = 0.0f;
    EntityId bossId_ = kInvalidId;

    // Accumulated sector modifiers.
    float spawnMultiplier_ = 1.0f;
    float hpMultiplier_ = 1.0f;
    float speedMultiplier_ = 1.0f;
    float xpMultiplier_ = 1.0f;
    float scoreMultiplier_ = 1.0f;
    float eliteRate_ = 1.0f;
};

} // namespace gb
