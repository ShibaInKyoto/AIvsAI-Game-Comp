#include "game/World.h"

#include "game/EnemyCatalog.h"
#include "game/WorldTuning.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace gb {

namespace {

constexpr std::array<DifficultyTuning, static_cast<std::size_t>(Difficulty::Count)> kDifficulty{{
    // hp     speed  spawn  bullets score  bonusHp
    {0.75f, 0.88f, 0.75f, 0.85f, 0.6f, 2},  // Relaxed
    {1.00f, 1.00f, 1.00f, 1.00f, 1.0f, 0},  // Standard
    {1.6f, 1.15f, 1.4f, 1.2f, 1.6f, 0},     // Overclocked
}};

} // namespace

const DifficultyTuning& GetDifficultyTuning(Difficulty difficulty) {
    return kDifficulty[static_cast<std::size_t>(difficulty)];
}

const char* DifficultyName(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Relaxed: return "RELAXED";
    case Difficulty::Standard: return "STANDARD";
    case Difficulty::Overclocked: return "OVERCLOCKED";
    case Difficulty::Count: break;
    }
    return "";
}

int World::XpForLevel(int level) {
    const float l = static_cast<float>(std::max(level, 1) - 1);
    return static_cast<int>(std::round(6.0f + 5.0f * l + 0.35f * l * l));
}

World::World(const RunConfig& config)
    : config_(config), tuning_(GetDifficultyTuning(config.difficulty)), rng_(config.seed) {
    enemyIndex_.Configure(kArena, 96.0f);
    enemies_.reserve(kMaxEnemies);
    bullets_.reserve(kMaxBullets);
    enemyBullets_.reserve(kMaxEnemyBullets);
    pickups_.reserve(kMaxPickups);
    events_.reserve(512);

    baseDamage_ = ShipBaseStats(config.ship).damage;
    const ShipInfo& ship = GetShipInfo(config.ship);
    for (int i = 0; i < ship.startingUpgradeLevel; ++i) {
        inventory_.Add(ship.startingUpgrade);
    }
    RecomputeStats();

    player_.hp = stats_.maxHp;
    player_.dashCharges = stats_.dashCharges;
    player_.shieldCharges = stats_.shieldCharges;
    player_.novaCharge = std::clamp(config.meta.novaCharge, 0.0f, 1.0f);
    rerolls_ = stats_.rerolls;
    xpToNext_ = XpForLevel(1);
    director_.Start(0);
}

void World::RecomputeStats() {
    PlayerStats s = ShipBaseStats(config_.ship);
    s.maxHp += tuning_.bonusHp + config_.meta.bonusHp;
    s.damage *= 1.0f + config_.meta.damage;
    s.xpMultiplier += config_.meta.xp;
    s.magnetRadius *= 1.0f + config_.meta.magnet;
    s.rerolls += config_.meta.rerolls;
    s.dashCooldown *= 1.0f - config_.meta.dashCooldown;

    std::array<int, static_cast<std::size_t>(UpgradeId::Count)> stacks{};
    for (UpgradeId id : inventory_.Order()) {
        const int stack = ++stacks[static_cast<std::size_t>(id)];
        ApplyUpgradeStack(id, stack, s);
    }
    stats_ = s;
    player_.hp = std::min(player_.hp, stats_.maxHp);
    player_.dashCharges = std::min(player_.dashCharges, stats_.dashCharges);

    // Keep the drone wing in sync with the module level.
    static constexpr std::array<int, 6> kDroneCount{0, 1, 2, 2, 3, 3};
    const auto drones = stats_.Evolved(ModuleId::Drones)
                            ? std::size_t{6}
                            : static_cast<std::size_t>(kDroneCount[static_cast<std::size_t>(
                                  std::clamp(stats_.Module(ModuleId::Drones), 0, 5))]);
    while (drones_.size() < drones) {
        Drone d;
        d.position = player_.position;
        d.cooldown = 0.2f * static_cast<float>(drones_.size());
        drones_.push_back(d);
    }
    drones_.resize(drones);
}

