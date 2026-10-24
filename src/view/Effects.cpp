#include "view/Effects.h"

#include "audio/AudioSystem.h"
#include "core/Settings.h"
#include "platform/Rumble.h"
#include "game/EnemyCatalog.h"
#include "render/CameraRig.h"
#include "render/Particles.h"
#include "render/WarpGrid.h"
#include "view/Palette.h"

#include <algorithm>
#include <cstdio>

namespace gb {

namespace {

constexpr std::size_t kMaxTexts = 90;

} // namespace

EffectsDirector::EffectsDirector(ParticleSystem& particles, WarpGrid& grid, CameraRig& camera, AudioSystem& audio,
                                 Rumble& rumble, const Settings& settings)
    : particles_(particles), grid_(grid), camera_(camera), audio_(audio), rumble_(rumble), settings_(settings) {
    texts_.reserve(kMaxTexts);
}

float EffectsDirector::TimeScale() const {
    if (hitStop_ > 0.0f) {
        return 0.05f;
    }
    return slowMo_ > 0.0f ? slowMoScale_ : 1.0f;
}

float EffectsDirector::Pan(Vector2 position) const {
    return std::clamp((position.x - camera_.Position().x) / 900.0f, -1.0f, 1.0f) * 0.6f;
}

void EffectsDirector::PlaySfx(SfxId id, float volume, float pitch, float pan) {
    if (!muted_) {
        audio_.Play(id, volume, pitch, pan);
    }
}

void EffectsDirector::Shake(float trauma) { camera_.AddTrauma(trauma * settings_.screenShake); }

void EffectsDirector::Ripple(Vector2 position, float maxRadius, float speed, float strength) {
    if (distortions_.size() >= 8) {
        distortions_.erase(distortions_.begin());
    }
    distortions_.push_back({position, 0.0f, maxRadius, speed, strength});
}

void EffectsDirector::Vibrate(float low, float high, float duration) {
    if (!muted_) {
        rumble_.Pulse(low, high, duration);
    }
}

void EffectsDirector::Flash(Color color, float amount) {
    flashColor_ = color;
    flashAmount_ = std::max(flashAmount_, amount);
}

void EffectsDirector::AddText(Vector2 position, const char* text, Color color, float size, bool big) {
    if (texts_.size() >= kMaxTexts) {
        texts_.erase(texts_.begin());
    }
    FloatingText t;
    t.position = position;
    t.velocity = {particles_.Random().Range(-30.0f, 30.0f), big ? -60.0f : -110.0f};
    std::snprintf(t.text.data(), t.text.size(), "%s", text);
    t.color = color;
    t.maxLife = t.life = big ? 1.6f : 0.75f;
    t.size = size;
    t.big = big;
    texts_.push_back(t);
}

void EffectsDirector::AddDamage(EntityId enemy, Vector2 position, float amount, bool crit) {
    // Merge into a recent number on the same enemy so streams of small hits
    // (beams, blades, drones) read as one climbing total instead of a cloud.
    constexpr float kMergeWindow = 0.45f;
    if (enemy != kInvalidId) {
        for (auto it = texts_.rbegin(); it != texts_.rend(); ++it) {
            FloatingText& t = *it;
            if (t.owner != enemy || t.maxLife - t.life > kMergeWindow) {
                continue;
            }
            t.total += amount;
            std::snprintf(t.text.data(), t.text.size(), "%d", static_cast<int>(std::round(t.total)));
            t.life = t.maxLife;
            t.pop = 1.0f;
            t.position = Vector2Lerp(t.position, position + Vector2{0.0f, -18.0f}, 0.5f);
            t.velocity.y = std::min(t.velocity.y, -40.0f);
            const float grown = 18.0f + 2.2f * std::log2(1.0f + t.total / std::max(amount, 1.0f));
            t.size = std::max(t.size, std::min(30.0f, grown));
            if (crit) {
                t.color = palette::kPlayerBulletCrit;
            }
            return;
        }
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::round(amount)));
    AddText(position + Vector2{particles_.Random().Range(-10.0f, 10.0f), -14.0f}, buffer,
            crit ? palette::kPlayerBulletCrit : Color{230, 235, 255, 210}, crit ? 26.0f : 18.0f);
    FloatingText& t = texts_.back();
    t.owner = enemy;
    t.total = amount;
    t.pop = crit ? 1.0f : 0.0f;
}

