// Boss encounters. Each boss runs a scripted attack sequence that grows with
// its phase (66% and 33% health thresholds). Shared conventions:
//   segment  - current phase (0..2)
//   state    - index into the phase's attack sequence
//   timer    - time spent in the current attack (negative = recovery gap)
//   timer2   - countdown to the next shot within an attack
//   counter  - shots fired in the current attack
//   param    - rotating pattern offset

#include "game/EnemyCatalog.h"
#include "game/World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace gb {

namespace {

constexpr float kIntroTime = 1.4f;

struct AttackRunner {
    Enemy& e;

    int Phase() const { return e.segment; }
    bool Recovering() const { return e.timer < 0.0f; }
    bool ShotReady(float dt) {
        e.timer2 -= dt;
        return e.timer2 <= 0.0f;
    }
    void Finish(float gap) {
        e.state++;
        e.timer = -gap;
        e.timer2 = 0.0f;
        e.counter = 0;
    }
};

int CurrentAttack(const Enemy& e, std::span<const int> sequence) {
    return sequence[static_cast<std::size_t>(e.state) % sequence.size()];
}

// Picks the attack sequence for a boss's current phase (0..2).
std::span<const int> PhaseSequence(int phase, std::span<const int> phase0, std::span<const int> phase1,
                                   std::span<const int> phase2) {
    return phase == 0 ? phase0 : (phase == 1 ? phase1 : phase2);
}

} // namespace

