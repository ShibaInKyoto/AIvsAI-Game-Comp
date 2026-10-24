#include "game/World.h"

#include "game/EnemyCatalog.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace gb {

namespace {

constexpr std::array<DifficultyTuning, static_cast<std::size_t>(Difficulty::Count)> kDifficulty{{
    // hp     speed  spawn  bullets score  bonusHp
    {0.75f, 0.88f, 0.75f, 0.85f, 0.6f, 2},  // Relaxed
    {1.00f, 1.00f, 1.00f, 1.00f, 1.0f, 0},  // Standard
    {1.35f, 1.12f, 1.25f, 1.15f, 1.6f, 0},  // Overclocked
}};

constexpr float kComboWindow = 3.0f;
constexpr int kMaxBullets = 3000;
constexpr int kMaxEnemyBullets = 2500;
constexpr int kMaxPickups = 600;
constexpr float kPickupCollectRadius = 16.0f;
constexpr float kBulletHitboxScale = 0.55f;  // forgiving: bullets must hit the ship's core
constexpr float kBodyHitboxScale = 0.75f;
constexpr Color kPlayerBulletColor{255, 240, 140, 255};

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
    const auto drones = static_cast<std::size_t>(kDroneCount[static_cast<std::size_t>(
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
    inventory_.Add(id);
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

std::vector<UpgradeId> World::RollOffers(int count, std::span<const UpgradeId> banned) {
    return inventory_.RollOffers(rng_, count, stats_, player_.hp, banned);
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
}

void World::ContinueEndless() { director_.ContinueEndless(); }

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
                 EnemyKind enemy) {
    events_.push_back(GameEvent{type, position, direction, color, magnitude, value, enemy});
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

Enemy& World::SpawnEnemy(EnemyKind kind, Vector2 position, float telegraph, bool elite) {
    const EnemyInfo& info = GetEnemyInfo(kind);
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

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

void World::UpdatePlayer(float dt, const PlayerInput& input) {
    Player& p = player_;
    if (!p.alive) {
        p.velocity = {};
        return;
    }
    p.invulnerable = std::max(0.0f, p.invulnerable - dt);
    p.hitFlash = std::max(0.0f, p.hitFlash - dt * 3.0f);
    p.recoil = std::max(0.0f, p.recoil - dt * 12.0f);

    if (p.dashCharges < stats_.dashCharges) {
        p.dashRecharge += dt;
        if (p.dashRecharge >= stats_.dashCooldown) {
            p.dashCharges++;
            p.dashRecharge = 0.0f;
        }
    }
    if (stats_.shieldCharges > 0 && p.shieldCharges < stats_.shieldCharges) {
        p.shieldTimer -= dt;
        if (p.shieldTimer <= 0.0f) {
            p.shieldCharges = stats_.shieldCharges;
        }
    }
    if (stats_.regenInterval > 0.0f && p.hp < stats_.maxHp) {
        p.regenTimer += dt;
        if (p.regenTimer >= stats_.regenInterval) {
            p.regenTimer = 0.0f;
            p.hp++;
            Emit(EventType::PickupHealth, p.position, {110, 255, 150, 255});
        }
    }

    if (LengthSq(input.aim) > 0.0001f) {
        p.aimAngle = AngleOf(input.aim);
    }
    const Vector2 aimDir = FromAngle(p.aimAngle);

    if (input.dash && p.dashCharges > 0 && !p.Dashing()) {
        p.dashDirection = LengthSq(input.move) > 0.01f ? Normalized(input.move) : aimDir;
        p.dashTime = stats_.dashDuration;
        p.dashCharges--;
        Emit(EventType::PlayerDash, p.position, {140, 220, 255, 255}, 1.0f, p.dashDirection);
    }

    if (p.Dashing()) {
        p.dashTime -= dt;
        p.velocity = p.dashDirection * stats_.dashSpeed;
        if (!p.Dashing()) {
            // Carry some momentum out of the dash, and a sliver of grace time.
            p.velocity = p.dashDirection * stats_.moveSpeed;
            p.invulnerable = std::max(p.invulnerable, 0.08f);
        }
    } else {
        const Vector2 desired = ClampLength(input.move, 1.0f) * stats_.moveSpeed;
        p.velocity = DampV(p.velocity, desired, 0.035f, dt);
    }

    p.position += p.velocity * dt;
    p.position.x = std::clamp(p.position.x, kArena.x + p.radius, kArena.x + kArena.width - p.radius);
    p.position.y = std::clamp(p.position.y, kArena.y + p.radius, kArena.y + kArena.height - p.radius);

    p.fireCooldown -= dt;
    if (input.fire && p.fireCooldown <= 0.0f) {
        FireMainGun();
        p.fireCooldown = std::max(p.fireCooldown + 1.0f / stats_.fireRate, 0.0f);
    } else if (!input.fire) {
        p.fireCooldown = std::max(p.fireCooldown, 0.0f);
    }

    if (input.nova && p.novaCharge >= 1.0f) {
        const bool overdrive = inventory_.Stacks(UpgradeId::OverdriveCore) > 0;
        const float damage = (70.0f + 10.0f * static_cast<float>(level_)) * ModulePower() * (overdrive ? 1.5f : 1.0f);
        AddShockwave(p.position, overdrive ? 1300.0f : 1000.0f, 1900.0f, damage, 1200.0f, true);
        p.novaCharge = 0.0f;
        p.invulnerable = std::max(p.invulnerable, 0.6f);
        Emit(EventType::NovaFired, p.position, {200, 240, 255, 255}, overdrive ? 1.3f : 1.0f);
    }
}

void World::FireMainGun() {
    Player& p = player_;
    const float base = p.aimAngle;
    const Vector2 dir = FromAngle(base);
    const Vector2 side = Perp(dir);
    const Vector2 muzzle = p.position + dir * 20.0f;
    const int n = std::max(1, stats_.projectiles);

    for (int i = 0; i < n; ++i) {
        const float k = static_cast<float>(i) - static_cast<float>(n - 1) * 0.5f;
        const float jitter = rng_.Range(-stats_.inaccuracy, stats_.inaccuracy);
        SpawnPlayerBullet(muzzle + side * (k * 7.0f), base + k * stats_.spread + jitter, 1.0f);
    }
    for (int i = 0; i < stats_.rearGuns; ++i) {
        const float k = static_cast<float>(i) - static_cast<float>(stats_.rearGuns - 1) * 0.5f;
        SpawnPlayerBullet(p.position - dir * 12.0f, base + kPi + k * 0.18f, 0.9f);
    }
    for (int i = 0; i < stats_.sideGuns; ++i) {
        const float offset = 0.12f * static_cast<float>(i);
        SpawnPlayerBullet(p.position + side * 10.0f, base + kPi * 0.5f - offset, 0.9f);
        SpawnPlayerBullet(p.position - side * 10.0f, base - kPi * 0.5f + offset, 0.9f);
    }
    p.recoil = 1.0f;
    p.muzzleSide = -p.muzzleSide;
    Emit(EventType::PlayerShot, muzzle, kPlayerBulletColor, static_cast<float>(n), dir);
}

void World::SpawnPlayerBullet(Vector2 origin, float angle, float speedScale) {
    if (static_cast<int>(bullets_.size()) >= kMaxBullets) {
        return;
    }
    Bullet b;
    b.position = origin;
    b.velocity = FromAngle(angle, stats_.bulletSpeed * speedScale);
    b.radius = 5.0f * stats_.bulletSize;
    b.damage = stats_.damage;
    b.crit = rng_.Chance(stats_.critChance);
    if (b.crit) {
        b.damage *= stats_.critMultiplier;
    }
    b.life = stats_.bulletLifetime;
    b.pierce = stats_.pierce;
    b.bounce = stats_.bounce;
    b.homing = stats_.homing;
    b.explosionRadius = stats_.explosionRadius;
    b.slowChance = stats_.slowChance;
    bullets_.push_back(b);
}

void World::DamagePlayer(Vector2 source) {
    Player& p = player_;
    if (!p.alive || p.invulnerable > 0.0f || p.Dashing()) {
        return;
    }
    const Vector2 away = Normalized(p.position - source, FromAngle(p.aimAngle + kPi));

    // Being hit always buys breathing room: nearby bullets vanish and enemies
    // are shoved back so a single mistake cannot chain into several.
    auto repel = [&](float radius, float force) {
        ClearEnemyBullets(p.position, radius);
        enemyIndex_.Query(p.position, radius, [&](int index) {
            Enemy& e = enemies_[static_cast<std::size_t>(index)];
            if (!e.Active() || IsBoss(e.kind)) {
                return;
            }
            const Vector2 d = e.position - p.position;
            if (LengthSq(d) < radius * radius) {
                e.knockback += Normalized(d) * (force / GetEnemyInfo(e.kind).mass);
            }
        });
    };

    if (p.shieldCharges > 0) {
        p.shieldCharges--;
        p.shieldTimer = stats_.shieldRecharge;
        p.invulnerable = 0.8f;
        repel(180.0f, 600.0f);
        Emit(EventType::ShieldBlock, p.position, {120, 200, 255, 255}, 1.0f, away);
        return;
    }

    p.hp--;
    p.invulnerable = stats_.invulnerability;
    p.hitFlash = 1.0f;
    runStats_.hitsTaken++;
    combo_ = 0;
    comboTimer_ = 0.0f;
    repel(260.0f, 900.0f);
    if (stats_.thornsDamage > 0.0f) {
        AddShockwave(p.position, 360.0f, 1500.0f, stats_.thornsDamage * ModulePower(), 800.0f, true);
    }
    Emit(EventType::PlayerHit, p.position, {255, 70, 70, 255}, static_cast<float>(p.hp), away);
    if (p.hp <= 0) {
        p.alive = false;
        p.velocity = {};
        Emit(EventType::PlayerDeath, p.position, {255, 255, 255, 255}, 1.0f);
    }
}

void World::HandlePlayerContacts() {
    Player& p = player_;
    if (!p.alive) {
        return;
    }
    const bool dashing = p.Dashing();
    enemyIndex_.Query(p.position, p.radius + 20.0f, [&](int index) {
        Enemy& e = enemies_[static_cast<std::size_t>(index)];
        if (!e.Active()) {
            return;
        }
        const float reach = e.radius * kBodyHitboxScale + p.radius * kBodyHitboxScale;
        const float distSq = DistanceSq(e.position, p.position);
        if (dashing && stats_.dashDamage > 0.0f && !e.invulnerable && e.dashHitCooldown <= 0.0f &&
            distSq < (e.radius + p.radius + 14.0f) * (e.radius + p.radius + 14.0f)) {
            e.dashHitCooldown = 0.45f;
            DamageEnemy(e, stats_.dashDamage * ModulePower(), true, p.dashDirection, 400.0f);
            return;
        }
        if (e.contactDamage && distSq < reach * reach) {
            DamagePlayer(e.position);
        }
    });
}

// ---------------------------------------------------------------------------
// Projectiles
// ---------------------------------------------------------------------------

void World::UpdateBullets(float dt) {
    const float right = kArena.x + kArena.width;
    const float bottom = kArena.y + kArena.height;
    for (Bullet& b : bullets_) {
        if (!b.alive) {
            continue;
        }
        b.life -= dt;
        if (b.life <= 0.0f) {
            b.alive = false;
            continue;
        }
        if (b.homing > 0.0f) {
            if (Enemy* target = NearestEnemy(b.position, 420.0f)) {
                const float current = AngleOf(b.velocity);
                const float desired = AngleOf(target->position - b.position);
                b.velocity = FromAngle(RotateToward(current, desired, b.homing * dt), Length(b.velocity));
            }
        }
        b.position += b.velocity * dt;
        if (b.position.x < kArena.x || b.position.x > right || b.position.y < kArena.y || b.position.y > bottom) {
            b.alive = false;
            const Vector2 clamped{std::clamp(b.position.x, kArena.x, right), std::clamp(b.position.y, kArena.y, bottom)};
            Emit(EventType::BulletWallHit, clamped, kPlayerBulletColor, 1.0f, b.velocity);
            continue;
        }
        enemyIndex_.Query(b.position, b.radius, [&](int index) {
            if (!b.alive) {
                return;
            }
            Enemy& e = enemies_[static_cast<std::size_t>(index)];
            if (!e.Active() || b.RecentlyHit(e.id)) {
                return;
            }
            const float r = e.radius + b.radius;
            if (DistanceSq(e.position, b.position) < r * r) {
                OnBulletHit(b, e);
            }
        });
    }
}

void World::OnBulletHit(Bullet& b, Enemy& e) {
    b.RememberHit(e.id);
    const Vector2 dir = Normalized(b.velocity);

    if (e.kind == EnemyKind::Shielder) {
        // The shield covers a 130 degree arc facing the enemy's heading.
        const Vector2 facing = FromAngle(e.angle);
        if (Dot(facing, Normalized(b.position - e.position)) > std::cos(1.13f)) {
            b.alive = false;
            Emit(EventType::EnemyShieldBlock, b.position, GetEnemyInfo(e.kind).color, 1.0f, -dir);
            return;
        }
    }
    if (e.invulnerable) {
        b.alive = false;
        Emit(EventType::EnemyShieldBlock, b.position, GetEnemyInfo(e.kind).color, 0.6f, -dir);
        return;
    }

    DamageEnemy(e, b.damage, b.crit, dir, 45.0f);
    if (b.slowChance > 0.0f && rng_.Chance(b.slowChance)) {
        e.slowTimer = 2.2f;
    }
    if (b.explosionRadius > 0.0f) {
        Explode(b.position, b.explosionRadius, b.damage * 0.4f, e.id, {255, 170, 80, 255});
    }
    if (b.bounce > 0) {
        Enemy* next = nullptr;
        float best = 460.0f * 460.0f;
        enemyIndex_.Query(b.position, 460.0f, [&](int index) {
            Enemy& candidate = enemies_[static_cast<std::size_t>(index)];
            if (!candidate.Active() || candidate.invulnerable || b.RecentlyHit(candidate.id) ||
                candidate.kind == EnemyKind::Mine) {
                return;
            }
            const float d = DistanceSq(candidate.position, b.position);
            if (d < best) {
                best = d;
                next = &candidate;
            }
        });
        if (next != nullptr) {
            b.velocity = Normalized(next->position - b.position) * Length(b.velocity);
            b.bounce--;
            b.life = std::max(b.life, 0.45f);
            return;
        }
    }
    if (b.pierce > 0) {
        b.pierce--;
        return;
    }
    b.alive = false;
}

void World::Explode(Vector2 position, float radius, float damage, EntityId exclude, Color color) {
    enemyIndex_.Query(position, radius, [&](int index) {
        Enemy& e = enemies_[static_cast<std::size_t>(index)];
        if (!e.Active() || e.id == exclude) {
            return;
        }
        const Vector2 d = e.position - position;
        const float reach = radius + e.radius;
        if (LengthSq(d) < reach * reach) {
            DamageEnemy(e, damage, false, Normalized(d), 120.0f);
        }
    });
    Emit(EventType::Explosion, position, color, radius);
}

void World::UpdateEnemyBullets(float dt) {
    Player& p = player_;
    const bool vulnerable = p.alive && p.invulnerable <= 0.0f && !p.Dashing();
    for (EnemyBullet& b : enemyBullets_) {
        if (!b.alive) {
            continue;
        }
        b.age += dt;
        b.life -= dt;
        if (b.turn != 0.0f) {
            b.velocity = Rotated(b.velocity, b.turn * dt);
        }
        if (b.acceleration != 0.0f) {
            const float speed = std::max(40.0f, Length(b.velocity) + b.acceleration * dt);
            b.velocity = Normalized(b.velocity) * speed;
        }
        b.position += b.velocity * dt;
        const float margin = 60.0f;
        if (b.life <= 0.0f || b.position.x < kArena.x - margin || b.position.x > kArena.x + kArena.width + margin ||
            b.position.y < kArena.y - margin || b.position.y > kArena.y + kArena.height + margin) {
            b.alive = false;
            continue;
        }
        if (vulnerable) {
            const float reach = b.radius * 0.8f + p.radius * kBulletHitboxScale;
            if (DistanceSq(b.position, p.position) < reach * reach) {
                b.alive = false;
                DamagePlayer(b.position);
            }
        }
    }
}

void World::ClearEnemyBullets(Vector2 center, float radius) {
    for (EnemyBullet& b : enemyBullets_) {
        if (b.alive && DistanceSq(b.position, center) < radius * radius) {
            b.alive = false;
            Emit(EventType::EnemyShieldBlock, b.position, b.color, 0.3f);
        }
    }
}

void World::UpdateBeams(float dt) {
    Player& p = player_;
    for (Beam& beam : beams_) {
        if (!beam.alive) {
            continue;
        }
        if (beam.followOwner) {
            const Enemy* owner = FindEnemy(beam.owner);
            if (owner == nullptr || !owner->alive) {
                beam.alive = false;
                continue;
            }
            beam.origin = owner->position;
        }
        if (beam.warmup > 0.0f) {
            beam.warmup -= dt;
            if (beam.warmup <= 0.0f) {
                Emit(EventType::LaserFire, beam.origin, beam.color, 1.0f, FromAngle(beam.angle));
            }
            continue;
        }
        beam.duration -= dt;
        beam.angle += beam.sweep * dt;
        if (beam.duration <= 0.0f) {
            beam.alive = false;
            continue;
        }
        if (p.alive && p.invulnerable <= 0.0f && !p.Dashing()) {
            const Vector2 end = beam.origin + FromAngle(beam.angle, beam.length);
            const Vector2 closest = [&] {
                const Vector2 ab = end - beam.origin;
                const float t = Saturate(Dot(p.position - beam.origin, ab) / std::max(LengthSq(ab), 1.0f));
                return beam.origin + ab * t;
            }();
            const float reach = beam.width * 0.45f + p.radius * kBulletHitboxScale;
            if (DistanceSq(closest, p.position) < reach * reach) {
                DamagePlayer(closest);
            }
        }
    }
}

void World::UpdateShockwaves(float dt) {
    for (Shockwave& s : shockwaves_) {
        if (!s.alive) {
            continue;
        }
        s.radius += s.speed * dt;
        if (s.radius >= s.maxRadius) {
            s.radius = s.maxRadius;
            s.alive = false;
        }
        enemyIndex_.Query(s.position, s.radius, [&](int index) {
            Enemy& e = enemies_[static_cast<std::size_t>(index)];
            if (!e.Active() || std::find(s.hit.begin(), s.hit.end(), e.id) != s.hit.end()) {
                return;
            }
            const Vector2 d = e.position - s.position;
            if (LengthSq(d) < (s.radius + e.radius) * (s.radius + e.radius)) {
                s.hit.push_back(e.id);
                DamageEnemy(e, s.damage, false, Normalized(d), s.knockback);
            }
        });
        if (s.clearsBullets) {
            ClearEnemyBullets(s.position, s.radius);
        }
    }
}

// ---------------------------------------------------------------------------
// Damage, death and loot
// ---------------------------------------------------------------------------

bool World::DamageEnemy(Enemy& e, float amount, bool crit, Vector2 direction, float knockback) {
    if (!e.Active() || e.invulnerable || amount <= 0.0f) {
        return false;
    }
    runStats_.damageDealt += std::min(amount, e.hp);
    e.hp -= amount;
    e.hitFlash = 1.0f;
    e.knockback += direction * (knockback / GetEnemyInfo(e.kind).mass);
    Emit(EventType::EnemyHit, e.position, GetEnemyInfo(e.kind).color, amount, direction, crit ? 1 : 0, e.kind);
    if (e.hp <= 0.0f) {
        KillEnemy(e, true);
        return true;
    }
    return false;
}

void World::KillEnemy(Enemy& e, bool dropLoot) {
    if (!e.alive) {
        return;
    }
    e.alive = false;
    const EnemyInfo& info = GetEnemyInfo(e.kind);
    if (dropLoot && e.dropsLoot) {
        DropLoot(e);
    }
    if (e.countsForClear) {
        runStats_.kills++;
        combo_++;
        comboTimer_ = kComboWindow;
        runStats_.bestCombo = std::max(runStats_.bestCombo, combo_);
        if (combo_ % 50 == 0) {
            Emit(EventType::ComboMilestone, player_.position, {255, 220, 100, 255}, static_cast<float>(combo_));
        }
        const float scoreScale = tuning_.scoreMultiplier * director_.ScoreMultiplier() * (e.elite ? 4.0f : 1.0f);
        AddScore(static_cast<long long>(static_cast<float>(info.score * ComboMultiplier()) * scoreScale));
        player_.novaCharge = std::min(1.0f, player_.novaCharge + 0.0085f * std::max(info.threat, 0.4f) *
                                                                     stats_.novaChargeRate * (e.elite ? 6.0f : 1.0f));
    }
    if (e.elite) {
        runStats_.elitesKilled++;
    }
    if (IsBoss(e.kind)) {
        runStats_.bossesKilled++;
        Emit(EventType::BossDefeated, e.position, info.color, e.radius, {}, 0, e.kind);
    }
    Emit(EventType::EnemyKilled, e.position, info.color, e.radius, e.velocity + e.knockback, e.elite ? 1 : 0, e.kind);
    OnEnemyDeath(e);
}

void World::PurgeEnemies(bool dropLoot) {
    for (Enemy& e : enemies_) {
        if (e.alive && !IsBoss(e.kind)) {
            KillEnemy(e, dropLoot && e.Active());
        }
    }
    for (Enemy& e : pendingSpawns_) {
        e.alive = false;
    }
    for (Beam& b : beams_) {
        b.alive = false;
    }
}

void World::DropLoot(const Enemy& e) {
    const EnemyInfo& info = GetEnemyInfo(e.kind);
    int xp = info.xp * (e.elite ? 8 : 1);
    if (IsBoss(e.kind)) {
        xp = static_cast<int>(static_cast<float>(xp) * (1.0f + 0.5f * static_cast<float>(director_.Loop())));
    }

    auto spawnPickup = [&](PickupKind kind, int value, float speed) {
        if (static_cast<int>(pickups_.size()) >= kMaxPickups && kind == PickupKind::Xp) {
            // Too many shards on the floor: fold the value into an existing one.
            pickups_[rng_.Next() % pickups_.size()].value += value;
            return;
        }
        Pickup p;
        p.kind = kind;
        p.position = e.position + rng_.InCircle(e.radius * 0.5f);
        p.velocity = rng_.UnitVector() * rng_.Range(speed * 0.4f, speed);
        p.value = value;
        p.life = kind == PickupKind::Xp ? 30.0f : 20.0f;
        pickups_.push_back(p);
    };

    // Break the experience into a handful of shards: big ones read as a jackpot.
    while (xp > 0) {
        const int value = xp >= 25 ? 25 : (xp >= 5 ? 5 : 1);
        spawnPickup(PickupKind::Xp, value, 160.0f + static_cast<float>(e.radius) * 3.0f);
        xp -= value;
    }

    const bool boss = IsBoss(e.kind);
    if (boss || (e.elite && rng_.Chance(0.4f)) || rng_.Chance(0.004f)) {
        spawnPickup(PickupKind::Health, 1, 120.0f);
    }
    if (boss) {
        spawnPickup(PickupKind::Health, 1, 160.0f);
    }
    if (e.elite) {
        spawnPickup(PickupKind::Cache, 1, 100.0f);
    }
    if (!boss && rng_.Chance(0.0025f)) {
        spawnPickup(PickupKind::Magnet, 1, 100.0f);
    }
}

void World::GainXp(int amount) {
    xp_ += amount;
    while (xp_ >= xpToNext_) {
        xp_ -= xpToNext_;
        level_++;
        xpToNext_ = XpForLevel(level_);
        pendingLevelUps_++;
        Emit(EventType::LevelUp, player_.position, {120, 255, 200, 255}, static_cast<float>(level_));
    }
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
        // Shards only become collectable after their initial pop-out.
        if (p.alive && pickup.age > 0.25f && (pickup.magnetized || distSq < magnetSq)) {
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
    static constexpr std::array<int, 6> kBlades{0, 2, 3, 3, 4, 6};
    return kBlades[static_cast<std::size_t>(std::clamp(stats_.Module(ModuleId::OrbitBlades), 0, 5))];
}

float World::OrbitRadius() const { return stats_.Module(ModuleId::OrbitBlades) >= 3 ? 125.0f : 100.0f; }

} // namespace gb
