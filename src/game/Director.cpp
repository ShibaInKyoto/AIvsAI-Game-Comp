#include "game/Director.h"

#include "game/EnemyCatalog.h"
#include "game/World.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace gb {

namespace {

using Weights = std::array<float, static_cast<std::size_t>(EnemyKind::Count)>;

Weights MakeWeights(std::initializer_list<std::pair<EnemyKind, float>> entries) {
    Weights weights{};
    for (const auto& [kind, weight] : entries) {
        weights[static_cast<std::size_t>(kind)] = weight;
    }
    return weights;
}

const std::array<SectorInfo, Director::kCampaignSectors>& Sectors() {
    using K = EnemyKind;
    // clang-format off
    static const std::array<SectorInfo, Director::kCampaignSectors> sectors{{
        {"SECTOR 01", "PERIMETER", {40, 90, 220, 255}, {80, 220, 255, 255}, 110.0f, 2.5f, K::BossWarden,
         MakeWeights({{K::Drifter, 10.0f}, {K::Pinwheel, 5.0f}, {K::Dart, 3.0f}, {K::Swarmer, 2.0f}})},
        {"SECTOR 02", "DATA STREAM", {150, 40, 200, 255}, {255, 90, 220, 255}, 130.0f, 3.5f, K::BossHydra,
         MakeWeights({{K::Drifter, 6.0f}, {K::Pinwheel, 4.0f}, {K::Dart, 4.0f}, {K::Swarmer, 3.0f},
                      {K::Splitter, 4.0f}, {K::Gunner, 3.0f}, {K::Weaver, 0.8f}})},
        {"SECTOR 03", "EVENT HORIZON", {200, 90, 30, 255}, {255, 170, 60, 255}, 145.0f, 4.6f, K::BossSingularity,
         MakeWeights({{K::Drifter, 5.0f}, {K::Dart, 3.0f}, {K::Swarmer, 3.0f}, {K::Splitter, 3.0f},
                      {K::Gunner, 3.0f}, {K::Singularity, 1.0f}, {K::Bomber, 2.0f}, {K::Hive, 0.8f},
                      {K::Weaver, 1.0f}})},
        {"SECTOR 04", "FIREWALL", {30, 160, 90, 255}, {110, 255, 140, 255}, 155.0f, 5.6f, K::BossArchitect,
         MakeWeights({{K::Drifter, 4.0f}, {K::Dart, 3.0f}, {K::Pinwheel, 2.0f}, {K::Swarmer, 3.0f},
                      {K::Splitter, 2.0f}, {K::Gunner, 3.0f}, {K::Shielder, 3.0f}, {K::Lancer, 2.5f},
                      {K::Weaver, 1.2f}, {K::Bomber, 1.5f}})},
        {"SECTOR 05", "THE CORE", {200, 30, 60, 255}, {255, 90, 90, 255}, 165.0f, 6.8f, K::BossCore,
         MakeWeights({{K::Drifter, 3.0f}, {K::Dart, 3.0f}, {K::Pinwheel, 2.0f}, {K::Swarmer, 3.0f},
                      {K::Splitter, 2.5f}, {K::Gunner, 3.0f}, {K::Shielder, 2.5f}, {K::Lancer, 2.5f},
                      {K::Weaver, 1.5f}, {K::Bomber, 1.5f}, {K::Singularity, 0.8f}, {K::Hive, 1.0f}})},
    }};
    // clang-format on
    return sectors;
}

// clang-format off
constexpr std::array<SectorModifierInfo, static_cast<std::size_t>(SectorModifier::Count)> kModifiers{{
    {"SWARM PROTOCOL", "+30% enemies. +30% experience.", {255, 90, 200, 255}},
    {"HARDENED TARGETS", "+25% enemy integrity. +40% score.", {255, 160, 60, 255}},
    {"BOUNTY HUNT", "Elites appear twice as often. Gain an upgrade cache now.", {255, 210, 70, 255}},
    {"ARMORY", "Gain an upgrade cache now. Enemies move 10% faster.", {120, 220, 255, 255}},
    {"RESPITE", "Repair 2 integrity. No strings attached.", {110, 255, 150, 255}},
}};
// clang-format on

constexpr float kIntroDuration = 3.0f;
constexpr float kPurgeDuration = 1.2f;
constexpr float kBossWarningDuration = 3.2f;
constexpr float kClearedDelay = 2.5f;
constexpr float kMinSpawnDistance = 380.0f;
constexpr float kGentleOpening = 18.0f;

// Typical group sizes for each kind (min, max).
std::pair<int, int> GroupSize(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Drifter: return {3, 6};
    case EnemyKind::Dart: return {2, 4};
    case EnemyKind::Pinwheel: return {2, 4};
    case EnemyKind::Swarmer: return {6, 11};
    case EnemyKind::Splitter: return {1, 3};
    case EnemyKind::Gunner: return {1, 3};
    case EnemyKind::Shielder: return {1, 2};
    case EnemyKind::Lancer: return {1, 2};
    case EnemyKind::Bomber: return {1, 2};
    default: return {1, 1};
    }
}