void World::BehaveBoss(Enemy& e, float dt) {
    const float fraction = e.hp / std::max(e.maxHp, 1.0f);
    const int phase = fraction < 0.33f ? 2 : (fraction < 0.66f ? 1 : 0);
    if (phase > e.segment) {
        e.segment = phase;
        e.state = 0;
        e.timer = -1.0f;
        e.counter = 0;
        // Never strand a boss mid-attack in a vulnerable-state change (e.g. teleport).
        if (e.kind != EnemyKind::BossCore) {
            e.invulnerable = false;
        }
        e.contactDamage = true;
        Emit(EventType::BossPhase, e.position, GetEnemyInfo(e.kind).color, static_cast<float>(phase), {}, phase,
             e.kind);
    }
    if (e.timer < 0.0f || e.age > kIntroTime) {
        e.timer += dt;
    }
    switch (e.kind) {
    case EnemyKind::BossWarden: UpdateBossWarden(e, dt); break;
    case EnemyKind::BossHydra: UpdateBossHydra(e, dt); break;
    case EnemyKind::BossSingularity: UpdateBossSingularity(e, dt); break;
    case EnemyKind::BossArchitect: UpdateBossArchitect(e, dt); break;
    case EnemyKind::BossCore: UpdateBossCore(e, dt); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// WARDEN: a rotating fortress. Rings, aimed volleys, summons; later spirals
// and a telegraphed ram.
// ---------------------------------------------------------------------------

void World::UpdateBossWarden(Enemy& e, float dt) {
    AttackRunner run{e};
    const Color color = GetEnemyInfo(e.kind).color;
    const Color bullet{255, 90, 160, 255};
    const int phase = run.Phase();
    const Vector2 toPlayer = ToPlayer(e);

    static constexpr std::array<int, 3> kPhase0{0, 1, 2};
    static constexpr std::array<int, 4> kPhase1{0, 3, 1, 2};
    static constexpr std::array<int, 6> kPhase2{3, 4, 0, 4, 1, 2};
    const std::span<const int> sequence = PhaseSequence(phase, kPhase0, kPhase1, kPhase2);
    const int attack = CurrentAttack(e, sequence);

    const bool charging = attack == 4 && !run.Recovering();
    if (!charging) {
        const Vector2 anchor = player_.position - Normalized(toPlayer) * 400.0f;
        const float arrive = std::min(1.0f, Distance(anchor, e.position) / 200.0f);
        SteerToward(e, anchor, e.speed * (1.0f + 0.3f * static_cast<float>(phase)) * arrive, 0.7f, dt);
    }
    e.angle += dt * (0.5f + 0.4f * static_cast<float>(phase));

    if (e.age < kIntroTime || run.Recovering()) {
        return;
    }

    switch (attack) {
    case 0: // rings
        if (run.ShotReady(dt)) {
            e.timer2 = 0.5f;
            FireRing(e.position, 16 + 4 * phase, 220.0f + 30.0f * static_cast<float>(phase), e.param,
                     EnemyBulletStyle::Orb, bullet);
            e.param += 0.19f;
            Emit(EventType::EnemyShot, e.position, bullet, 2.0f);
            if (++e.counter >= 3) {
                run.Finish(0.9f);
            }
        }
        break;
    case 1: // aimed volleys
        if (run.ShotReady(dt)) {
            e.timer2 = 0.32f;
            FireFan(e.position, AngleOf(toPlayer), 5, 0.16f, 360.0f, EnemyBulletStyle::Needle, color);
            Emit(EventType::EnemyShot, e.position, color, 1.5f, Normalized(toPlayer));
            if (++e.counter >= 4 + phase) {
                run.Finish(0.9f);
            }
        }
        break;
    case 2: { // summon
        const int count = 4 + 2 * phase;
        for (int i = 0; i < count; ++i) {
            const Vector2 offset = FromAngle(e.angle + kTau * static_cast<float>(i) / count, e.radius + 70.0f);
            SpawnEnemy(phase >= 2 && (i % 2 == 0) ? EnemyKind::Dart : EnemyKind::Drifter, e.position + offset, 0.7f);
        }
        run.Finish(1.6f);
        break;
    }
    case 3: // spiral
        if (run.ShotReady(dt)) {
            e.timer2 = 0.085f;
            const int arms = phase >= 2 ? 3 : 2;
            FireRing(e.position, arms, 250.0f, e.param, EnemyBulletStyle::Orb, bullet);
            e.param += 0.23f;
            if (e.counter++ % 6 == 0) {
                Emit(EventType::EnemyShot, e.position, bullet, 0.6f);
            }
        }
        if (e.timer > 3.2f) {
            run.Finish(0.8f);
        }
        break;
    case 4: // telegraphed ram
        if (e.counter == 0) {
            e.counter = 1;
            e.target = player_.position;
            e.velocity = {};
            Emit(EventType::LaserCharge, e.position, color, 0.8f, Normalized(toPlayer));
        }
        if (e.timer < 0.8f) {
            e.velocity = DampV(e.velocity, {0.0f, 0.0f}, 0.05f, dt);
            e.telegraph = e.timer / 0.8f;
            e.telegraphTarget = e.target;
        } else if (e.counter == 1) {
            e.counter = 2;
            e.telegraph = 0.0f;
            e.velocity = Normalized(e.target - e.position) * 950.0f;
        } else if (e.timer > 1.45f) {
            FireRing(e.position, 20, 240.0f, e.param, EnemyBulletStyle::Orb, bullet);
            Emit(EventType::Explosion, e.position, color, 160.0f);
            e.velocity = {};
            run.Finish(1.0f);
        }
        break;
    default: run.Finish(0.5f); break;
    }
}

// ---------------------------------------------------------------------------
// HYDRA: a colossal serpent whose body is a wall of bullets.
// ---------------------------------------------------------------------------

void World::UpdateBossHydra(Enemy& e, float dt) {
    AttackRunner run{e};
    const Color color = GetEnemyInfo(e.kind).color;
    const Color bullet{255, 120, 60, 255};
    const int phase = run.Phase();

    if (!e.initialized) {
        e.initialized = true;
        EntityId leader = e.id;
        for (int i = 0; i < 18; ++i) {
            Enemy& body = SpawnEnemy(EnemyKind::WeaverBody, e.position, 0.0f);
            body.leader = leader;
            body.segment = i + 1;
            body.radius = 24.0f - static_cast<float>(i) * 0.6f;
            leader = body.id;
        }
    }

    static constexpr std::array<int, 2> kPhase0{0, 1};
    static constexpr std::array<int, 3> kPhase1{0, 2, 1};
    static constexpr std::array<int, 4> kPhase2{3, 2, 0, 1};
    const std::span<const int> sequence = PhaseSequence(phase, kPhase0, kPhase1, kPhase2);
    const int attack = CurrentAttack(e, sequence);

    const bool rushing = attack == 2 && !run.Recovering();
    const float speed = (230.0f + 50.0f * static_cast<float>(phase)) * (rushing ? 1.9f : 1.0f);
    const float weave = std::sin(e.age * 1.7f) * (rushing ? 0.25f : 0.9f);
    const Vector2 dir = Rotated(Normalized(ToPlayer(e)), weave);
    e.velocity = DampV(e.velocity, dir * speed, 0.35f, dt);
    e.angle = AngleOf(e.velocity);

    if (e.age < kIntroTime || run.Recovering()) {
        return;
    }

    auto forEachSegment = [&](auto&& fn) {
        for (Enemy& body : enemies_) {
            if (body.alive && body.kind == EnemyKind::WeaverBody && body.segment > 0) {
                // Walk the chain: only segments that belong to this boss.
                const Enemy* walker = &body;
                int guard = 0;
                while (walker != nullptr && walker->kind == EnemyKind::WeaverBody && guard++ < 64) {
                    walker = FindEnemy(walker->leader);
                }
                if (walker != nullptr && walker->id == e.id) {
                    fn(body);
                }
            }
        }
    };

    switch (attack) {
    case 0: // broadside: every segment fires sideways
        if (run.ShotReady(dt)) {
            e.timer2 = 0.75f;
            forEachSegment([&](Enemy& body) {
                if (body.segment % 2 == e.counter % 2) {
                    const Vector2 side = FromAngle(body.angle + kPi * 0.5f, 190.0f);
                    FireEnemyBullet(body.position, side, EnemyBulletStyle::Orb, bullet, 8.0f);
                    FireEnemyBullet(body.position, side * -1.0f, EnemyBulletStyle::Orb, bullet, 8.0f);
                }
            });
            Emit(EventType::EnemyShot, e.position, bullet, 2.0f);
            if (++e.counter >= 3) {
                run.Finish(1.0f);
            }
        }
        break;
    case 1: // head fans
        if (run.ShotReady(dt)) {
            e.timer2 = 0.45f;
            FireFan(e.position, AngleOf(ToPlayer(e)), 7, 0.13f, 380.0f, EnemyBulletStyle::Needle, color);
            Emit(EventType::EnemyShot, e.position, color, 1.5f);
            if (++e.counter >= 3) {
                run.Finish(1.0f);
            }
        }
        break;
    case 2: // rush
        if (e.counter == 0) {
            e.counter = 1;
            Emit(EventType::BossPhase, e.position, color, 0.0f, {}, -1, e.kind);
        }
        if (e.timer > 2.6f) {
            run.Finish(0.8f);
        }
        break;
    case 3: // tail spray: segments aim at the player one after another
        if (run.ShotReady(dt)) {
            e.timer2 = 0.06f;
            int index = 0;
            forEachSegment([&](Enemy& body) {
                if (index++ == e.counter) {
                    FireEnemyBullet(body.position, Normalized(player_.position - body.position) * 300.0f,
                                    EnemyBulletStyle::Needle, bullet, 8.0f);
                }
            });
            if (++e.counter >= 18) {
                run.Finish(0.8f);
            }
        }
        break;
    default: run.Finish(0.5f); break;
    }
}

// ---------------------------------------------------------------------------
// EVENT HORIZON: a gravity well that drags the player into spiralling arms.
// ---------------------------------------------------------------------------

void World::UpdateBossSingularity(Enemy& e, float dt) {
    AttackRunner run{e};
    const Color color = GetEnemyInfo(e.kind).color;
    const Color bullet{255, 170, 60, 255};
    const int phase = run.Phase();

    // Drift lazily around the arena centre.
    const Vector2 anchor = FromAngle(e.age * 0.25f, 260.0f);
    SteerToward(e, anchor, e.speed, 1.2f, dt);
    e.angle += dt * (0.8f + 0.5f * static_cast<float>(phase));

    static constexpr std::array<int, 2> kPhase0{0, 1};
    static constexpr std::array<int, 3> kPhase1{0, 2, 1};
    static constexpr std::array<int, 4> kPhase2{3, 0, 1, 2};
    const std::span<const int> sequence = PhaseSequence(phase, kPhase0, kPhase1, kPhase2);
    const int attack = CurrentAttack(e, sequence);

    if (e.age < kIntroTime || run.Recovering()) {
        return;
    }

    switch (attack) {
    case 0: // spiral arms
        if (run.ShotReady(dt)) {
            e.timer2 = 0.11f;
            const int arms = 4 + phase;
            for (int i = 0; i < arms; ++i) {
                const float a = e.param + kTau * static_cast<float>(i) / static_cast<float>(arms);
                EnemyBullet& b = FireEnemyBullet(e.position + FromAngle(a, e.radius), FromAngle(a, 210.0f),
                                                 EnemyBulletStyle::Orb, bullet);
                b.turn = phase >= 2 ? -0.25f : 0.25f;
            }
            e.param += 0.16f;
            if (e.counter++ % 5 == 0) {
                Emit(EventType::EnemyShot, e.position, bullet, 0.6f);
            }
        }
        if (e.timer > 4.0f) {
            run.Finish(1.0f);
        }
        break;
    case 1: // collapse rings that accelerate outward
        if (run.ShotReady(dt)) {
            e.timer2 = 0.7f;
            for (int i = 0; i < 30; ++i) {
                const float a = e.param + kTau * static_cast<float>(i) / 30.0f;
                EnemyBullet& b = FireEnemyBullet(e.position + FromAngle(a, e.radius), FromAngle(a, 50.0f),
                                                 EnemyBulletStyle::BigOrb, color, 11.0f);
                b.acceleration = 230.0f;
            }
            e.param += 0.1f;
            Emit(EventType::EnemyShot, e.position, color, 2.0f);
            if (++e.counter >= 2 + phase) {
                run.Finish(1.2f);
            }
        }
        break;
    case 2: { // spawn lesser wells
        int wells = 0;
        for (const Enemy& other : enemies_) {
            if (other.alive && other.kind == EnemyKind::Singularity) {
                ++wells;
            }
        }
        for (int i = wells; i < 2; ++i) {
            const Vector2 p{rng_.Range(kArena.x + 200.0f, kArena.x + kArena.width - 200.0f),
                            rng_.Range(kArena.y + 200.0f, kArena.y + kArena.height - 200.0f)};
            SpawnEnemy(EnemyKind::Singularity, p, 1.0f);
        }
        for (int i = 0; i < 6; ++i) {
            SpawnEnemy(EnemyKind::Swarmer, e.position + FromAngle(kTau * static_cast<float>(i) / 6.0f, 120.0f), 0.6f);
        }
        run.Finish(1.2f);
        break;
    }
    case 3: // rotating lances
        if (e.counter == 0) {
            e.counter = 1;
            for (int i = 0; i < 5; ++i) {
                Beam& beam =
                    AddBeam(e.position, e.param + kTau * static_cast<float>(i) / 5.0f, 1.3f, 2.6f, 26.0f, color, e.id);
                beam.sweep = 0.45f;
            }
        }
        if (e.timer > 4.2f) {
            run.Finish(0.6f);
        }
        break;
    default: run.Finish(0.5f); break;
    }
}

// ---------------------------------------------------------------------------
// ARCHITECT: teleporting laser specialist that builds turrets.
// ---------------------------------------------------------------------------

void World::UpdateBossArchitect(Enemy& e, float dt) {
    AttackRunner run{e};
    const Color color = GetEnemyInfo(e.kind).color;
    const int phase = run.Phase();
    const Vector2 toPlayer = ToPlayer(e);

    static constexpr std::array<int, 3> kPhase0{0, 2, 3};
    static constexpr std::array<int, 4> kPhase1{0, 1, 3, 2};
    static constexpr std::array<int, 5> kPhase2{1, 0, 3, 1, 2};
    const std::span<const int> sequence = PhaseSequence(phase, kPhase0, kPhase1, kPhase2);
    const int attack = CurrentAttack(e, sequence);

    if (attack != 3 || run.Recovering()) {
        const float dist = Length(toPlayer);
        const Vector2 dir = Normalized(toPlayer);
        Vector2 desired = Perp(dir) * 0.7f;
        if (dist < 420.0f) {
            desired -= dir;
        } else if (dist > 640.0f) {
            desired += dir;
        }
        e.velocity = DampV(e.velocity, Normalized(desired, {}) * e.speed, 0.6f, dt);
    }
    e.angle += dt * 0.7f;

    if (e.age < kIntroTime || run.Recovering()) {
        return;
    }

    switch (attack) {
    case 0: // laser fan
        if (e.counter == 0) {
            e.counter = 1;
            const int beams = 3 + 2 * phase;
            const float spread = 0.32f;
            const float base = AngleOf(toPlayer);
            for (int i = 0; i < beams; ++i) {
                const float k = static_cast<float>(i) - static_cast<float>(beams - 1) * 0.5f;
                AddBeam(e.position, base + k * spread, 1.0f, 0.55f, 26.0f, color, e.id);
            }
        }
        e.velocity = DampV(e.velocity, {0.0f, 0.0f}, 0.1f, dt);
        if (e.timer > 1.7f) {
            run.Finish(0.6f);
        }
        break;
    case 1: // twin sweeping beams
        if (e.counter == 0) {
            e.counter = 1;
            const float base = AngleOf(toPlayer) - 1.1f;
            Beam& a = AddBeam(e.position, base, 1.2f, 2.2f, 30.0f, color, e.id);
            a.sweep = 1.0f;
            Beam& b = AddBeam(e.position, base + kPi, 1.2f, 2.2f, 30.0f, color, e.id);
            b.sweep = 1.0f;
        }
        e.velocity = DampV(e.velocity, {0.0f, 0.0f}, 0.1f, dt);
        if (e.timer > 3.5f) {
            run.Finish(0.6f);
        }
        break;
    case 2: { // build turrets
        int turrets = 0;
        for (const Enemy& other : enemies_) {
            if (other.alive && other.kind == EnemyKind::BossMinion) {
                ++turrets;
            }
        }
        const int target = 2 + phase;
        for (int i = turrets; i < target; ++i) {
            const Vector2 p{rng_.Range(kArena.x + 150.0f, kArena.x + kArena.width - 150.0f),
                            rng_.Range(kArena.y + 150.0f, kArena.y + kArena.height - 150.0f)};
            Enemy& turret = SpawnEnemy(EnemyKind::BossMinion, p, 1.0f);
            turret.maxHp = turret.hp = 180.0f * director_.EnemyHpMultiplier() * tuning_.enemyHp;
            turret.timer = 1.5f;
        }
        run.Finish(0.8f);
        break;
    }
    case 3: // teleport
        if (e.counter == 0) {
            e.counter = 1;
            e.invulnerable = true;
            e.contactDamage = false;
            e.velocity = {};
            for (int attempt = 0; attempt < 12; ++attempt) {
                e.target = {rng_.Range(kArena.x + 200.0f, kArena.x + kArena.width - 200.0f),
                            rng_.Range(kArena.y + 200.0f, kArena.y + kArena.height - 200.0f)};
                if (DistanceSq(e.target, player_.position) > 450.0f * 450.0f) {
                    break;
                }
            }
            Emit(EventType::EnemySpawnWarning, e.target, color, 1.5f, {}, 0, e.kind);
        }
        if (e.counter == 1 && e.timer > 0.7f) {
            e.counter = 2;
            e.position = e.target;
            e.invulnerable = false;
            e.contactDamage = true;
            FireRing(e.position, 18 + 4 * phase, 260.0f, e.param, EnemyBulletStyle::Orb, color);
            e.param += 0.3f;
            Emit(EventType::Explosion, e.position, color, 140.0f);
        }
        if (e.timer > 1.2f) {
            run.Finish(0.5f);
        }
        break;
    default: run.Finish(0.5f); break;
    }
}

// ---------------------------------------------------------------------------
// THE CORE: shielded by orbiting nodes, then everything at once.
// ---------------------------------------------------------------------------

void World::UpdateBossCore(Enemy& e, float dt) {
    AttackRunner run{e};
    const Color color{255, 255, 255, 255};
    const Color bullet{255, 70, 90, 255};
    const Color bullet2{120, 200, 255, 255};
    const int phase = run.Phase();

    if (!e.initialized) {
        e.initialized = true;
        for (int i = 0; i < 4; ++i) {
            Enemy& node = SpawnEnemy(EnemyKind::BossMinion, e.position, 0.8f);
            node.leader = e.id;
            node.param = kTau * static_cast<float>(i) / 4.0f;
            node.maxHp = node.hp = 520.0f * tuning_.enemyHp * (1.0f + 0.9f * static_cast<float>(director_.Loop()));
            node.radius = 26.0f;
            node.timer = 1.0f + 0.4f * static_cast<float>(i);
        }
    }
    int nodes = 0;
    for (const Enemy& other : enemies_) {
        if (other.alive && other.kind == EnemyKind::BossMinion && other.leader == e.id) {
            ++nodes;
        }
    }
    for (const Enemy& other : pendingSpawns_) {
        if (other.alive && other.kind == EnemyKind::BossMinion && other.leader == e.id) {
            ++nodes;
        }
    }
    const bool shielded = nodes > 0;
    if (e.invulnerable != shielded) {
        e.invulnerable = shielded;
        if (!shielded) {
            Emit(EventType::BossPhase, e.position, color, 0.0f, {}, -2, e.kind);
        }
    }

    const Vector2 anchor = FromAngle(e.age * 0.2f, 180.0f);
    SteerToward(e, anchor, e.speed, 1.5f, dt);
    e.angle += dt * (0.4f + 0.4f * static_cast<float>(phase));

    static constexpr std::array<int, 3> kShielded{0, 3, 1};
    static constexpr std::array<int, 4> kPhase0{0, 1, 2, 3};
    static constexpr std::array<int, 4> kPhase1{2, 1, 0, 3};
    static constexpr std::array<int, 4> kPhase2{4, 2, 4, 3};
    const std::span<const int> sequence = shielded     ? std::span<const int>(kShielded)
                                          : phase == 0 ? std::span<const int>(kPhase0)
                                          : phase == 1 ? std::span<const int>(kPhase1)
                                                       : std::span<const int>(kPhase2);
    const int attack = CurrentAttack(e, sequence);

    if (e.age < kIntroTime || run.Recovering()) {
        return;
    }

    switch (attack) {
    case 0: // alternating rings
        if (run.ShotReady(dt)) {
            e.timer2 = 0.42f;
            FireRing(e.position, 22 + 4 * phase, 230.0f, e.param, EnemyBulletStyle::Orb,
                     e.counter % 2 == 0 ? bullet : bullet2);
            e.param += kTau / 44.0f;
            Emit(EventType::EnemyShot, e.position, bullet, 1.5f);
            if (++e.counter >= 4) {
                run.Finish(1.0f);
            }
        }
        break;
    case 1: // double spiral
        if (run.ShotReady(dt)) {
            e.timer2 = 0.09f;
            FireRing(e.position, 3, 250.0f, e.param, EnemyBulletStyle::Orb, bullet);
            FireRing(e.position, 3, 200.0f, -e.param * 1.3f, EnemyBulletStyle::Orb, bullet2);
            e.param += 0.2f;
            if (e.counter++ % 6 == 0) {
                Emit(EventType::EnemyShot, e.position, bullet, 0.6f);
            }
        }
        if (e.timer > 3.6f) {
            run.Finish(1.0f);
        }
        break;
    case 2: // rotating laser cross
        if (e.counter == 0) {
            e.counter = 1;
            const int beams = phase >= 2 ? 6 : 4;
            for (int i = 0; i < beams; ++i) {
                Beam& beam = AddBeam(e.position, e.param + kTau * static_cast<float>(i) / static_cast<float>(beams),
                                     1.3f, 3.0f, 30.0f, {255, 80, 120, 255}, e.id);
                beam.sweep = (phase % 2 == 0) ? 0.55f : -0.55f;
            }
        }
        if (e.timer > 4.6f) {
            run.Finish(0.6f);
        }
        break;
    case 3: { // summon a mixed squad
        const int count = 3 + phase;
        for (int i = 0; i < count; ++i) {
            const Vector2 p = e.position + FromAngle(kTau * static_cast<float>(i) / count + e.param, 220.0f);
            SpawnEnemy(i % 2 == 0 ? EnemyKind::Dart : EnemyKind::Gunner, p, 0.8f);
        }
        for (int i = 0; i < 8; ++i) {
            SpawnEnemy(EnemyKind::Swarmer, e.position + FromAngle(kTau * static_cast<float>(i) / 8.0f, 140.0f), 0.6f);
        }
        run.Finish(1.4f);
        break;
    }
    case 4: // desperation: spiral and rings together
        if (run.ShotReady(dt)) {
            e.timer2 = 0.1f;
            FireRing(e.position, 3, 260.0f, e.param, EnemyBulletStyle::Orb, bullet);
            e.param += 0.21f;
            if (++e.counter % 12 == 0) {
                FireRing(e.position, 24, 170.0f, -e.param, EnemyBulletStyle::BigOrb, bullet2, 11.0f);
                Emit(EventType::EnemyShot, e.position, bullet2, 2.0f);
            }
        }
        if (e.timer > 5.0f) {
            run.Finish(1.0f);
        }
        break;
    default: run.Finish(0.5f); break;
    }
}

} // namespace gb
