// Secondary weapon modules. Each module's level comes from the upgrade
// inventory (PlayerStats::moduleLevel); tuning per level lives right here.

#include "game/EnemyCatalog.h"
#include "game/World.h"

#include <algorithm>
#include <array>

namespace gb {

namespace {

constexpr Color kBladeColor{140, 255, 200, 255};
constexpr Color kArcColor{150, 200, 255, 255};
constexpr Color kMissileColor{255, 150, 80, 255};
constexpr Color kPulseColor{120, 230, 255, 255};
constexpr Color kMineColor{255, 220, 90, 255};
constexpr Color kLanceColor{255, 110, 230, 255};

int ModuleLevel(const PlayerStats& stats, ModuleId id) { return std::clamp(stats.Module(id), 0, 5); }

} // namespace

void World::UpdateModules(float dt) {
    if (!player_.alive) {
        return;
    }
    UpdateOrbitBlades(dt);
    UpdateDrones(dt);
    UpdateArcCoil(dt);
    UpdateMissiles(dt);
    UpdatePulseNova(dt);
    UpdateMines(dt);
    UpdatePrismLance(dt);
}

void World::UpdateOrbitBlades(float dt) {
    const Vector2 origin = player_.position;
    const float power = ModulePower();
    if (const int level = ModuleLevel(stats_, ModuleId::OrbitBlades); level > 0) {
        const bool evolved = stats_.Evolved(ModuleId::OrbitBlades);
        orbitAngle_ += dt * (level >= 5 ? 4.4f : 3.3f);
        const int blades = OrbitBladeCount();
        const float radius = OrbitRadius();
        const float damage = (level >= 3 ? 26.0f : 19.0f) * power * (evolved ? 1.8f : 1.0f);
        for (int i = 0; i < blades; ++i) {
            const Vector2 blade = origin + FromAngle(orbitAngle_ + kTau * static_cast<float>(i) / blades, radius);
            enemyIndex_.Query(blade, 18.0f, [&](int index) {
                Enemy& e = enemies_[static_cast<std::size_t>(index)];
                if (!e.Active() || e.bladeCooldown > 0.0f) {
                    return;
                }
                const float reach = e.radius + 18.0f;
                if (DistanceSq(e.position, blade) < reach * reach) {
                    e.bladeCooldown = 0.3f;
                    DamageEnemy(e, damage, false, Normalized(e.position - origin), 220.0f);
                }
            });
            // Blades also swat enemy bullets out of the air.
            for (EnemyBullet& b : enemyBullets_) {
                if (b.alive && DistanceSq(b.position, blade) < (b.radius + 16.0f) * (b.radius + 16.0f)) {
                    b.alive = false;
                    Emit(EventType::EnemyShieldBlock, b.position, kBladeColor, 0.4f);
                }
            }
        }
    }
}

void World::UpdateDrones(float dt) {
    const Vector2 origin = player_.position;
    if (const int level = ModuleLevel(stats_, ModuleId::Drones); level > 0 && !drones_.empty()) {
        const bool hive = stats_.Evolved(ModuleId::Drones);
        const float interval = level >= 3 ? 0.3f : 0.45f;
        for (std::size_t i = 0; i < drones_.size(); ++i) {
            Drone& d = drones_[i];
            const float slotAngle =
                player_.aimAngle + kPi + (static_cast<float>(i) - (drones_.size() - 1) * 0.5f) * 1.1f;
            const Vector2 slot =
                origin + FromAngle(slotAngle, 64.0f) +
                Vector2{std::sin(runStats_.time * 2.0f + i), std::cos(runStats_.time * 1.7f + i)} * 8.0f;
            d.position = DampV(d.position, slot, 0.09f, dt);
            d.cooldown -= dt;
            Enemy* target = NearestEnemy(d.position, 650.0f);
            if (target != nullptr) {
                d.angle = RotateToward(d.angle, AngleOf(target->position - d.position), 12.0f * dt);
            } else {
                d.angle = RotateToward(d.angle, player_.aimAngle, 6.0f * dt);
            }
            if (target != nullptr && d.cooldown <= 0.0f) {
                d.cooldown = interval;
                const int shots = hive ? 3 : (level >= 5 ? 2 : 1);
                for (int s = 0; s < shots; ++s) {
                    if (static_cast<int>(bullets_.size()) >= 3000) {
                        break;
                    }
                    Bullet b;
                    const float spread =
                        hive ? (static_cast<float>(s) - 1.0f) * 0.16f : (shots > 1 ? (s == 0 ? -0.06f : 0.06f) : 0.0f);
                    b.position = d.position;
                    b.velocity = FromAngle(d.angle + spread, 1350.0f);
                    b.radius = 4.5f;
                    b.damage = stats_.damage * stats_.powerMultiplier;
                    b.life = 0.6f;
                    b.pierce = level >= 5 ? 2 : 0;
                    bullets_.push_back(b);
                }
                Emit(EventType::PlayerShot, d.position, {120, 255, 180, 255}, 0.4f, FromAngle(d.angle));
            }
        }
    }
}

void World::UpdateArcCoil(float dt) {
    const Vector2 origin = player_.position;
    const float power = ModulePower();
    if (const int level = ModuleLevel(stats_, ModuleId::ArcCoil); level > 0) {
        arcTimer_ -= dt;
        if (arcTimer_ <= 0.0f) {
            static constexpr std::array<int, 6> kChains{0, 3, 5, 5, 7, 7};
            const bool storm = stats_.Evolved(ModuleId::ArcCoil);
            const int chains = storm ? 12 : kChains[static_cast<std::size_t>(level)];
            const float damage = (level >= 3 ? 34.0f : 24.0f) * power;
            const int arcs = level >= 5 ? 2 : 1;
            bool fired = false;
            std::vector<EntityId> hit;
            for (int a = 0; a < arcs; ++a) {
                ArcTrace trace;
                trace.points.push_back(origin);
                Vector2 from = origin;
                float range = 520.0f;
                for (int c = 0; c < chains; ++c) {
                    Enemy* best = nullptr;
                    float bestDist = range * range;
                    enemyIndex_.Query(from, range, [&](int index) {
                        Enemy& e = enemies_[static_cast<std::size_t>(index)];
                        if (!e.Active() || e.invulnerable || std::find(hit.begin(), hit.end(), e.id) != hit.end()) {
                            return;
                        }
                        const float d = DistanceSq(e.position, from);
                        if (d < bestDist) {
                            bestDist = d;
                            best = &e;
                        }
                    });
                    if (best == nullptr) {
                        break;
                    }
                    hit.push_back(best->id);
                    trace.points.push_back(best->position);
                    from = best->position;
                    range = 280.0f;
                    const bool crit = storm && rng_.Chance(stats_.critChance);
                    DamageEnemy(*best, damage * (crit ? stats_.critMultiplier : 1.0f), crit,
                                Normalized(best->position - origin), 60.0f);
                }
                if (trace.points.size() > 1) {
                    fired = true;
                    Emit(EventType::ArcZap, trace.points.back(), kArcColor, static_cast<float>(trace.points.size()));
                    arcs_.push_back(std::move(trace));
                }
            }
            arcTimer_ = fired ? (storm ? 0.8f : (level >= 3 ? 1.5f : 2.0f)) : 0.3f;
        }
    }
}

void World::UpdateMissiles(float dt) {
    const Vector2 origin = player_.position;
    const float power = ModulePower();
    if (const int level = ModuleLevel(stats_, ModuleId::Missiles); level > 0) {
        missileTimer_ -= dt;
        if (missileTimer_ <= 0.0f && NearestEnemy(origin, 900.0f) != nullptr) {
            static constexpr std::array<int, 6> kCount{0, 2, 3, 3, 5, 5};
            missileTimer_ = level >= 5 ? 2.2f : 3.0f;
            const int count = stats_.Evolved(ModuleId::Missiles) ? 10 : kCount[static_cast<std::size_t>(level)];
            for (int i = 0; i < count; ++i) {
                Missile m;
                m.position = origin;
                const float spread = (static_cast<float>(i) - (count - 1) * 0.5f) * 0.5f;
                m.velocity = FromAngle(player_.aimAngle + kPi + spread, 380.0f);
                m.damage = (level >= 5 ? 48.0f : 32.0f) * power;
                m.radius = level >= 3 ? 125.0f : 90.0f;
                missiles_.push_back(m);
            }
            Emit(EventType::MissileLaunch, origin, kMissileColor, static_cast<float>(count));
        }
    }
    for (Missile& m : missiles_) {
        if (!m.alive) {
            continue;
        }
        m.age += dt;
        m.life -= dt;
        Enemy* target = FindEnemy(m.target);
        if (target == nullptr || !target->Active()) {
            target = NearestEnemy(m.position, 1000.0f);
            m.target = target != nullptr ? target->id : kInvalidId;
        }
        const float speed = std::min(1100.0f, 380.0f + m.age * 1400.0f);
        if (target != nullptr && m.age > 0.15f) {
            const float turn = 7.0f * dt;
            const float angle = RotateToward(AngleOf(m.velocity), AngleOf(target->position - m.position), turn);
            m.velocity = FromAngle(angle, speed);
        } else {
            m.velocity = Normalized(m.velocity) * speed;
        }
        m.position += m.velocity * dt;
        const bool hitTarget = target != nullptr && DistanceSq(m.position, target->position) <
                                                        (target->radius + 10.0f) * (target->radius + 10.0f);
        const bool outside = m.position.x < kArena.x || m.position.x > kArena.x + kArena.width ||
                             m.position.y < kArena.y || m.position.y > kArena.y + kArena.height;
        if (hitTarget || outside || m.life <= 0.0f) {
            m.alive = false;
            Explode(m.position, m.radius, m.damage, kInvalidId, kMissileColor);
            if (stats_.Evolved(ModuleId::Missiles)) {
                for (int i = 0; i < 3; ++i) {
                    const Vector2 p = m.position + FromAngle(rng_.Angle(), m.radius * 0.7f);
                    Explode(p, m.radius * 0.55f, m.damage * 0.5f, kInvalidId, kMissileColor);
                }
            }
        }
    }
}

void World::UpdatePulseNova(float dt) {
    const Vector2 origin = player_.position;
    const float power = ModulePower();
    if (const int level = ModuleLevel(stats_, ModuleId::PulseNova); level > 0) {
        pulseTimer_ -= dt;
        if (pulseTimer_ <= 0.0f) {
            const bool reactor = stats_.Evolved(ModuleId::PulseNova);
            pulseTimer_ = reactor ? 1.5f : (level >= 3 ? 3.0f : 4.0f);
            float radius = 260.0f * (reactor ? 1.15f : 1.0f);
            if (level >= 2) radius *= 1.3f;
            if (level >= 4) radius *= 1.3f;
            const float damage = (level >= 3 ? 32.0f : 22.0f) * power * (reactor ? 1.6f : 1.0f);
            AddShockwave(origin, radius, 1300.0f, damage, 750.0f, level >= 5);
            Emit(EventType::PulseNova, origin, kPulseColor, radius);
        }
    }
}

void World::UpdateMines(float dt) {
    const Vector2 origin = player_.position;
    const float power = ModulePower();
    if (const int level = ModuleLevel(stats_, ModuleId::Mines); level > 0) {
        mineTimer_ -= dt * (Length(player_.velocity) > 80.0f ? 1.0f : 0.35f);
        if (mineTimer_ <= 0.0f && playerMines_.size() < 14) {
            mineTimer_ = level >= 2 ? 0.85f : 1.15f;
            PlayerMine mine;
            mine.position = origin - Normalized(player_.velocity, FromAngle(player_.aimAngle)) * 20.0f;
            const float gravity = stats_.Evolved(ModuleId::Mines) ? 1.5f : 1.0f;
            mine.damage = (level >= 4 ? 66.0f : 44.0f) * power * gravity;
            mine.radius = (level >= 3 ? 155.0f : 110.0f) * gravity;
            playerMines_.push_back(mine);
        }
    }
    for (PlayerMine& mine : playerMines_) {
        if (!mine.alive) {
            continue;
        }
        mine.armTime -= dt;
        mine.life -= dt;
        bool triggered = mine.life <= 0.0f;
        if (mine.armTime <= 0.0f && !triggered) {
            enemyIndex_.Query(mine.position, 46.0f, [&](int index) {
                const Enemy& e = enemies_[static_cast<std::size_t>(index)];
                if (e.Active() && e.kind != EnemyKind::Mine &&
                    DistanceSq(e.position, mine.position) < (e.radius + 46.0f) * (e.radius + 46.0f)) {
                    triggered = true;
                }
            });
        }
        if (triggered) {
            mine.alive = false;
            if (stats_.Evolved(ModuleId::Mines)) {
                // Gravity mines yank everything nearby into the blast first.
                enemyIndex_.Query(mine.position, mine.radius * 2.0f, [&](int index) {
                    Enemy& e = enemies_[static_cast<std::size_t>(index)];
                    if (e.Active() && !IsBoss(e.kind)) {
                        e.position = LerpV(e.position, mine.position, 0.55f);
                    }
                });
            }
            Explode(mine.position, mine.radius, mine.damage, kInvalidId, kMineColor);
            if (ModuleLevel(stats_, ModuleId::Mines) >= 5) {
                for (int i = 0; i < 3; ++i) {
                    const Vector2 p =
                        mine.position + FromAngle(kTau * static_cast<float>(i) / 3.0f, mine.radius * 0.8f);
                    Explode(p, mine.radius * 0.6f, mine.damage * 0.5f, kInvalidId, kMineColor);
                }
            }
        }
    }
}

void World::UpdatePrismLance(float dt) {
    const Vector2 origin = player_.position;
    const float power = ModulePower();
    if (const int level = ModuleLevel(stats_, ModuleId::PrismLance); level > 0) {
        lanceTimer_ -= dt;
        if (lanceTimer_ <= 0.0f) {
            const bool spectrum = stats_.Evolved(ModuleId::PrismLance);
            lanceTimer_ = spectrum ? 1.0f : (level >= 3 ? 2.0f : 2.5f);
            float damage = 48.0f * power;
            if (level >= 2) damage *= 1.4f;
            if (level >= 4) damage *= 1.4f;
            const float width = level >= 3 ? 34.0f : 22.0f;
            const float length = 1500.0f;
            const int beams = spectrum ? 5 : (level >= 5 ? 3 : 1);
            const float fan = spectrum ? 0.22f : 0.32f;
            for (int i = 0; i < beams; ++i) {
                const float angle = player_.aimAngle + (static_cast<float>(i) - (beams - 1) * 0.5f) * fan;
                const Vector2 dir = FromAngle(angle);
                const Vector2 end = origin + dir * length;
                // Damage everything whose body touches the beam segment.
                for (Enemy& e : enemies_) {
                    if (!e.Active() || e.invulnerable) {
                        continue;
                    }
                    const float t = std::clamp(Dot(e.position - origin, dir), 0.0f, length);
                    const Vector2 closest = origin + dir * t;
                    const float reach = e.radius + width * 0.5f;
                    if (DistanceSq(closest, e.position) < reach * reach) {
                        DamageEnemy(e, damage, false, dir, 160.0f);
                    }
                }
                lances_.push_back(LanceTrace{origin, end, width, 0.28f, 0.28f});
            }
            Emit(EventType::PlayerLance, origin, kLanceColor, static_cast<float>(beams), FromAngle(player_.aimAngle));
        }
    }
}

} // namespace gb