Vector2 ClampToArena(Vector2 p, float margin) {
    const Rectangle& a = World::kArena;
    return {std::clamp(p.x, a.x + margin, a.x + a.width - margin),
            std::clamp(p.y, a.y + margin, a.y + a.height - margin)};
}

bool InsideArena(Vector2 p, float margin) {
    const Rectangle& a = World::kArena;
    return p.x >= a.x + margin && p.x <= a.x + a.width - margin && p.y >= a.y + margin &&
           p.y <= a.y + a.height - margin;
}

} // namespace

const SectorModifierInfo& GetSectorModifierInfo(SectorModifier modifier) {
    return kModifiers[static_cast<std::size_t>(modifier)];
}

const char* SectorEventName(SectorEvent event) {
    switch (event) {
    case SectorEvent::Ambush: return "AMBUSH";
    case SectorEvent::Crossfire: return "CROSSFIRE";
    case SectorEvent::DataStorm: return "DATA STORM";
    case SectorEvent::None:
    case SectorEvent::Count: break;
    }
    return "";
}

const char* SectorEventTagline(SectorEvent event) {
    switch (event) {
    case SectorEvent::Ambush: return "HOSTILES CLOSING IN FROM ALL SIDES";
    case SectorEvent::Crossfire: return "DART LINE LOCKING ON  //  MOVE SIDEWAYS";
    case SectorEvent::DataStorm: return "FREE DATA IN THE AREA  //  GRAB IT";
    case SectorEvent::None:
    case SectorEvent::Count: break;
    }
    return "";
}

const SectorInfo& Director::Info() const { return Sectors()[static_cast<std::size_t>(sector_ % kCampaignSectors)]; }

void Director::Start(int sector) {
    sector_ = sector;
    budget_ = 2.0f;
    waveTime_ = 0.0f;
    surgeTimer_ = 38.0f;
    eliteTimer_ = 45.0f / (eliteRate_ * LoopScale(1.5f));
    bossId_ = kInvalidId;
    SetPhase(SectorPhase::Intro);
}

void Director::SetPhase(SectorPhase phase) {
    phase_ = phase;
    phaseTime_ = 0.0f;
    event_ = SectorEvent::None;
}

float Director::WaveProgress() const {
    switch (phase_) {
    case SectorPhase::Intro: return 0.0f;
    case SectorPhase::Waves: return std::clamp(waveTime_ / Info().duration, 0.0f, 1.0f);
    default: return 1.0f;
    }
}

float Director::LoopScale(float perLoop) const {
    // Endless loops compound: the player's build compounds too (levels,
    // stacking overcharge), and linear scaling never caught up with it.
    return std::pow(perLoop, static_cast<float>(Loop()));
}

float Director::EnemyHpMultiplier() const {
    const float loopScale = LoopScale(4.0f); // x4 keeps loop starts at least as tough as the sector before
    const float sectorScale = 1.0f + 0.75f * static_cast<float>(sector_ % kCampaignSectors);
    const float rampWithinSector = 1.0f + 0.8f * WaveProgress();
    return sectorScale * loopScale * rampWithinSector * hpMultiplier_;
}

float Director::EnemySpeedMultiplier() const { return (1.0f + 0.05f * static_cast<float>(sector_)) * speedMultiplier_; }

