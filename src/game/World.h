#pragma once

#include "core/Random.h"
#include "game/Director.h"
#include "game/Entities.h"
#include "game/Ships.h"
#include "game/SpatialHash.h"
#include "game/Stats.h"
#include "game/Upgrades.h"

#include <deque>
#include <span>
#include <unordered_map>
#include <vector>

namespace gb {

// Device-independent intent for one simulation step (from a human or the bot).
struct PlayerInput {
    Vector2 move{};
    Vector2 aim{1.0f, 0.0f}; // unit direction
    bool fire = false;
    bool dash = false; // edge-triggered
    bool nova = false; // edge-triggered
};

// Permanent bonuses bought with credits between runs.
struct MetaBonuses {
    int bonusHp = 0;
    float damage = 0.0f; // fraction, e.g. 0.1 = +10%
    float xp = 0.0f;
    float magnet = 0.0f;
    int rerolls = 0;
    float dashCooldown = 0.0f;
    float novaCharge = 0.0f; // starting nova charge (0..1)
};

struct RunConfig {
    GameMode mode = GameMode::Campaign;
    Difficulty difficulty = Difficulty::Standard;
    ShipId ship = ShipId::Vector;
    std::uint64_t seed = 1;
    MetaBonuses meta;
};

struct RunStats {
    float time = 0.0f;
    int kills = 0;
    int elitesKilled = 0;
    int bossesKilled = 0;
    int hitsTaken = 0;
    long long score = 0;
    int bestCombo = 0;
    int shardsCollected = 0;
    double damageDealt = 0.0;
    int sectorsCleared = 0;
    int flawlessSectors = 0;               // sectors cleared without taking a hit
    std::uint32_t bossMask = 0;            // bit per EnemyKind of defeated bosses
    std::uint32_t enemiesSeen = 0;         // bit per EnemyKind encountered (codex)
    std::uint64_t upgradesTaken = 0;       // bit per UpgradeId drafted (codex)
    EnemyKind killedBy = EnemyKind::Count; // what dealt the final hit (Count = unknown)
    bool victory = false;
};

struct DifficultyTuning {
    float enemyHp;
    float enemySpeed;
    float spawnRate;
    float bulletSpeed;
    float scoreMultiplier;
    int bonusHp;
};

const DifficultyTuning& GetDifficultyTuning(Difficulty difficulty);
const char* DifficultyName(Difficulty difficulty);

// The whole gameplay simulation. Rendering-agnostic: everything visual is
// communicated through the event queue and read-only accessors, which keeps it
// testable headless and lets the bot drive it.
class World {
public:
    static constexpr Rectangle kArena{-1400.0f, -900.0f, 2800.0f, 1800.0f};
    static constexpr int kMaxEnemies = 700;
    static constexpr float kMaxStep = 1.0f / 120.0f;

    explicit World(const RunConfig& config);

    // Advances the simulation; large steps are split into substeps internally.
    void Update(float dt, const PlayerInput& input);

    // --- Build -------------------------------------------------------------
    void ApplyUpgrade(UpgradeId id);
    const UpgradeInventory& Inventory() const { return inventory_; }
    // luckBonus shifts the roll toward rarer cards (data caches use it).
    std::vector<UpgradeId> RollOffers(int count, std::span<const UpgradeId> banned = {}, float luckBonus = 0.0f);
    const PlayerStats& Stats() const { return stats_; }
    int Rerolls() const { return rerolls_; }
    bool UseReroll();

    int PendingLevelUps() const { return pendingLevelUps_; }
    int PendingCaches() const { return pendingCaches_; }
    void ConsumeLevelUp() { pendingLevelUps_ = std::max(0, pendingLevelUps_ - 1); }
    void ConsumeCache() { pendingCaches_ = std::max(0, pendingCaches_ - 1); }

    int Level() const { return level_; }
    int Xp() const { return xp_; }
    int XpToNext() const { return xpToNext_; }
    static int XpForLevel(int level);

