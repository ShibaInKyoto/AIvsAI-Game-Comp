#include "game/Bot.h"

#include <algorithm>

namespace gb {

namespace {

float UpgradeScore(const World& world, UpgradeId id) {
    const UpgradeInfo& info = GetUpgradeInfo(id);
    const int owned = world.Inventory().Stacks(id);
    float score = 1.0f + static_cast<float>(info.rarity) * 0.6f;
    switch (info.category) {
    case UpgradeCategory::Module: score += owned > 0 ? 2.5f : 2.0f; break;
    case UpgradeCategory::Weapon: score += 1.5f; break;
    case UpgradeCategory::Defense: score += 0.8f; break;
    default: break;
    }
    const Player& p = world.GetPlayer();
    if (id == UpgradeId::NanoRepair || id == UpgradeId::HullPlating) {
        score += static_cast<float>(world.Stats().maxHp - p.hp) * 1.5f;
    }
    return score;
}

} // namespace

UpgradeId Bot::ChooseUpgrade(const World& world, const std::vector<UpgradeId>& offers) {
    UpgradeId best = offers.front();
    float bestScore = -1.0f;
    for (UpgradeId id : offers) {
        const float s = UpgradeScore(world, id);
        if (s > bestScore) {
            bestScore = s;
            best = id;
        }
    }
    return best;
}

SectorModifier Bot::ChooseModifier(const World& world, SectorModifier a, SectorModifier b) {
    const Player& p = world.GetPlayer();
    if (p.hp <= 2 && (a == SectorModifier::Respite || b == SectorModifier::Respite)) {
        return SectorModifier::Respite;
    }
    return a == SectorModifier::Hardened ? b : a;
}

PlayerInput Bot::Think(World& world, float dt) {
    PlayerInput input;
    const Player& p = world.GetPlayer();
    if (!p.alive) {
        return input;
    }
    const Rectangle arena = World::kArena;
    Vector2 force{};

    // Enemies: keep a healthy distance, scaled by how close they are.
    const Enemy* target = nullptr;
    float bestTargetScore = 1e9f;
    int nearby = 0;
    for (const Enemy& e : world.Enemies()) {
        if (!e.alive) {
            continue;
        }
        const Vector2 d = p.position - e.position;
        const float dist = std::max(Length(d), 1.0f);
        const float danger = e.radius + 150.0f + (IsBoss(e.kind) ? 220.0f : 0.0f);
        if (dist < danger && e.contactDamage) {
            force += d / dist * ((danger - dist) / danger) * 3.0f;
        }
        if (dist < 400.0f) {
            ++nearby;
        }
        if (e.Active() && !e.invulnerable && e.kind != EnemyKind::Mine) {
            const float score = dist - (IsBoss(e.kind) ? 300.0f : 0.0f);
            if (score < bestTargetScore) {
                bestTargetScore = score;
                target = &e;
            }
        }
    }

    // Bullets: dodge perpendicular to their path when they are headed our way.
    bool emergency = false;
    for (const EnemyBullet& b : world.EnemyBullets()) {
        const Vector2 d = p.position - b.position;
        const float distSq = LengthSq(d);
        if (distSq > 260.0f * 260.0f) {
            continue;
        }
        const Vector2 v = Normalized(b.velocity);
        const float along = Dot(d, v);
        if (along < -20.0f) {
            continue;  // moving away
        }
        const Vector2 closest = b.position + v * along;
        const Vector2 off = p.position - closest;
        const float miss = Length(off);
        if (miss < 60.0f) {
            const Vector2 away = miss > 0.1f ? off / miss : Perp(v);
            force += away * (1.0f - miss / 60.0f) * 5.0f;
            if (miss < 22.0f && along < 90.0f) {
                emergency = true;
            }
        }
    }

    // Beams: get out of any telegraphed line.
    for (const Beam& beam : world.Beams()) {
        const Vector2 dir = FromAngle(beam.angle);
        const Vector2 rel = p.position - beam.origin;
        const float along = Dot(rel, dir);
        if (along < 0.0f) {
            continue;
        }
        const float side = Cross(dir, rel);
        if (std::abs(side) < beam.width + 60.0f) {
            force += Perp(dir) * (side >= 0.0f ? 1.0f : -1.0f) * 6.0f;
            if (beam.Firing() || beam.warmup < 0.25f) {
                emergency = emergency || std::abs(side) < beam.width;
            }
        }
    }

    // Walls and arena centre.
    const float wallMargin = 180.0f;
    if (p.position.x < arena.x + wallMargin) force.x += 2.0f;
    if (p.position.x > arena.x + arena.width - wallMargin) force.x -= 2.0f;
    if (p.position.y < arena.y + wallMargin) force.y += 2.0f;
    if (p.position.y > arena.y + arena.height - wallMargin) force.y -= 2.0f;
    force += (Vector2{0.0f, 0.0f} - p.position) * 0.0004f;

    // Shards: drift toward the nearest one when nothing is pressing.
    if (LengthSq(force) < 1.0f) {
        const Pickup* nearest = nullptr;
        float best = 600.0f * 600.0f;
        for (const Pickup& pickup : world.Pickups()) {
            const float d = DistanceSq(pickup.position, p.position);
            if (d < best) {
                best = d;
                nearest = &pickup;
            }
        }
        if (nearest != nullptr) {
            force += Normalized(nearest->position - p.position) * 0.8f;
        }
    }

    // Gentle wander so the bot never stands still.
    wanderTimer_ -= dt;
    if (wanderTimer_ <= 0.0f) {
        wanderTimer_ = 1.5f;
        wander_ = world.Random().UnitVector() * 0.35f;
    }
    force += wander_;

    input.move = LengthSq(force) > 0.01f ? Normalized(force) : Vector2{};

    if (target != nullptr) {
        const Vector2 lead = target->position + target->velocity * 0.12f;
        input.aim = Normalized(lead - p.position);
        input.fire = true;
    } else {
        input.aim = FromAngle(p.aimAngle);
    }

    dashCooldown_ -= dt;
    if (emergency && dashCooldown_ <= 0.0f && p.dashCharges > 0) {
        input.dash = true;
        dashCooldown_ = 0.4f;
    }
    input.nova = p.novaCharge >= 1.0f && (nearby > 12 || p.hp <= 1 || world.EnemyBullets().size() > 120);
    return input;
}

} // namespace gb