void EffectsDirector::Process(const World& world, const std::vector<GameEvent>& events) {
    Rng& rng = particles_.Random();
    for (const GameEvent& ev : events) {
        const float pan = Pan(ev.position);
        switch (ev.type) {
        case EventType::PlayerShot: {
            const bool drone = ev.magnitude < 1.0f;
            particles_.Spray(ev.position, ev.direction, 0.35f, drone ? 1 : 2, ev.color, 200.0f, 500.0f, 0.06f, 0.14f,
                             8.0f, 2.0f);
            particles_.GlowPuff(ev.position, drone ? 10.0f : 18.0f, 0.06f, ev.color);
            if (!drone) {
                const ShipId ship = world.Config().ship;
                const SfxId sfx = ship == ShipId::Lancer    ? SfxId::ShootHeavy
                                  : ship == ShipId::Bruiser ? SfxId::ShootScatter
                                                            : SfxId::Shoot;
                PlaySfx(sfx, sfx == SfxId::Shoot ? 0.45f : 0.6f, rng.Range(0.92f, 1.08f), pan);
                camera_.Kick(ev.direction * -1.0f, ship == ShipId::Lancer ? 7.0f : 1.6f);
            } else {
                PlaySfx(SfxId::Shoot, 0.18f, rng.Range(1.4f, 1.6f), pan);
            }
            break;
        }
        case EventType::EnemyHit: {
            const bool crit = ev.value != 0;
            particles_.Spray(ev.position, ev.direction * -1.0f, 0.8f, crit ? 7 : 3, LerpColor(ev.color, WHITE, 0.4f),
                             150.0f, 550.0f, 0.12f, 0.3f, 9.0f, 2.2f);
            grid_.ApplyDirectedForce(ev.direction * 4.0f, ev.position, 70.0f);
            PlaySfx(crit ? SfxId::HitCrit : SfxId::Hit, crit ? 0.5f : 0.32f, rng.Range(0.9f, 1.15f), pan);
            if (settings_.damageNumbers) {
                AddDamage(ev.source, ev.position, ev.magnitude, crit);
            }
            break;
        }
        case EventType::EnemyKilled: {
            const float size = ev.magnitude;
            const bool big = size >= 30.0f;
            const bool boss = IsBoss(ev.enemy);
            const int sparks = std::clamp(static_cast<int>(size * 1.6f), 14, 90);
            particles_.Burst(ev.position, sparks, ev.color, 150.0f, 650.0f + size * 8.0f, 0.35f, 0.9f, 16.0f, 3.0f);
            particles_.Burst(ev.position, sparks / 3, WHITE, 100.0f, 400.0f, 0.2f, 0.45f, 10.0f, 2.0f);
            particles_.Debris(ev.position, big ? 8 : 4, ev.color, 300.0f + size * 4.0f, size * 0.9f);
            particles_.ShockRing(ev.position, size * 0.5f, size * 4.0f, 0.35f, ev.color);
            particles_.GlowPuff(ev.position, size * 3.0f, 0.3f, ev.color);
            grid_.ApplyExplosiveForce(10.0f + size * 0.8f, ev.position, 90.0f + size * 3.0f);
            if (ev.enemy == EnemyKind::WeaverBody || ev.enemy == EnemyKind::Mine) {
                PlaySfx(SfxId::Hit, 0.3f, rng.Range(0.6f, 0.8f), pan);
            } else if (big || ev.value != 0) {
                PlaySfx(SfxId::EnemyDieBig, 0.6f, rng.Range(0.9f, 1.1f), pan);
                Shake(0.18f);
                Vibrate(0.35f, 0.3f, 0.12f);
            } else {
                PlaySfx(SfxId::EnemyDie, 0.42f, rng.Range(0.85f, 1.2f), pan);
                Shake(0.04f);
            }
            if (ev.value != 0 && !boss) {
                AddText(ev.position, "ELITE DOWN", palette::kElite, 30.0f, true);
            }
            break;
        }
        case EventType::BossDefeated: {
            // A rolling chain of blasts across the hull before the final burst.
            for (int i = 0; i < 9; ++i) {
                const float delay = 0.12f + 0.11f * static_cast<float>(i);
                const Vector2 offset = rng.InCircle(ev.magnitude * 1.6f);
                scheduled_.push_back({delay, ev.position + offset, i % 2 == 0 ? ev.color : WHITE,
                                      rng.Range(40.0f, 90.0f)});
            }
            scheduled_.push_back({1.25f, ev.position, ev.color, 260.0f});
            particles_.Burst(ev.position, 220, ev.color, 300.0f, 1600.0f, 0.6f, 2.0f, 30.0f, 4.0f);
            particles_.Burst(ev.position, 120, WHITE, 200.0f, 900.0f, 0.5f, 1.4f, 20.0f, 3.0f);
            for (int i = 0; i < 4; ++i) {
                particles_.ShockRing(ev.position, 20.0f, 500.0f + 250.0f * i, 0.6f + 0.25f * i, ev.color);
            }
            particles_.Debris(ev.position, 30, ev.color, 900.0f, 40.0f);
            grid_.ApplyExplosiveForce(220.0f, ev.position, 1400.0f);
            PlaySfx(SfxId::BossDeath, 1.0f, 1.0f, pan);
            Ripple(ev.position, 1500.0f, 1300.0f, 0.03f);
            Shake(1.0f);
            Vibrate(1.0f, 0.8f, 0.9f);
            Flash(WHITE, 0.9f);
            aberration_ = std::max(aberration_, 0.02f);
            slowMo_ = 1.6f;
            slowMoScale_ = 0.25f;
            camera_.SetZoomPunch(0.08f);
            break;
        }
        case EventType::EnemySpawnWarning:
            grid_.ApplyImplosiveForce(4.0f * ev.magnitude, ev.position, 140.0f);
            if (ev.magnitude >= 1.5f) {
                PlaySfx(SfxId::Spawn, 0.5f, 0.7f, pan);
            } else {
                PlaySfx(SfxId::Spawn, 0.18f, rng.Range(0.9f, 1.2f), pan);
            }
            break;
        case EventType::EnemyShot:
            PlaySfx(SfxId::EnemyShoot, std::min(0.5f, 0.25f + 0.1f * ev.magnitude), rng.Range(0.9f, 1.1f), pan);
            particles_.GlowPuff(ev.position, 20.0f + 6.0f * ev.magnitude, 0.12f, ev.color);
            break;
        case EventType::EnemyShieldBlock:
            particles_.Spray(ev.position, ev.direction, 0.9f, 3, ev.color, 150.0f, 380.0f, 0.1f, 0.22f, 8.0f, 2.0f);
            if (ev.magnitude > 0.5f) {
                PlaySfx(SfxId::ShieldPing, 0.3f, rng.Range(0.9f, 1.15f), pan);
            }
            break;
        case EventType::BulletWallHit:
            particles_.Spray(ev.position, ev.direction * -1.0f, 1.1f, 2, ev.color, 100.0f, 300.0f, 0.08f, 0.2f, 7.0f,
                             2.0f);
            break;
        case EventType::Explosion: {
            const float r = ev.magnitude;
            particles_.Burst(ev.position, std::clamp(static_cast<int>(r * 0.4f), 10, 70), ev.color, 120.0f,
                             300.0f + r * 4.0f, 0.25f, 0.6f, 14.0f, 3.0f);
            particles_.ShockRing(ev.position, r * 0.2f, r, 0.25f, ev.color);
            particles_.GlowPuff(ev.position, r * 1.2f, 0.25f, ev.color);
            grid_.ApplyExplosiveForce(r * 0.2f, ev.position, r * 1.5f);
            PlaySfx(SfxId::Explosion, std::min(0.75f, 0.3f + r / 400.0f), rng.Range(0.9f, 1.15f), pan);
            if (r >= 120.0f) {
                Ripple(ev.position, r * 1.8f, 900.0f, 0.008f);
            }
            Shake(std::min(0.3f, r / 700.0f));
            break;
        }
        case EventType::PlayerHit: {
            particles_.Burst(ev.position, 70, palette::kDanger, 200.0f, 900.0f, 0.4f, 1.0f, 20.0f, 3.5f);
            particles_.ShockRing(ev.position, 20.0f, 260.0f, 0.35f, palette::kDanger);
            grid_.ApplyExplosiveForce(70.0f, ev.position, 500.0f);
            PlaySfx(SfxId::PlayerHurt, 0.9f, 1.0f, pan);
            Ripple(ev.position, 420.0f, 1100.0f, 0.018f);
            Shake(0.6f);
            Vibrate(0.8f, 1.0f, 0.3f);
            Flash(palette::kDanger, 0.45f);
            aberration_ = std::max(aberration_, 0.018f);
            hitStop_ = 0.09f;
            camera_.SetZoomPunch(0.04f);
            break;
        }
        case EventType::PlayerDeath:
            particles_.Burst(ev.position, 260, WHITE, 300.0f, 1400.0f, 0.6f, 2.0f, 26.0f, 4.0f);
            particles_.Burst(ev.position, 160, palette::kDanger, 200.0f, 900.0f, 0.6f, 1.8f, 20.0f, 3.0f);
            particles_.ShockRing(ev.position, 20.0f, 900.0f, 0.9f, WHITE);
            particles_.Debris(ev.position, 14, WHITE, 600.0f, 26.0f);
            grid_.ApplyExplosiveForce(260.0f, ev.position, 1100.0f);
            PlaySfx(SfxId::PlayerDeath, 1.0f, 1.0f, pan);
            Ripple(ev.position, 1000.0f, 1000.0f, 0.035f);
            Shake(1.0f);
            Vibrate(1.0f, 1.0f, 0.7f);
            Flash(WHITE, 0.8f);
            aberration_ = 0.03f;
            slowMo_ = 2.0f;
            slowMoScale_ = 0.2f;
            break;
        case EventType::PlayerDash:
            particles_.Spray(ev.position, ev.direction * -1.0f, 0.5f, 14, ev.color, 200.0f, 700.0f, 0.15f, 0.35f, 14.0f,
                             2.5f);
            grid_.ApplyDirectedForce(ev.direction * 20.0f, ev.position, 160.0f);
            PlaySfx(SfxId::Dash, 0.6f, rng.Range(0.95f, 1.1f), pan);
            Vibrate(0.0f, 0.25f, 0.07f);
            break;
        case EventType::ShieldBlock:
            particles_.ShockRing(ev.position, 20.0f, 200.0f, 0.4f, palette::kShield);
            particles_.Burst(ev.position, 40, palette::kShield, 200.0f, 700.0f, 0.3f, 0.7f, 14.0f, 3.0f);
            PlaySfx(SfxId::ShieldBlock, 0.8f, 1.0f, pan);
            Shake(0.35f);
            Vibrate(0.4f, 0.7f, 0.2f);
            Flash(palette::kShield, 0.25f);
            AddText(ev.position, "SHIELD BROKEN", palette::kShield, 30.0f, true);
            break;
        case EventType::NovaFired:
            particles_.ShockRing(ev.position, 30.0f, 1100.0f * ev.magnitude, 0.7f, Color{200, 240, 255, 255});
            particles_.ShockRing(ev.position, 10.0f, 700.0f * ev.magnitude, 0.5f, Color{120, 200, 255, 255});
            particles_.Burst(ev.position, 160, Color{180, 230, 255, 255}, 400.0f, 1800.0f, 0.4f, 1.0f, 26.0f, 3.5f);
            grid_.ApplyExplosiveForce(300.0f, ev.position, 1300.0f);
            PlaySfx(SfxId::Nova, 1.0f, 1.0f, pan);
            Ripple(ev.position, 1100.0f * ev.magnitude, 1800.0f, 0.022f);
            Shake(0.75f);
            Vibrate(0.9f, 0.5f, 0.4f);
            Flash(Color{180, 230, 255, 255}, 0.6f);
            aberration_ = std::max(aberration_, 0.015f);
            camera_.SetZoomPunch(0.06f);
            break;
        case EventType::LevelUp:
            particles_.ShockRing(ev.position, 20.0f, 320.0f, 0.5f, palette::kUiGood);
            particles_.Burst(ev.position, 40, palette::kUiGood, 200.0f, 600.0f, 0.4f, 0.9f, 14.0f, 2.5f);
            PlaySfx(SfxId::LevelUp, 0.7f, 1.0f, 0.0f);
            break;
        case EventType::PickupXp:
            if (pickupTimer_ > 0.0f) {
                pickupPitch_ = std::min(pickupPitch_ + 0.035f, 2.0f);
            } else {
                pickupPitch_ = 1.0f;
            }
            pickupTimer_ = 0.35f;
            PlaySfx(SfxId::PickupXp, 0.35f, pickupPitch_, pan);
            particles_.GlowPuff(ev.position, 12.0f + ev.magnitude, 0.12f, ev.color);
            break;
        case EventType::PickupHealth:
            particles_.Burst(ev.position, 24, palette::kHealth, 100.0f, 400.0f, 0.3f, 0.7f, 12.0f, 2.5f);
            PlaySfx(SfxId::PickupHealth, 0.8f, 1.0f, pan);
            AddText(ev.position, "+1 INTEGRITY", palette::kHealth, 30.0f, true);
            break;
        case EventType::PickupCache:
            particles_.ShockRing(ev.position, 10.0f, 260.0f, 0.5f, palette::kCache);
            particles_.Burst(ev.position, 50, palette::kCache, 150.0f, 600.0f, 0.4f, 1.0f, 14.0f, 2.5f);
            PlaySfx(SfxId::PickupCache, 0.9f, 1.0f, pan);
            AddText(ev.position, "DATA CACHE", palette::kCache, 34.0f, true);
            break;
        case EventType::PickupMagnet:
            particles_.ShockRing(ev.position, 10.0f, 1200.0f, 0.8f, palette::kMagnet);
            PlaySfx(SfxId::PickupCache, 0.6f, 1.3f, pan);
            AddText(ev.position, "MAGNET", palette::kMagnet, 34.0f, true);
            break;
        case EventType::LaserCharge:
            PlaySfx(SfxId::LaserCharge, 0.45f, 1.0f / std::max(ev.magnitude, 0.5f), pan);
            break;
        case EventType::LaserFire:
            particles_.Spray(ev.position, ev.direction, 0.25f, 16, ev.color, 300.0f, 1200.0f, 0.2f, 0.5f, 18.0f, 3.0f);
            grid_.ApplyDirectedForce(ev.direction * 30.0f, ev.position + ev.direction * 200.0f, 300.0f);
            PlaySfx(SfxId::LaserFire, 0.6f, rng.Range(0.95f, 1.05f), pan);
            Shake(0.12f);
            break;
        case EventType::ArcZap:
            PlaySfx(SfxId::Zap, 0.45f, rng.Range(0.9f, 1.2f), pan);
            break;
        case EventType::PlayerLance:
            PlaySfx(SfxId::Lance, 0.55f, 1.0f, pan);
            grid_.ApplyDirectedForce(ev.direction * 25.0f, ev.position + ev.direction * 300.0f, 400.0f);
            Shake(0.1f);
            break;
        case EventType::MissileLaunch:
            PlaySfx(SfxId::Missile, 0.4f, rng.Range(0.9f, 1.1f), pan);
            break;
        case EventType::PulseNova:
            particles_.ShockRing(ev.position, 10.0f, ev.magnitude, 0.3f, ev.color);
            grid_.ApplyExplosiveForce(ev.magnitude * 0.3f, ev.position, ev.magnitude * 1.5f);
            PlaySfx(SfxId::Pulse, 0.6f, 1.0f, pan);
            break;
        case EventType::MineArmed:
            PlaySfx(SfxId::MineArm, 0.5f, 1.0f, pan);
            break;
        case EventType::BossSpawn:
            PlaySfx(SfxId::BossWarning, 0.8f, 1.0f, 0.0f);
            break;
        case EventType::BossPhase:
            if (ev.value >= 1) {
                particles_.ShockRing(ev.position, 30.0f, 600.0f, 0.6f, ev.color);
                grid_.ApplyExplosiveForce(120.0f, ev.position, 800.0f);
                PlaySfx(SfxId::BossRoar, 0.9f, 1.0f - 0.08f * ev.magnitude, pan);
                Ripple(ev.position, 800.0f, 1200.0f, 0.02f);
                Shake(0.5f);
                Flash(ev.color, 0.3f);
            } else if (ev.value == -2) {
                PlaySfx(SfxId::ShieldBlock, 0.9f, 0.6f, pan);
                AddText(ev.position, "SHIELD DOWN", Color{255, 120, 140, 255}, 40.0f, true);
                Shake(0.4f);
            } else if (ev.value == -1) {
                PlaySfx(SfxId::BossRoar, 0.6f, 1.3f, pan);
            }
            break;
        case EventType::SectorCleared:
            PlaySfx(SfxId::SectorClear, 0.9f, 1.0f, 0.0f);
            break;
        case EventType::ComboMilestone: {
            char buffer[24];
            std::snprintf(buffer, sizeof(buffer), "%d COMBO", static_cast<int>(ev.magnitude));
            AddText(ev.position + Vector2{0.0f, -50.0f}, buffer, palette::kUiWarn, 36.0f, true);
            PlaySfx(SfxId::Combo, 0.5f, 1.0f, 0.0f);
            break;
        }
        }
    }
}