float Director::CurrentThreatRate() const {
    const float loopScale = LoopScale(1.4f);
    // The very first sector eases in so new players can learn the controls.
    const float warmup = sector_ == 0 ? 0.55f + 0.45f * Saturate(waveTime_ / 25.0f) : 1.0f;
    return Info().threatRate * (1.0f + 1.3f * WaveProgress()) * loopScale * spawnMultiplier_ * warmup;
}

int Director::EnemyCap() const { return std::min(World::kMaxEnemies, 140 + 55 * sector_); }

void Director::Update(World& world, float dt) {
    phaseTime_ += dt;
    switch (phase_) {
    case SectorPhase::Intro:
        if (phaseTime_ >= kIntroDuration) {
            // Boss rush skips the waves and goes straight to the guardian.
            SetPhase(world.Config().mode == GameMode::BossRush ? SectorPhase::Purge : SectorPhase::Waves);
        }
        break;
    case SectorPhase::Waves:
        waveTime_ += dt;
        UpdateWaves(world, dt);
        if (waveTime_ >= Info().duration) {
            SetPhase(SectorPhase::Purge);
        }
        break;
    case SectorPhase::Purge:
        if (phaseTime_ >= kPurgeDuration) {
            world.PurgeEnemies(true);
            world.ClearEnemyBullets(world.GetPlayer().position, 1e6f);
            world.Emit(EventType::BossSpawn, {0.0f, 0.0f}, Info().accentColor, 0.0f);
            SetPhase(SectorPhase::BossWarning);
        }
        break;
    case SectorPhase::BossWarning:
        if (phaseTime_ >= kBossWarningDuration) {
            SpawnBoss(world);
            SetPhase(SectorPhase::Boss);
        }
        break;
    case SectorPhase::Boss: {
        const Enemy* boss = world.FindEnemy(bossId_);
        if (boss == nullptr || !boss->alive) {
            world.OnSectorCleared();
            SetPhase(SectorPhase::Cleared);
        }
        break;
    }
    case SectorPhase::Cleared:
        if (phaseTime_ >= kClearedDelay && sector_ + 1 >= kCampaignSectors &&
            world.Config().mode != GameMode::Endless && Loop() == 0) {
            SetPhase(SectorPhase::Victory);
        }
        break;
    case SectorPhase::Victory: break;
    }
}

void Director::UpdateWaves(World& world, float dt) {
    budget_ += CurrentThreatRate() * world.Tuning().spawnRate * dt;

    eventTime_ += dt;
    surgeTimer_ -= dt;
    if (surgeTimer_ <= 0.0f && waveTime_ < Info().duration - 12.0f) {
        surgeTimer_ = world.Random().Range(34.0f, 44.0f);
        StartEvent(world, PickEvent(world));
    }

    eliteTimer_ -= dt;
    if (eliteTimer_ <= 0.0f) {
        eliteTimer_ = world.Random().Range(42.0f, 55.0f) / (eliteRate_ * LoopScale(1.5f));
        SpawnGroup(world, PickKind(world), true);
    }

    // Spend the budget one group at a time while under the population cap.
    int guard = 0;
    while (budget_ > 0.0f && world.ActiveEnemyCount() < EnemyCap() && guard++ < 4) {
        const EnemyKind kind = PickKind(world);
        const auto [minCount, maxCount] = GroupSize(kind);
        const float groupCost = GetEnemyInfo(kind).threat * static_cast<float>(minCount + maxCount) * 0.5f;
        if (budget_ < groupCost * 0.6f) {
            break;
        }
        SpawnGroup(world, kind, false);
        budget_ -= groupCost;
    }
    // Do not bank an enormous budget while capped.
    budget_ = std::min(budget_, 30.0f);
}

EnemyKind Director::PickKind(World& world) const {
    // The opening seconds of a run only field slow, readable enemies.
    if (sector_ == 0 && waveTime_ < kGentleOpening) {
        return world.Random().Chance(0.7f) ? EnemyKind::Drifter : EnemyKind::Pinwheel;
    }
    Weights weights = Info().weights;
    // Ramp in the tougher kinds over the first part of each sector.
    const float early = std::clamp(waveTime_ / 25.0f, 0.0f, 1.0f);
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const auto kind = static_cast<EnemyKind>(i);
        if (kind != EnemyKind::Drifter && kind != EnemyKind::Pinwheel) {
            weights[i] *= 0.25f + 0.75f * early;
        }
    }
    const int index = world.Random().WeightedIndex(weights);
    return index < 0 ? EnemyKind::Drifter : static_cast<EnemyKind>(index);
}