float World::ModulePower() const {
    const float damageScale = stats_.damage / std::max(baseDamage_, 1.0f);
    const float levelScale = 1.0f + 0.03f * static_cast<float>(level_ - 1);
    return stats_.powerMultiplier * damageScale * levelScale;
}

void World::ApplyUpgrade(UpgradeId id) {
    static_assert(static_cast<int>(UpgradeId::Count) <= 64, "upgradesTaken is a 64-bit mask");
    inventory_.Add(id);
    runStats_.upgradesTaken |= std::uint64_t{1} << static_cast<unsigned>(id);
    RecomputeStats();
    switch (id) {
    case UpgradeId::HullPlating: player_.hp = std::min(player_.hp + 1, stats_.maxHp); break;
    case UpgradeId::NanoRepair: player_.hp = std::min(player_.hp + 2, stats_.maxHp); break;
    case UpgradeId::BackupDrive: rerolls_ += 2; break;
    case UpgradeId::Deflector: player_.shieldCharges = stats_.shieldCharges; break;
    case UpgradeId::ExtraDash: player_.dashCharges = std::min(player_.dashCharges + 1, stats_.dashCharges); break;
    default: break;
    }
}

std::vector<UpgradeId> World::RollOffers(int count, std::span<const UpgradeId> banned, float luckBonus) {
    PlayerStats rollStats = stats_;
    rollStats.luck += luckBonus;
    return inventory_.RollOffers(rng_, count, rollStats, player_.hp, banned);
}

bool World::UseReroll() {
    if (rerolls_ <= 0) {
        return false;
    }
    --rerolls_;
    return true;
}

int World::ComboMultiplier() const { return std::min(10, 1 + combo_ / 30); }

void World::AddScore(long long amount) { runStats_.score += amount; }

void World::OnSectorCleared() {
    runStats_.sectorsCleared++;
    if (runStats_.hitsTaken == hitsAtSectorStart_) {
        runStats_.flawlessSectors++;
    }
    pendingCaches_++;
    // Hoover up every remaining shard so nothing is lost to the transition.
    for (Pickup& p : pickups_) {
        p.magnetized = true;
    }
    Emit(EventType::SectorCleared, player_.position, director_.Info().accentColor);
}

void World::ChooseSectorModifier(SectorModifier modifier) {
    switch (modifier) {
    case SectorModifier::Bounty:
    case SectorModifier::Armory: pendingCaches_++; break;
    case SectorModifier::Respite: player_.hp = std::min(player_.hp + 2, stats_.maxHp); break;
    default: break;
    }
    director_.AdvanceSector(modifier);
    hitsAtSectorStart_ = runStats_.hitsTaken;
}

