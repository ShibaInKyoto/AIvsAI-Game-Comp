// Per-kind enemy AI. Every behaviour sets the enemy's velocity (or position for
// rigidly-attached bodies); integration and arena clamping happen in UpdateEnemy.

#include "game/EnemyCatalog.h"
#include "game/World.h"

#include <algorithm>
#include <cmath>

namespace gb {

namespace {

constexpr Color kEnemyBulletOrange{255, 140, 50, 255};
constexpr Color kMineColor{60, 255, 200, 255};

} // namespace

float World::SpeedScale(const Enemy& e) const {
    float scale = director_.EnemySpeedMultiplier() * tuning_.enemySpeed;
    if (e.slowTimer > 0.0f) {
        scale *= 0.5f;
    }
    if (e.elite) {
        scale *= 0.9f;
    }
    return scale;
}

void World::SteerToward(Enemy& e, Vector2 target, float speed, float halfLife, float dt) {
    const Vector2 desired = Normalized(target - e.position) * speed;
    e.velocity = DampV(e.velocity, desired, halfLife, dt);
}

void World::KeepInArena(Enemy& e, bool bounce) {
    const float left = kArena.x + e.radius;
    const float right = kArena.x + kArena.width - e.radius;
    const float top = kArena.y + e.radius;
    const float bottom = kArena.y + kArena.height - e.radius;
    if (e.position.x < left || e.position.x > right) {
        e.position.x = std::clamp(e.position.x, left, right);
        if (bounce) {
            e.velocity.x = -e.velocity.x;
            e.param = AngleOf(e.velocity);
        }
        e.knockback.x = 0.0f;
    }
    if (e.position.y < top || e.position.y > bottom) {
        e.position.y = std::clamp(e.position.y, top, bottom);
        if (bounce) {
            e.velocity.y = -e.velocity.y;
            e.param = AngleOf(e.velocity);
        }
        e.knockback.y = 0.0f;
    }
}

void World::UpdateEnemies(float dt) {
    for (std::size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& e = enemies_[i];
        if (e.alive) {
            UpdateEnemy(e, dt);
        }
    }
}

void World::UpdateEnemy(Enemy& e, float dt) {
    e.hitFlash = std::max(0.0f, e.hitFlash - dt * 8.0f);
    e.slowTimer = std::max(0.0f, e.slowTimer - dt);
    e.bladeCooldown = std::max(0.0f, e.bladeCooldown - dt);
    e.lanceCooldown = std::max(0.0f, e.lanceCooldown - dt);
    e.dashHitCooldown = std::max(0.0f, e.dashHitCooldown - dt);
    if (e.spawnDelay > 0.0f) {
        e.spawnDelay -= dt;
        return;
    }
    e.age += dt;

    switch (e.kind) {
    case EnemyKind::Drifter: BehaveDrifter(e, dt); break;
    case EnemyKind::Dart: BehaveDart(e, dt); break;
    case EnemyKind::Pinwheel: BehavePinwheel(e, dt); break;
    case EnemyKind::Splitter:
    case EnemyKind::SplitterMini: BehaveSplitter(e, dt); break;
    case EnemyKind::Gunner: BehaveGunner(e, dt); break;
    case EnemyKind::Swarmer: BehaveSwarmer(e, dt); break;
    case EnemyKind::Hive: BehaveHive(e, dt); break;
    case EnemyKind::Singularity: BehaveSingularity(e, dt); break;
    case EnemyKind::Weaver: BehaveWeaver(e, dt); break;
    case EnemyKind::WeaverBody:
        BehaveWeaverBody(e, dt);
        return;  // positioned rigidly by its leader
    case EnemyKind::Shielder: BehaveShielder(e, dt); break;
    case EnemyKind::Lancer: BehaveLancer(e, dt); break;
    case EnemyKind::Bomber: BehaveBomber(e, dt); break;
    case EnemyKind::Mine: BehaveMine(e, dt); break;
    case EnemyKind::BossMinion: BehaveBossMinion(e, dt); break;
    case EnemyKind::BossWarden:
    case EnemyKind::BossHydra:
    case EnemyKind::BossSingularity:
    case EnemyKind::BossArchitect:
    case EnemyKind::BossCore: BehaveBoss(e, dt); break;
    case EnemyKind::Count: break;
    }
    if (!e.alive) {
        return;
    }

    e.position += (e.velocity + e.knockback) * dt;
    e.knockback = DampV(e.knockback, {0.0f, 0.0f}, 0.07f, dt);
    KeepInArena(e, e.kind == EnemyKind::Pinwheel || e.kind == EnemyKind::Singularity);
}

void World::ResolveEnemySeparation() {
    // Soft pairwise push so crowds spread into readable shapes instead of
    // collapsing into a single blob.
    for (std::size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& a = enemies_[i];
        if (!a.Active() || a.kind == EnemyKind::WeaverBody || a.kind == EnemyKind::Mine || IsBoss(a.kind)) {
            continue;
        }
        enemyIndex_.Query(a.position, a.radius, [&](int index) {
            if (static_cast<std::size_t>(index) <= i) {
                return;
            }
            Enemy& b = enemies_[static_cast<std::size_t>(index)];
            if (!b.Active() || b.kind == EnemyKind::WeaverBody || b.kind == EnemyKind::Mine || IsBoss(b.kind)) {
                return;
            }
            const Vector2 d = b.position - a.position;
            const float minDist = (a.radius + b.radius) * 0.85f;
            const float distSq = LengthSq(d);
            if (distSq < minDist * minDist && distSq > 0.0001f) {
                const float dist = std::sqrt(distSq);
                const Vector2 push = d / dist * ((minDist - dist) * 0.5f);
                a.position -= push;
                b.position += push;
            }
        });
    }
}

// ---------------------------------------------------------------------------

void World::BehaveDrifter(Enemy& e, float dt) {
    const float wobble = std::sin(e.age * 2.6f + e.timer * 10.0f) * 0.55f;
    const Vector2 dir = Rotated(Normalized(ToPlayer(e)), wobble);
    e.velocity = DampV(e.velocity, dir * (e.speed * SpeedScale(e)), 0.25f, dt);
    e.angle += dt * 1.8f;
}

void World::BehaveDart(Enemy& e, float dt) {
    const Vector2 toPlayer = ToPlayer(e);
    switch (e.state) {
    case 0:  // creep and line up
        e.timer -= dt;
        e.velocity = DampV(e.velocity, Normalized(toPlayer) * 80.0f * SpeedScale(e), 0.3f, dt);
        e.angle = RotateToward(e.angle, AngleOf(toPlayer), 6.0f * dt);
        if (e.timer <= 0.0f) {
            e.state = 1;
            e.timer = 0.55f;
        }
        break;
    case 1:  // telegraph: brake and lock on
        e.timer -= dt;
        e.velocity = DampV(e.velocity, {0.0f, 0.0f}, 0.06f, dt);
        if (e.timer > 0.25f) {
            e.angle = RotateToward(e.angle, AngleOf(toPlayer), 8.0f * dt);
        }
        if (e.timer <= 0.0f) {
            e.state = 2;
            e.timer = 0.6f;
            e.velocity = FromAngle(e.angle, e.speed * SpeedScale(e));
        }
        break;
    case 2:  // lunge
        e.timer -= dt;
        if (e.timer <= 0.0f || e.position.x <= kArena.x + e.radius + 1.0f ||
            e.position.x >= kArena.x + kArena.width - e.radius - 1.0f || e.position.y <= kArena.y + e.radius + 1.0f ||
            e.position.y >= kArena.y + kArena.height - e.radius - 1.0f) {
            e.state = 3;
            e.timer = 0.45f;
        }
        break;
    default:  // recover
        e.timer -= dt;
        e.velocity = DampV(e.velocity, {0.0f, 0.0f}, 0.1f, dt);
        if (e.timer <= 0.0f) {
            e.state = 0;
            e.timer = rng_.Range(0.7f, 1.3f) * (e.elite ? 0.5f : 1.0f);
        }
        break;
    }
}

void World::BehavePinwheel(Enemy& e, float dt) {
    if (LengthSq(e.velocity) < 1.0f) {
        e.param = rng_.Angle();
    }
    e.timer -= dt;
    if (e.timer <= 0.0f) {
        e.timer = rng_.Range(0.6f, 1.8f);
        e.param += rng_.Range(-1.2f, 1.2f);
        // Later sectors lean the wander toward the player.
        if (director_.Sector() > 0 && rng_.Chance(0.35f)) {
            e.param = AngleOf(ToPlayer(e)) + rng_.Range(-0.6f, 0.6f);
        }
    }
    e.velocity = DampV(e.velocity, FromAngle(e.param, e.speed * SpeedScale(e)), 0.35f, dt);
    e.angle += dt * 5.5f;
}

void World::BehaveSplitter(Enemy& e, float dt) {
    const bool mini = e.kind == EnemyKind::SplitterMini;
    const float jitter = mini ? std::sin(e.age * 9.0f + e.timer * 20.0f) * 0.7f : 0.0f;
    const Vector2 dir = Rotated(Normalized(ToPlayer(e)), jitter);
    e.velocity = DampV(e.velocity, dir * (e.speed * SpeedScale(e)), mini ? 0.12f : 0.3f, dt);
    e.angle += dt * (mini ? 6.0f : 1.2f);
}

void World::BehaveGunner(Enemy& e, float dt) {
    const Vector2 toPlayer = ToPlayer(e);
    const float dist = Length(toPlayer);
    const Vector2 dir = Normalized(toPlayer);
    if (e.param == 0.0f) {
        e.param = rng_.Sign();
    }
    Vector2 desired = Perp(dir) * (e.param * 0.6f);
    if (dist < 360.0f) {
        desired -= dir;
    } else if (dist > 540.0f) {
        desired += dir;
    }
    e.velocity = DampV(e.velocity, Normalized(desired, {}) * (e.speed * SpeedScale(e)), 0.35f, dt);
    e.angle = RotateToward(e.angle, AngleOf(toPlayer), 5.0f * dt);

    e.timer2 -= dt;
    if (e.timer2 <= -3.0f) {  // occasionally flip strafe direction
        e.timer2 = 0.0f;
        e.param = -e.param;
    }
    e.timer -= dt;
    if (e.state == 0 && e.timer <= 0.0f) {
        e.state = 1;  // charging (rendered as a glow)
        e.timer = 0.4f;
    } else if (e.state == 1 && e.timer <= 0.0f) {
        const float speed = 330.0f;
        const Vector2 lead = player_.position + player_.velocity * 0.25f;
        const float angle = AngleOf(lead - e.position);
        if (e.elite) {
            FireFan(e.position, angle, 5, 0.22f, speed, EnemyBulletStyle::Orb, kEnemyBulletOrange);
        } else {
            FireFan(e.position, angle, director_.Sector() >= 3 ? 3 : 1, 0.2f, speed, EnemyBulletStyle::Orb,
                    kEnemyBulletOrange);
        }
        Emit(EventType::EnemyShot, e.position, kEnemyBulletOrange, 1.0f, FromAngle(angle));
        e.state = 0;
        e.timer = rng_.Range(2.0f, 2.8f);
    }
}

void World::BehaveSwarmer(Enemy& e, float dt) {
    const float noise = std::sin(e.age * 5.0f + static_cast<float>(e.id) * 1.7f) * 0.45f;
    const Vector2 dir = Rotated(Normalized(ToPlayer(e)), noise);
    e.velocity = DampV(e.velocity, dir * (e.speed * SpeedScale(e)), 0.18f, dt);
    e.angle = AngleOf(e.velocity);
}

void World::BehaveHive(Enemy& e, float dt) {
    const Vector2 toPlayer = ToPlayer(e);
    const float dist = Length(toPlayer);
    const Vector2 dir = Normalized(toPlayer);
    const Vector2 desired = dist > 650.0f ? dir : (dist < 450.0f ? -dir : Perp(dir));
    e.velocity = DampV(e.velocity, desired * (e.speed * SpeedScale(e)), 0.8f, dt);
    e.angle += dt * 0.6f;

    e.timer -= dt;
    if (e.timer <= 0.0f) {
        e.timer = e.elite ? 2.2f : 3.4f;
        if (activeEnemies_ < kMaxEnemies - 40) {
            const int count = e.elite ? 5 : 3;
            for (int i = 0; i < count; ++i) {
                const Vector2 offset = FromAngle(e.angle + kTau * static_cast<float>(i) / count, e.radius + 14.0f);
                Enemy& s = SpawnEnemy(EnemyKind::Swarmer, e.position + offset, 0.25f);
                s.velocity = Normalized(offset) * 260.0f;
                s.dropsLoot = rng_.Chance(0.5f);  // keep hives from being xp farms
            }
        }
    }
}

void World::BehaveSingularity(Enemy& e, float dt) {
    if (LengthSq(e.velocity) < 1.0f) {
        e.param = rng_.Angle();
        e.velocity = FromAngle(e.param, e.speed);
    }
    e.velocity = DampV(e.velocity, FromAngle(e.param, e.speed * SpeedScale(e)), 1.0f, dt);
    e.angle += dt * 1.5f;
}

void World::BehaveWeaver(Enemy& e, float dt) {
    if (e.state == 0) {
        // Grow a tail the first time the head becomes active.
        e.state = 1;
        const int segments = (e.elite ? 16 : 10) + std::min(director_.Sector(), 4);
        EntityId leader = e.id;
        for (int i = 0; i < segments; ++i) {
            Enemy& body = SpawnEnemy(EnemyKind::WeaverBody, e.position, 0.0f);
            body.leader = leader;
            body.segment = i + 1;
            body.hp = body.maxHp = 1.0f;
            leader = body.id;
        }
    }
    const float weave = std::sin(e.age * 2.2f) * 0.85f;
    const Vector2 dir = Rotated(Normalized(ToPlayer(e)), weave);
    e.velocity = DampV(e.velocity, dir * (e.speed * SpeedScale(e)), 0.3f, dt);
    e.angle = AngleOf(e.velocity);
}

void World::BehaveWeaverBody(Enemy& e, float dt) {
    const Enemy* leader = FindEnemy(e.leader);
    if (leader == nullptr || !leader->alive) {
        // Leader destroyed: detonate in a rippling chain down the tail.
        if (e.state == 0) {
            e.state = 1;
            e.timer = 0.06f;
        }
        e.timer -= dt;
        if (e.timer <= 0.0f) {
            e.invulnerable = false;
            KillEnemy(e, false);
        }
        return;
    }
    const Vector2 d = e.position - leader->position;
    const float dist = Length(d);
    const float spacing = leader->kind == EnemyKind::WeaverBody ? e.radius * 1.5f : leader->radius + e.radius * 0.6f;
    if (dist > spacing && dist > 0.001f) {
        e.position = leader->position + d / dist * spacing;
    }
    e.angle = AngleOf(leader->position - e.position);
    e.elite = leader->elite;
}

void World::BehaveShielder(Enemy& e, float dt) {
    const Vector2 toPlayer = ToPlayer(e);
    // Slow turning means the player can flank around the shield.
    e.angle = RotateToward(e.angle, AngleOf(toPlayer), (e.elite ? 2.2f : 1.5f) * dt);
    const Vector2 forward = FromAngle(e.angle);
    e.velocity = DampV(e.velocity, forward * (e.speed * SpeedScale(e)), 0.5f, dt);
}

void World::BehaveLancer(Enemy& e, float dt) {
    const Vector2 toPlayer = ToPlayer(e);
    switch (e.state) {
    case 0: {  // reposition at a respectful distance
        e.timer -= dt;
        const Vector2 dir = Normalized(toPlayer);
        const float dist = Length(toPlayer);
        Vector2 desired = Perp(dir) * 0.8f;
        if (dist < 420.0f) {
            desired -= dir;
        } else if (dist > 650.0f) {
            desired += dir;
        }
        e.velocity = DampV(e.velocity, Normalized(desired, {}) * (e.speed * SpeedScale(e)), 0.4f, dt);
        e.angle = RotateToward(e.angle, AngleOf(toPlayer), 4.0f * dt);
        if (e.timer <= 0.0f && dist < 900.0f) {
            e.state = 1;
            e.timer = 1.75f;
            e.angle = AngleOf(toPlayer);
            AddBeam(e.position, e.angle, 1.05f, 0.45f, e.elite ? 34.0f : 24.0f, GetEnemyInfo(e.kind).color, e.id);
        }
        break;
    }
    default:  // charging / firing: hold position
        e.velocity = DampV(e.velocity, {0.0f, 0.0f}, 0.08f, dt);
        e.timer -= dt;
        if (e.timer <= 0.0f) {
            e.state = 0;
            e.timer = rng_.Range(2.2f, 3.2f);
        }
        break;
    }
}

void World::BehaveBomber(Enemy& e, float dt) {
    if (e.state == 0 || DistanceSq(e.position, e.target) < 60.0f * 60.0f) {
        // Pick a run across the arena that passes near the player.
        e.state = 1;
        const Vector2 through = player_.position + rng_.InCircle(220.0f);
        const Vector2 dir = Normalized(through - e.position, rng_.UnitVector());
        e.target = through + dir * 900.0f;
        e.target.x = std::clamp(e.target.x, kArena.x + 80.0f, kArena.x + kArena.width - 80.0f);
        e.target.y = std::clamp(e.target.y, kArena.y + 80.0f, kArena.y + kArena.height - 80.0f);
    }
    SteerToward(e, e.target, e.speed * SpeedScale(e), 0.4f, dt);
    e.angle = AngleOf(e.velocity);

    e.timer -= dt;
    if (e.timer <= 0.0f) {
        e.timer = e.elite ? 0.55f : 0.9f;
        Enemy& mine = SpawnEnemy(EnemyKind::Mine, e.position - Normalized(e.velocity) * e.radius, 0.0f);
        mine.timer = 0.7f;  // arming delay
        mine.contactDamage = false;
    }
}

void World::BehaveMine(Enemy& e, float dt) {
    e.velocity = {};
    e.angle += dt * 2.0f;
    e.timer -= dt;
    if (e.age > 14.0f && e.state == 0) {
        KillEnemy(e, false);
        return;
    }
    const float distSq = DistanceSq(e.position, player_.position);
    if (e.state == 0 && e.timer <= 0.0f && player_.alive && distSq < 95.0f * 95.0f) {
        e.state = 1;  // fuse lit
        e.timer = 0.55f;
        Emit(EventType::MineArmed, e.position, kMineColor);
    } else if (e.state == 1 && e.timer <= 0.0f) {
        const float blast = 115.0f;
        if (DistanceSq(e.position, player_.position) < blast * blast) {
            DamagePlayer(e.position);
        }
        Emit(EventType::Explosion, e.position, kMineColor, blast);
        e.alive = false;
    }
}

void World::BehaveBossMinion(Enemy& e, float dt) {
    if (e.leader != kInvalidId) {
        // Shield node orbiting its boss.
        const Enemy* boss = FindEnemy(e.leader);
        if (boss == nullptr || !boss->alive) {
            KillEnemy(e, false);
            return;
        }
        e.param += dt * 0.8f;
        const Vector2 slot = boss->position + FromAngle(e.param, boss->radius + 110.0f);
        e.velocity = (slot - e.position) * 8.0f;
    } else {
        // Stationary turret.
        e.velocity = DampV(e.velocity, {0.0f, 0.0f}, 0.2f, dt);
    }
    e.angle = RotateToward(e.angle, AngleOf(ToPlayer(e)), 3.0f * dt);
    e.timer -= dt;
    if (e.timer <= 0.0f) {
        e.timer = rng_.Range(1.6f, 2.4f);
        FireFan(e.position, AngleOf(ToPlayer(e)), 3, 0.18f, 320.0f, EnemyBulletStyle::Orb, GetEnemyInfo(e.kind).color);
        Emit(EventType::EnemyShot, e.position, GetEnemyInfo(e.kind).color, 1.0f);
    }
}

void World::FireRing(Vector2 center, int count, float speed, float offset, EnemyBulletStyle style, Color color,
                     float radius) {
    for (int i = 0; i < count; ++i) {
        const float angle = offset + kTau * static_cast<float>(i) / static_cast<float>(count);
        FireEnemyBullet(center + FromAngle(angle, 10.0f), FromAngle(angle, speed), style, color, radius);
    }
}

void World::FireFan(Vector2 center, float angle, int count, float spread, float speed, EnemyBulletStyle style,
                    Color color, float radius) {
    for (int i = 0; i < count; ++i) {
        const float k = static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f;
        const float a = angle + k * spread;
        FireEnemyBullet(center + FromAngle(a, 12.0f), FromAngle(a, speed), style, color, radius);
    }
}

void World::OnEnemyDeath(Enemy& e) {
    switch (e.kind) {
    case EnemyKind::Splitter: {
        const int count = e.elite ? 5 : 3;
        for (int i = 0; i < count; ++i) {
            const Vector2 dir = FromAngle(e.angle + kTau * static_cast<float>(i) / count);
            Enemy& mini = SpawnEnemy(EnemyKind::SplitterMini, e.position + dir * 12.0f, 0.0f);
            mini.knockback = dir * 420.0f;
            mini.age = 0.0f;
        }
        break;
    }
    case EnemyKind::Singularity:
        // Collapsing wells take their neighbours with them.
        Explode(e.position, 240.0f, 150.0f * ModulePower(), e.id, GetEnemyInfo(e.kind).color);
        break;
    default:
        break;
    }
    if (IsBoss(e.kind)) {
        for (Enemy& other : enemies_) {
            if (other.alive && (other.kind == EnemyKind::BossMinion || other.kind == EnemyKind::Mine)) {
                KillEnemy(other, true);
            }
        }
        for (Enemy& other : pendingSpawns_) {
            other.alive = false;
        }
        ClearEnemyBullets(e.position, 1e6f);
        for (Beam& b : beams_) {
            b.alive = false;
        }
    }
}

} // namespace gb