void Director::SpawnGroup(World& world, EnemyKind kind, bool elite) {
    Rng& rng = world.Random();
    const Rectangle arena = World::kArena;
    const Vector2 playerPos = world.GetPlayer().position;
    auto randomPoint = [&](float margin) {
        for (int attempt = 0; attempt < 16; ++attempt) {
            const Vector2 p{rng.Range(arena.x + margin, arena.x + arena.width - margin),
                            rng.Range(arena.y + margin, arena.y + arena.height - margin)};
            if (DistanceSq(p, playerPos) > kMinSpawnDistance * kMinSpawnDistance) {
                return p;
            }
        }
        // Fall back to the far side of the arena.
        return Vector2{-playerPos.x * 0.8f, -playerPos.y * 0.8f};
    };

    auto [minCount, maxCount] = GroupSize(kind);
    int count = elite ? 1 : rng.RangeInt(minCount, maxCount);

    const int pattern = rng.RangeInt(0, 2);
    if (elite || pattern == 0 || count == 1) {
        // Tight cluster.
        const Vector2 center = randomPoint(120.0f);
        for (int i = 0; i < count; ++i) {
            const Vector2 offset = count == 1 ? Vector2{} : rng.InCircle(40.0f + 12.0f * static_cast<float>(count));
            world.SpawnEnemy(kind, center + offset, 1.0f + rng.Range(0.0f, 0.25f), elite);
        }
    } else if (pattern == 1) {
        // Spread along a random arena edge.
        const int edge = rng.RangeInt(0, 3);
        const float t0 = rng.Range(0.1f, 0.5f);
        for (int i = 0; i < count; ++i) {
            const float t = t0 + static_cast<float>(i) * 0.4f / static_cast<float>(std::max(count, 1));
            Vector2 p{};
            switch (edge) {
            case 0: p = {arena.x + arena.width * t, arena.y + 60.0f}; break;
            case 1: p = {arena.x + arena.width * t, arena.y + arena.height - 60.0f}; break;
            case 2: p = {arena.x + 60.0f, arena.y + arena.height * t}; break;
            default: p = {arena.x + arena.width - 60.0f, arena.y + arena.height * t}; break;
            }
            if (DistanceSq(p, playerPos) < kMinSpawnDistance * kMinSpawnDistance) {
                p = randomPoint(80.0f);
            }
            world.SpawnEnemy(kind, p, 1.0f + 0.05f * static_cast<float>(i));
        }
    } else {
        // Scattered individually.
        for (int i = 0; i < count; ++i) {
            world.SpawnEnemy(kind, randomPoint(80.0f), 0.9f + rng.Range(0.0f, 0.5f));
        }
    }
}

SectorEvent Director::PickEvent(World& world) const {
    // A run's first event is a gift: it introduces events without a threat.
    if (lastEvent_ == SectorEvent::None) {
        return SectorEvent::DataStorm;
    }
    // Weights per event (index = SectorEvent - 1). Crossfire waits for sector 2;
    // the same event never fires twice in a row.
    std::array<float, static_cast<std::size_t>(SectorEvent::Count) - 1> weights{1.0f, sector_ >= 1 ? 1.0f : 0.0f, 0.6f};
    if (lastEvent_ != SectorEvent::None) {
        weights[static_cast<std::size_t>(lastEvent_) - 1] = 0.0f;
    }
    return static_cast<SectorEvent>(world.Random().WeightedIndex(weights) + 1);
}

void Director::StartEvent(World& world, SectorEvent event) {
    event_ = event;
    lastEvent_ = event;
    eventTime_ = 0.0f;
    switch (event) {
    case SectorEvent::Ambush: SpawnAmbush(world); break;
    case SectorEvent::Crossfire: SpawnCrossfire(world); break;
    case SectorEvent::DataStorm: SpawnDataStorm(world); break;
    case SectorEvent::None:
    case SectorEvent::Count: break;
    }
    world.Emit(EventType::SectorEvent, world.GetPlayer().position, Info().accentColor, 1.0f, {},
               static_cast<int>(event));
}

