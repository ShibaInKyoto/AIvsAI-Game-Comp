#pragma once

#include <cstdint>
#include <vector>

namespace gb {

enum class SfxId : std::uint8_t {
    Shoot,
    ShootHeavy,
    ShootScatter,
    EnemyShoot,
    Hit,
    HitCrit,
    ShieldPing,
    EnemyDie,
    EnemyDieBig,
    Explosion,
    PlayerHurt,
    PlayerDeath,
    ShieldBlock,
    Dash,
    Nova,
    LevelUp,
    PickupXp,
    PickupHealth,
    PickupCache,
    Spawn,
    LaserCharge,
    LaserFire,
    Zap,
    Lance,
    Missile,
    Pulse,
    MineArm,
    BossWarning,
    BossRoar,
    BossDeath,
    SectorClear,
    Combo,
    UiMove,
    UiConfirm,
    UiBack,
    UiDeny,
    CardFlip,
    Reroll,
    Count
};

inline constexpr int kSfxSampleRate = 44100;

// Synthesises a sound effect as mono float samples in [-1, 1]. Deterministic:
// the same id always yields the same waveform.
std::vector<float> SynthesizeSfx(SfxId id);

// Minimum time between two plays of the same effect (prevents stacking).
float SfxMinInterval(SfxId id);
// How many overlapping instances of an effect may play at once.
int SfxPolyphony(SfxId id);

} // namespace gb