    // --- Spawning API (director, bosses, behaviours) ----------------------
    // Returned references stay valid until the end of the current step.
    Enemy& SpawnEnemy(EnemyKind kind, Vector2 position, float telegraph = 0.9f, bool elite = false);
    EnemyBullet& FireEnemyBullet(Vector2 position, Vector2 velocity, EnemyBulletStyle style, Color color,
                                 float radius = 9.0f);
    Beam& AddBeam(Vector2 origin, float angle, float warmup, float duration, float width, Color color,
                  EntityId owner = kInvalidId);
    Shockwave& AddShockwave(Vector2 position, float maxRadius, float speed, float damage, float knockback,
                            bool clearsBullets);
    // Drops a pickup with a random outward velocity of up to `speed`.
    void SpawnPickup(PickupKind kind, Vector2 position, int value, float speed);
    void Emit(EventType type, Vector2 position, Color color = WHITE, float magnitude = 1.0f, Vector2 direction = {},
              int value = 0, EnemyKind enemy = EnemyKind::Drifter, EntityId source = kInvalidId);

    // --- Damage ------------------------------------------------------------
    // Returns true if the enemy died from this hit.
    bool DamageEnemy(Enemy& enemy, float amount, bool crit, Vector2 direction, float knockback = 0.0f);
    void KillEnemy(Enemy& enemy, bool dropLoot);
    void DamagePlayer(Vector2 source, EnemyKind cause = EnemyKind::Count);
    // The kind leading a segment chain (the enemy's own kind for non-segments).
    EnemyKind ChainHeadKind(const Enemy& e) const;
    void ClearEnemyBullets(Vector2 center, float radius);
    // Removes every non-boss enemy (used before boss fights); optionally pays out loot.
    void PurgeEnemies(bool dropLoot);

    // --- Queries -----------------------------------------------------------
    Rng& Random() { return rng_; }
    const RunConfig& Config() const { return config_; }
    const DifficultyTuning& Tuning() const { return tuning_; }
    Player& GetPlayer() { return player_; }
    const Player& GetPlayer() const { return player_; }
    std::vector<Enemy>& Enemies() { return enemies_; }
    const std::vector<Enemy>& Enemies() const { return enemies_; }
    const std::vector<Bullet>& Bullets() const { return bullets_; }
    const std::vector<EnemyBullet>& EnemyBullets() const { return enemyBullets_; }
    const std::vector<Beam>& Beams() const { return beams_; }
    const std::vector<Pickup>& Pickups() const { return pickups_; }
    const std::vector<Drone>& Drones() const { return drones_; }
    const std::vector<Missile>& Missiles() const { return missiles_; }
    const std::vector<PlayerMine>& Mines() const { return playerMines_; }
    const std::vector<Shockwave>& Shockwaves() const { return shockwaves_; }
    const std::vector<ArcTrace>& Arcs() const { return arcs_; }
    const std::vector<LanceTrace>& Lances() const { return lances_; }
    float OrbitAngle() const { return orbitAngle_; }
    int OrbitBladeCount() const;
    float OrbitRadius() const;

    Enemy* FindEnemy(EntityId id);
    const Enemy* FindEnemy(EntityId id) const { return const_cast<World*>(this)->FindEnemy(id); }
    Enemy* NearestEnemy(Vector2 position, float maxRange, EntityId exclude = kInvalidId);
    int ActiveEnemyCount() const { return activeEnemies_; }

    std::vector<GameEvent>& Events() { return events_; }
    const RunStats& Record() const { return runStats_; }
    Director& GetDirector() { return director_; }
    const Director& GetDirector() const { return director_; }

    int Combo() const { return combo_; }
    float ComboTimer() const { return comboTimer_; }
    int ComboMultiplier() const;
    float Time() const { return runStats_.time; }
    bool PlayerAlive() const { return player_.alive; }

    // Run-level hooks the director calls.
    void AddScore(long long amount);
    void OnSectorCleared();
    // Applies the player's sector modifier choice and starts the next sector.
    void ChooseSectorModifier(SectorModifier modifier);
    void ContinueEndless();
    float ModulePower() const;

private:
    void Step(float dt, const PlayerInput& input);
    void RecomputeStats();

    void UpdatePlayer(float dt, const PlayerInput& input);
    void FireMainGun();
    void SpawnPlayerBullet(Vector2 origin, float angle, float speedScale);
    void UpdateBullets(float dt);
    void UpdateEnemyBullets(float dt);
    void UpdateBeams(float dt);
    void UpdatePickups(float dt);
    void UpdateShockwaves(float dt);
    void UpdateEnemies(float dt);
    void UpdateEnemy(Enemy& enemy, float dt);
    void UpdateModules(float dt);
    void UpdateOrbitBlades(float dt);
    void UpdateDrones(float dt);
    void UpdateArcCoil(float dt);
    void UpdateMissiles(float dt);
    void UpdatePulseNova(float dt);
    void UpdateMines(float dt);
    void UpdatePrismLance(float dt);
    void UpdateTraces(float dt);
    void ResolveEnemySeparation();
    void HandlePlayerContacts();
    void RebuildSpatialIndex();
    void FlushSpawns();
    void Compact();
    void GainXp(int amount);
    void DropLoot(const Enemy& enemy);
    void Explode(Vector2 position, float radius, float damage, EntityId exclude, Color color);
    void OnBulletHit(Bullet& bullet, Enemy& enemy);
    void ApplyGravityWells(float dt);