void World::ContinueEndless() {
    director_.ContinueEndless();
    hitsAtSectorStart_ = runStats_.hitsTaken;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void World::Update(float dt, const PlayerInput& input) {
    const int steps = std::max(1, static_cast<int>(std::ceil(dt / kMaxStep)));
    const float step = dt / static_cast<float>(steps);
    PlayerInput stepInput = input;
    for (int i = 0; i < steps; ++i) {
        Step(step, stepInput);
        // Edge-triggered actions only fire on the first substep.
        stepInput.dash = false;
        stepInput.nova = false;
    }
}

void World::Step(float dt, const PlayerInput& input) {
    if (player_.alive) {
        runStats_.time += dt;
    }
    director_.Update(*this, dt);

    UpdatePlayer(dt, input);
    UpdateEnemies(dt);
    FlushSpawns();
    RebuildSpatialIndex();
    ResolveEnemySeparation();
    ApplyGravityWells(dt);

    UpdateModules(dt);
    UpdateBullets(dt);
    UpdateEnemyBullets(dt);
    UpdateBeams(dt);
    UpdateShockwaves(dt);
    HandlePlayerContacts();
    UpdatePickups(dt);
    UpdateTraces(dt);

    if (comboTimer_ > 0.0f) {
        comboTimer_ -= dt;
        if (comboTimer_ <= 0.0f) {
            combo_ = 0;
        }
    }

    Compact();
    FlushSpawns();
    RebuildSpatialIndex();
}

void World::FlushSpawns() {
    while (!pendingSpawns_.empty()) {
        if (static_cast<int>(enemies_.size()) < kMaxEnemies || IsBoss(pendingSpawns_.front().kind)) {
            enemies_.push_back(pendingSpawns_.front());
        }
        pendingSpawns_.pop_front();
    }
}

void World::RebuildSpatialIndex() {
    activeEnemies_ = 0;
    idToIndex_.clear();
    for (int i = 0; i < static_cast<int>(enemies_.size()); ++i) {
        const Enemy& e = enemies_[static_cast<std::size_t>(i)];
        idToIndex_[e.id] = i;
        if (e.alive && e.countsForClear) {
            ++activeEnemies_;
        }
    }
    enemyIndex_.Build(static_cast<int>(enemies_.size()), [this](int i, Vector2& position, float& radius) {
        const Enemy& e = enemies_[static_cast<std::size_t>(i)];
        if (!e.Active()) {
            return false;
        }
        position = e.position;
        radius = e.radius;
        return true;
    });
}

void World::Compact() {
    auto eraseDead = [](auto& container) {
        container.erase(std::remove_if(container.begin(), container.end(), [](const auto& x) { return !x.alive; }),
                        container.end());
    };
    // Enemies keep their relative order so serpent segments update after their leader.
    eraseDead(enemies_);
    eraseDead(bullets_);
    eraseDead(enemyBullets_);
    eraseDead(beams_);
    eraseDead(pickups_);
    eraseDead(missiles_);
    eraseDead(playerMines_);
    eraseDead(shockwaves_);
}

Enemy* World::FindEnemy(EntityId id) {
    if (id == kInvalidId) {
        return nullptr;
    }
    const auto it = idToIndex_.find(id);
    if (it != idToIndex_.end() && it->second < static_cast<int>(enemies_.size())) {
        Enemy& e = enemies_[static_cast<std::size_t>(it->second)];
        if (e.id == id) {
            return &e;
        }
    }
    for (Enemy& e : pendingSpawns_) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

Enemy* World::NearestEnemy(Vector2 position, float maxRange, EntityId exclude) {
    Enemy* best = nullptr;
    float bestDistSq = maxRange * maxRange;
    enemyIndex_.Query(position, maxRange, [&](int index) {
        Enemy& e = enemies_[static_cast<std::size_t>(index)];
        if (!e.Active() || e.id == exclude || e.invulnerable || e.kind == EnemyKind::Mine) {
            return;
        }
        const float d = DistanceSq(e.position, position);
        if (d < bestDistSq) {
            bestDistSq = d;
            best = &e;
        }
    });
    return best;
}

void World::Emit(EventType type, Vector2 position, Color color, float magnitude, Vector2 direction, int value,
                 EnemyKind enemy, EntityId source) {
    events_.push_back(GameEvent{type, position, direction, color, magnitude, value, enemy, source});
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

Enemy& World::SpawnEnemy(EnemyKind kind, Vector2 position, float telegraph, bool elite) {
    static_assert(static_cast<int>(EnemyKind::Count) <= 32, "enemiesSeen is a 32-bit mask");
    const EnemyInfo& info = GetEnemyInfo(kind);
    runStats_.enemiesSeen |= 1u << static_cast<unsigned>(kind);
    Enemy e;
    e.id = nextId_++;
    e.kind = kind;
    e.position = {std::clamp(position.x, kArena.x + 20.0f, kArena.x + kArena.width - 20.0f),
                  std::clamp(position.y, kArena.y + 20.0f, kArena.y + kArena.height - 20.0f)};
    e.radius = info.radius * (elite ? 1.45f : 1.0f);
    const float hpScale = director_.EnemyHpMultiplier() * tuning_.enemyHp * (elite ? 7.0f : 1.0f);
    e.maxHp = e.hp = info.hp * (IsBoss(kind) ? (1.0f + 0.9f * static_cast<float>(director_.Loop())) *
                                                   tuning_.enemyHp
                                             : hpScale);
    e.speed = info.speed;
    e.spawnDelay = telegraph;
    e.elite = elite;
    e.angle = rng_.Angle();
    e.timer = rng_.Range(0.0f, 1.0f);
    if (kind == EnemyKind::Mine) {
        e.countsForClear = false;
        e.dropsLoot = false;
    }
    if (kind == EnemyKind::WeaverBody) {
        e.invulnerable = true;
        e.dropsLoot = false;
        e.countsForClear = false;
    }
    if (telegraph > 0.0f && kind != EnemyKind::WeaverBody) {
        Emit(EventType::EnemySpawnWarning, e.position, info.color, elite ? 1.5f : 1.0f, {}, 0, kind);
    }
    pendingSpawns_.push_back(e);
    return pendingSpawns_.back();
}

EnemyBullet& World::FireEnemyBullet(Vector2 position, Vector2 velocity, EnemyBulletStyle style, Color color,
                                    float radius) {
    if (static_cast<int>(enemyBullets_.size()) >= kMaxEnemyBullets) {
        enemyBullets_.erase(enemyBullets_.begin());
    }
    EnemyBullet b;
    b.position = position;
    b.velocity = velocity * tuning_.bulletSpeed;
    b.style = style;
    b.source = actingEnemy_;
    b.color = color;
    b.radius = radius;
    enemyBullets_.push_back(b);
    return enemyBullets_.back();
}

Beam& World::AddBeam(Vector2 origin, float angle, float warmup, float duration, float width, Color color,
                     EntityId owner) {
    Beam b;
    b.origin = origin;
    b.angle = angle;
    b.warmup = warmup;
    b.duration = duration;
    b.width = width;
    b.color = color;
    b.owner = owner;
    b.source = actingEnemy_;
    b.followOwner = owner != kInvalidId;
    beams_.push_back(b);
    Emit(EventType::LaserCharge, origin, color, warmup);
    return beams_.back();
}

Shockwave& World::AddShockwave(Vector2 position, float maxRadius, float speed, float damage, float knockback,
                               bool clearsBullets) {
    Shockwave s;
    s.position = position;
    s.maxRadius = maxRadius;
    s.speed = speed;
    s.damage = damage;
    s.knockback = knockback;
    s.clearsBullets = clearsBullets;
    shockwaves_.push_back(std::move(s));
    return shockwaves_.back();
}

void World::UpdatePickups(float dt) {
    Player& p = player_;
    const float magnetSq = stats_.magnetRadius * stats_.magnetRadius;
    for (Pickup& pickup : pickups_) {
        if (!pickup.alive) {
            continue;
        }
        pickup.age += dt;
        pickup.life -= dt;
        if (pickup.life <= 0.0f) {
            pickup.alive = false;
            continue;
        }
        const Vector2 toPlayer = p.position - pickup.position;
        const float distSq = LengthSq(toPlayer);
        // Shards only become collectable after their initial pop-out; old
        // shards home in on their own so nothing is lost to clutter.
        const bool expiring = pickup.kind == PickupKind::Xp && pickup.age > kShardAutoCollectAge;
        if (p.alive && pickup.age > 0.25f && (pickup.magnetized || expiring || distSq < magnetSq)) {
            pickup.magnetized = true;
            const float speed = std::max(Length(pickup.velocity), 350.0f) + 2600.0f * dt;
            pickup.velocity = Normalized(toPlayer) * std::min(speed, 1800.0f);
        } else {
            pickup.velocity *= std::exp(-4.0f * dt);
        }
        pickup.position += pickup.velocity * dt;
        pickup.position.x = std::clamp(pickup.position.x, kArena.x + 8.0f, kArena.x + kArena.width - 8.0f);
        pickup.position.y = std::clamp(pickup.position.y, kArena.y + 8.0f, kArena.y + kArena.height - 8.0f);

        const float collect = p.radius + kPickupCollectRadius;
        if (p.alive && pickup.age > 0.25f && distSq < collect * collect) {
            pickup.alive = false;
            switch (pickup.kind) {
            case PickupKind::Xp: {
                const float mult = stats_.xpMultiplier * director_.XpMultiplier();
                GainXp(std::max(1, static_cast<int>(std::round(static_cast<float>(pickup.value) * mult))));
                runStats_.shardsCollected += pickup.value;
                Emit(EventType::PickupXp, pickup.position, {90, 255, 180, 255}, static_cast<float>(pickup.value));
                break;
            }
            case PickupKind::Health:
                p.hp = std::min(p.hp + 1, stats_.maxHp);
                Emit(EventType::PickupHealth, pickup.position, {110, 255, 150, 255});
                break;
            case PickupKind::Cache:
                pendingCaches_++;
                Emit(EventType::PickupCache, pickup.position, {255, 200, 70, 255});
                break;
            case PickupKind::Magnet:
                for (Pickup& other : pickups_) {
                    if (other.kind == PickupKind::Xp) {
                        other.magnetized = true;
                    }
                }
                Emit(EventType::PickupMagnet, pickup.position, {120, 200, 255, 255});
                break;
            }
        }
    }
}

void World::UpdateTraces(float dt) {
    for (ArcTrace& a : arcs_) {
        a.life -= dt;
    }
    arcs_.erase(std::remove_if(arcs_.begin(), arcs_.end(), [](const ArcTrace& a) { return a.life <= 0.0f; }),
                arcs_.end());
    for (LanceTrace& l : lances_) {
        l.life -= dt;
    }
    lances_.erase(std::remove_if(lances_.begin(), lances_.end(), [](const LanceTrace& l) { return l.life <= 0.0f; }),
                  lances_.end());
}

void World::ApplyGravityWells(float dt) {
    for (const Enemy& e : enemies_) {
        if (!e.Active() || (e.kind != EnemyKind::Singularity && e.kind != EnemyKind::BossSingularity)) {
            continue;
        }
        const bool boss = e.kind == EnemyKind::BossSingularity;
        const float range = boss ? 1100.0f : 520.0f;
        const float pull = boss ? 230.0f : 190.0f;
        if (player_.alive && !player_.Dashing()) {
            const Vector2 d = e.position - player_.position;
            const float dist = Length(d);
            if (dist < range && dist > 1.0f) {
                player_.position += d / dist * (pull * (1.0f - dist / range) * dt);
            }
        }
        for (Bullet& b : bullets_) {
            const Vector2 d = e.position - b.position;
            const float distSq = LengthSq(d);
            if (distSq < range * range && distSq > 1.0f) {
                const float dist = std::sqrt(distSq);
                b.velocity += d / dist * (2400.0f * (1.0f - dist / range) * dt);
            }
        }
    }
}

int World::OrbitBladeCount() const {
    if (stats_.Evolved(ModuleId::OrbitBlades)) {
        return 12;
    }
    static constexpr std::array<int, 6> kBlades{0, 2, 3, 3, 4, 6};
    return kBlades[static_cast<std::size_t>(std::clamp(stats_.Module(ModuleId::OrbitBlades), 0, 5))];
}

float World::OrbitRadius() const {
    if (stats_.Evolved(ModuleId::OrbitBlades)) {
        // The saw storm breathes in and out.
        return 150.0f + 55.0f * std::sin(orbitAngle_ * 0.35f);
    }
    return stats_.Module(ModuleId::OrbitBlades) >= 3 ? 125.0f : 100.0f;
}

} // namespace gb