void EffectsDirector::Update(const World& world, float realDt) {
    hitStop_ = std::max(0.0f, hitStop_ - realDt);
    slowMo_ = std::max(0.0f, slowMo_ - realDt);
    aberration_ = Damp(aberration_, 0.0f, 0.12f, realDt);
    flashAmount_ = Damp(flashAmount_, 0.0f, 0.08f, realDt);
    pickupTimer_ = std::max(0.0f, pickupTimer_ - realDt);

    for (FloatingText& t : texts_) {
        t.life -= realDt;
        t.pop = std::max(0.0f, t.pop - realDt * 6.0f);
        t.position += t.velocity * realDt;
        t.velocity *= std::exp(-3.0f * realDt);
    }
    texts_.erase(std::remove_if(texts_.begin(), texts_.end(), [](const FloatingText& t) { return t.life <= 0.0f; }),
                 texts_.end());

    const float dt = realDt * TimeScale();
    const Player& p = world.GetPlayer();
    Rng& rng = particles_.Random();

    for (Distortion& d : distortions_) {
        d.radius += d.speed * realDt;
    }
    distortions_.erase(std::remove_if(distortions_.begin(), distortions_.end(),
                                      [](const Distortion& d) { return d.radius >= d.maxRadius; }),
                       distortions_.end());

    // Scheduled cosmetic bursts run on real time so slow motion doesn't stall them.
    for (ScheduledBurst& burst : scheduled_) {
        burst.delay -= realDt;
        if (burst.delay <= 0.0f) {
            particles_.Burst(burst.position, static_cast<int>(burst.size * 0.6f), burst.color, 150.0f,
                             300.0f + burst.size * 5.0f, 0.3f, 0.8f, 18.0f, 3.0f);
            particles_.ShockRing(burst.position, burst.size * 0.2f, burst.size * 1.8f, 0.35f, burst.color);
            particles_.GlowPuff(burst.position, burst.size * 1.4f, 0.3f, burst.color);
            grid_.ApplyExplosiveForce(burst.size * 0.4f, burst.position, burst.size * 3.0f);
            PlaySfx(burst.size > 200.0f ? SfxId::BossDeath : SfxId::Explosion, burst.size > 200.0f ? 0.6f : 0.45f,
                    rng.Range(0.85f, 1.15f), Pan(burst.position));
            Shake(burst.size > 200.0f ? 0.7f : 0.2f);
        }
    }
    scheduled_.erase(std::remove_if(scheduled_.begin(), scheduled_.end(),
                                    [](const ScheduledBurst& b) { return b.delay <= 0.0f; }),
                     scheduled_.end());

    // Engine exhaust.
    exhaustTimer_ -= dt;
    if (p.alive && exhaustTimer_ <= 0.0f && LengthSq(p.velocity) > 900.0f) {
        exhaustTimer_ = 0.016f;
        const Vector2 back = Normalized(p.velocity) * -1.0f;
        const Color c = GetShipInfo(world.Config().ship).color;
        particles_.Spray(p.position + back * 14.0f, back, 0.25f, 1, c, 150.0f, 320.0f, 0.15f, 0.3f, 10.0f, 2.2f);
    }
    // Missile smoke.
    for (const Missile& m : world.Missiles()) {
        if (m.alive && rng.Chance(0.6f)) {
            particles_.Spray(m.position, m.velocity * -1.0f, 0.3f, 1, Color{255, 170, 90, 200}, 50.0f, 150.0f, 0.2f,
                             0.4f, 8.0f, 2.0f);
        }
    }
    // Gravity wells bend the grid continuously.
    for (const Enemy& e : world.Enemies()) {
        if (!e.Active()) {
            continue;
        }
        if (e.kind == EnemyKind::Singularity) {
            grid_.ApplyImplosiveForce(14.0f, e.position, 260.0f);
        } else if (e.kind == EnemyKind::BossSingularity) {
            grid_.ApplyImplosiveForce(30.0f, e.position, 520.0f);
        } else if (IsBoss(e.kind)) {
            grid_.ApplyImplosiveForce(4.0f, e.position, e.radius * 3.0f);
        }
    }
    // A sample of player bullets push the grid so firing visibly ripples it.
    const auto& bullets = world.Bullets();
    const std::size_t stride = std::max<std::size_t>(1, bullets.size() / 40);
    for (std::size_t i = static_cast<std::size_t>(bulletForceCounter_++ % static_cast<int>(stride)); i < bullets.size();
         i += stride) {
        const Bullet& b = bullets[i];
        if (b.alive) {
            grid_.ApplyDirectedForce(b.velocity * (0.0035f * static_cast<float>(stride)), b.position, 40.0f);
        }
    }
    if (p.alive && p.Dashing()) {
        grid_.ApplyDirectedForce(p.velocity * 0.01f, p.position, 120.0f);
    }
}

} // namespace gb