void Director::SpawnAmbush(World& world) {
    Rng& rng = world.Random();
    const Vector2 center = world.GetPlayer().position;
    const EnemyKind kind = sector_ >= 1 && rng.Chance(0.5f) ? EnemyKind::Swarmer : EnemyKind::Drifter;
    const int count = (kind == EnemyKind::Swarmer ? 22 : 14) + 3 * std::min(sector_, 6);
    const float radius = 560.0f;
    const float offset = rng.Angle();
    for (int i = 0; i < count; ++i) {
        const float angle = offset + kTau * static_cast<float>(i) / static_cast<float>(count);
        world.SpawnEnemy(kind, ClampToArena(center + FromAngle(angle, radius), 40.0f), 1.4f);
    }
    world.Emit(EventType::EnemySpawnWarning, center, Info().accentColor, 2.0f);
}

void Director::SpawnCrossfire(World& world) {
    Rng& rng = world.Random();
    const Vector2 player = world.GetPlayer().position;
    constexpr float kDistance = 500.0f;
    // Put the firing line on a side of the player that has room for it.
    Vector2 dir = Normalized(
        Vector2{World::kArena.x + World::kArena.width * 0.5f, World::kArena.y + World::kArena.height * 0.5f} - player);
    for (int attempt = 0; attempt < 8; ++attempt) {
        const Vector2 candidate = FromAngle(rng.Angle());
        const Vector2 lineCenter = player + candidate * kDistance;
        if (InsideArena(lineCenter, 120.0f)) {
            dir = candidate;
            break;
        }
    }
    if (LengthSq(dir) < 0.5f) {
        dir = {1.0f, 0.0f};
    }
    const Vector2 along = Perp(dir);
    const int count = 7 + 2 * std::min(sector_, 4);
    constexpr float kSpacing = 95.0f;
    for (int i = 0; i < count; ++i) {
        const float offset = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * kSpacing;
        // Staggered telegraphs make the line fire as a sweeping volley.
        const float telegraph = 1.3f + 0.06f * static_cast<float>(i);
        world.SpawnEnemy(EnemyKind::Dart, ClampToArena(player + dir * kDistance + along * offset, 40.0f), telegraph);
    }
    world.Emit(EventType::EnemySpawnWarning, player + dir * kDistance, Info().accentColor, 2.0f);
}

void Director::SpawnDataStorm(World& world) {
    Rng& rng = world.Random();
    const Vector2 player = world.GetPlayer().position;
    const int shards = 30 + 4 * std::min(sector_, 5);
    for (int i = 0; i < shards; ++i) {
        const Vector2 p = ClampToArena(player + rng.InCircle(560.0f), 60.0f);
        world.SpawnPickup(PickupKind::Xp, p, rng.Chance(0.15f) ? 5 : 1, 80.0f);
    }
}

void Director::SpawnBoss(World& world) {
    const Enemy& boss = world.SpawnEnemy(Info().boss, {0.0f, -260.0f}, 1.6f);
    bossId_ = boss.id;
}

void Director::AdvanceSector(SectorModifier modifier) {
    switch (modifier) {
    case SectorModifier::Swarm:
        spawnMultiplier_ *= 1.3f;
        xpMultiplier_ *= 1.3f;
        break;
    case SectorModifier::Hardened:
        hpMultiplier_ *= 1.25f;
        scoreMultiplier_ *= 1.4f;
        break;
    case SectorModifier::Bounty: eliteRate_ *= 2.0f; break;
    case SectorModifier::Armory: speedMultiplier_ *= 1.1f; break;
    case SectorModifier::Respite:
    case SectorModifier::Count: break;
    }
    Start(sector_ + 1);
}

void Director::ContinueEndless() { Start(sector_ + 1); }

void Director::JumpToWaves(int sector, float waveTime) {
    Start(sector);
    SetPhase(SectorPhase::Waves);
    waveTime_ = std::clamp(waveTime, 0.0f, Info().duration - 5.0f);
}

void Director::DebugJump(int sector, bool boss) {
    Start(sector);
    if (boss) {
        waveTime_ = Info().duration;
        SetPhase(SectorPhase::BossWarning);
        phaseTime_ = kBossWarningDuration - 0.1f;
    }
}

} // namespace gb