    // Behaviours (EnemyBehaviors.cpp).
    void BehaveDrifter(Enemy& e, float dt);
    void BehaveDart(Enemy& e, float dt);
    void BehavePinwheel(Enemy& e, float dt);
    void BehaveSplitter(Enemy& e, float dt);
    void BehaveGunner(Enemy& e, float dt);
    void BehaveSwarmer(Enemy& e, float dt);
    void BehaveHive(Enemy& e, float dt);
    void BehaveSingularity(Enemy& e, float dt);
    void BehaveWeaver(Enemy& e, float dt);
    void BehaveWeaverBody(Enemy& e, float dt);
    void BehaveShielder(Enemy& e, float dt);
    void BehaveLancer(Enemy& e, float dt);
    void BehaveBomber(Enemy& e, float dt);
    void BehaveMine(Enemy& e, float dt);
    void BehaveBoss(Enemy& e, float dt);
    void BehaveBossMinion(Enemy& e, float dt);
    void OnEnemyDeath(Enemy& e);

    // Bosses (Bosses.cpp).
    void UpdateBossWarden(Enemy& e, float dt);
    void UpdateBossHydra(Enemy& e, float dt);
    void UpdateBossSingularity(Enemy& e, float dt);
    void UpdateBossArchitect(Enemy& e, float dt);
    void UpdateBossCore(Enemy& e, float dt);
    void FireRing(Vector2 center, int count, float speed, float offset, EnemyBulletStyle style, Color color,
                  float radius = 9.0f);
    void FireFan(Vector2 center, float angle, int count, float spread, float speed, EnemyBulletStyle style, Color color,
                 float radius = 9.0f);

    // Helpers shared by behaviours.
    Vector2 ToPlayer(const Enemy& e) const { return player_.position - e.position; }
    float SpeedScale(const Enemy& e) const;
    void SteerToward(Enemy& e, Vector2 target, float speed, float halfLife, float dt);
    void KeepInArena(Enemy& e, bool bounce);

    RunConfig config_;
    DifficultyTuning tuning_;
    Rng rng_;
    Player player_;
    PlayerStats stats_;
    UpgradeInventory inventory_;
    Director director_;
    RunStats runStats_;

    std::vector<Enemy> enemies_;
    std::deque<Enemy> pendingSpawns_;
    std::vector<Bullet> bullets_;
    std::vector<EnemyBullet> enemyBullets_;
    std::vector<Beam> beams_;
    std::vector<Pickup> pickups_;
    std::vector<Drone> drones_;
    std::vector<Missile> missiles_;
    std::vector<PlayerMine> playerMines_;
    std::vector<Shockwave> shockwaves_;
    std::vector<ArcTrace> arcs_;
    std::vector<LanceTrace> lances_;
    std::vector<GameEvent> events_;

    SpatialHash enemyIndex_;
    std::unordered_map<EntityId, int> idToIndex_;
    EntityId nextId_ = 1;
    int activeEnemies_ = 0;

    int level_ = 1;
    int xp_ = 0;
    int xpToNext_ = 0;
    int pendingLevelUps_ = 0;
    int pendingCaches_ = 0;
    int rerolls_ = 1;

    int combo_ = 0;
    EnemyKind actingEnemy_ = EnemyKind::Count; // enemy whose behaviour is running (attributes its shots)
    float comboTimer_ = 0.0f;
    float orbitAngle_ = 0.0f;
    float arcTimer_ = 0.0f;
    float missileTimer_ = 0.0f;
    float pulseTimer_ = 0.0f;
    float mineTimer_ = 0.0f;
    float lanceTimer_ = 0.0f;
    float shardMergeTimer_ = 0.0f;
    float baseDamage_ = 10.0f;
    int hitsAtSectorStart_ = 0;
};

} // namespace gb
