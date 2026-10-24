#include "game/Upgrades.h"

#include <algorithm>

namespace gb {

namespace {

using U = UpgradeId;
using R = Rarity;
using C = UpgradeCategory;
constexpr ModuleId kNoModule = ModuleId::Count;

// clang-format off
constexpr std::array<UpgradeInfo, static_cast<std::size_t>(UpgradeId::Count)> kUpgrades{{
    {U::Overclock, "OVERCLOCK", "+15% fire rate.", R::Common, C::Weapon, 6, kNoModule},
    {U::HighVoltage, "HIGH VOLTAGE", "+20% damage.", R::Common, C::Weapon, 6, kNoModule},
    {U::SplitShot, "SPLIT SHOT", "+1 projectile per shot.", R::Rare, C::Weapon, 4, kNoModule},
    {U::PiercingRounds, "PIERCING ROUNDS", "Shots pierce +1 enemy.", R::Common, C::Weapon, 4, kNoModule},
    {U::Ricochet, "RICOCHET", "Shots bounce to +1 nearby enemy.", R::Rare, C::Weapon, 3, kNoModule},
    {U::Velocity, "VELOCITY", "+20% shot speed and +15% range.", R::Common, C::Weapon, 3, kNoModule},
    {U::HeavyCaliber, "HEAVY CALIBER", "+30% shot size and +10% damage.", R::Common, C::Weapon, 3, kNoModule},
    {U::CriticalCode, "CRITICAL CODE", "+8% critical hit chance.", R::Common, C::Weapon, 5, kNoModule},
    {U::Exploit, "EXPLOIT", "+50% critical hit damage.", R::Rare, C::Weapon, 3, kNoModule},
    {U::SeekerRounds, "SEEKER ROUNDS", "Shots curve toward enemies.", R::Rare, C::Weapon, 3, kNoModule},
    {U::FragRounds, "FRAG ROUNDS", "Shots explode on impact.", R::Epic, C::Weapon, 3, kNoModule},
    {U::CryoRounds, "CRYO ROUNDS", "15% chance per hit to freeze-slow enemies.", R::Rare, C::Weapon, 3, kNoModule},
    {U::RearGuns, "REAR GUNS", "+1 shot fired behind you.", R::Rare, C::Weapon, 2, kNoModule},
    {U::SideCannons, "SIDE CANNONS", "+2 shots fired to your sides.", R::Rare, C::Weapon, 2, kNoModule},

    {U::HullPlating, "HULL PLATING", "+1 max integrity and repair 1.", R::Common, C::Defense, 5, kNoModule},
    {U::NanoRepair, "NANO REPAIR", "Repair 2 integrity.", R::Common, C::Defense, 0, kNoModule},
    {U::Deflector, "DEFLECTOR", "A shield absorbs one hit, then recharges.", R::Rare, C::Defense, 3, kNoModule},
    {U::AutoRepair, "AUTO-REPAIR", "Slowly regenerate integrity.", R::Epic, C::Defense, 3, kNoModule},
    {U::ReactiveArmor, "REACTIVE ARMOR", "Taking damage unleashes a shockwave.", R::Rare, C::Defense, 3, kNoModule},

    {U::Thrusters, "THRUSTERS", "+10% movement speed.", R::Common, C::Mobility, 4, kNoModule},
    {U::Reflex, "REFLEX", "-18% dash cooldown.", R::Common, C::Mobility, 3, kNoModule},
    {U::ExtraDash, "EXTRA DASH", "+1 dash charge.", R::Rare, C::Mobility, 2, kNoModule},
    {U::PhaseBlade, "PHASE BLADE", "Dashing through enemies shreds them.", R::Rare, C::Mobility, 3, kNoModule},

    {U::Magnet, "MAGNET", "+40% pickup radius.", R::Common, C::Utility, 3, kNoModule},
    {U::DataMiner, "DATA MINER", "+15% experience gained.", R::Common, C::Utility, 3, kNoModule},
    {U::Capacitor, "CAPACITOR", "+30% Nova charge rate.", R::Common, C::Utility, 3, kNoModule},
    {U::LuckyBreak, "LUCKY BREAK", "Rare upgrades appear more often.", R::Rare, C::Utility, 2, kNoModule},
    {U::BackupDrive, "BACKUP DRIVE", "+2 rerolls.", R::Common, C::Utility, 2, kNoModule},

    {U::OrbitBlades, "ORBIT BLADES", "Blades orbit you, slicing anything they touch.", R::Rare, C::Module, 5,
     ModuleId::OrbitBlades},
    {U::Drones, "WINGMEN", "Autonomous drones fly with you and shoot.", R::Rare, C::Module, 5, ModuleId::Drones},
    {U::ArcCoil, "ARC COIL", "Periodically zaps enemies with chain lightning.", R::Rare, C::Module, 5,
     ModuleId::ArcCoil},
    {U::SwarmMissiles, "SWARM MISSILES", "Launches homing missiles that explode.", R::Rare, C::Module, 5,
     ModuleId::Missiles},
    {U::PulseNova, "PULSE NOVA", "Emits a damaging shockwave around you.", R::Rare, C::Module, 5,
     ModuleId::PulseNova},
    {U::ProxMines, "PROX MINES", "Drops mines behind you as you move.", R::Rare, C::Module, 5, ModuleId::Mines},
    {U::PrismLance, "PRISM LANCE", "Fires a piercing laser in your aim direction.", R::Rare, C::Module, 5,
     ModuleId::PrismLance},

    {U::GlassCannon, "GLASS CANNON", "+60% damage, but -2 max integrity.", R::Legendary, C::Weapon, 1, kNoModule},
    {U::BulletStorm, "BULLET STORM", "+2 projectiles and +25% fire rate.", R::Legendary, C::Weapon, 1, kNoModule},
    {U::OverdriveCore, "OVERDRIVE CORE", "Nova charges twice as fast and hits harder.", R::Legendary, C::Utility, 1,
     kNoModule},
    {U::Overcharge, "OVERCHARGE", "+6% damage and +4% fire rate. Stacks forever.", R::Common, C::Weapon, 0,
     kNoModule},

    {U::SawStorm, "SAW STORM", "Twelve blades in a pulsing storm. +80% blade damage.", R::Legendary, C::Evolution, 1,
     ModuleId::OrbitBlades},
    {U::HiveSwarm, "HIVE SWARM", "Six drones, each firing a three-way spread.", R::Legendary, C::Evolution, 1,
     ModuleId::Drones},
    {U::StormCore, "STORM CORE", "Rapid chain lightning that jumps 12 times and can crit.", R::Legendary,
     C::Evolution, 1, ModuleId::ArcCoil},
    {U::ClusterBarrage, "CLUSTER BARRAGE", "Ten missiles whose blasts split into clusters.", R::Legendary,
     C::Evolution, 1, ModuleId::Missiles},
    {U::ShockReactor, "SHOCK REACTOR", "Pulses every 1.5s with 60% more force.", R::Legendary, C::Evolution, 1,
     ModuleId::PulseNova},
    {U::GravityMines, "GRAVITY MINES", "Mines drag enemies in before a huge blast.", R::Legendary, C::Evolution, 1,
     ModuleId::Mines},
    {U::SpectrumCannon, "SPECTRUM CANNON", "A five-beam prism that fires twice as often.", R::Legendary,
     C::Evolution, 1, ModuleId::PrismLance},
}};
// clang-format on

// clang-format off
constexpr std::array<EvolutionRecipe, 7> kEvolutions{{
    {U::SawStorm, U::OrbitBlades, U::Thrusters},
    {U::HiveSwarm, U::Drones, U::SplitShot},
    {U::StormCore, U::ArcCoil, U::CriticalCode},
    {U::ClusterBarrage, U::SwarmMissiles, U::FragRounds},
    {U::ShockReactor, U::PulseNova, U::ReactiveArmor},
    {U::GravityMines, U::ProxMines, U::Magnet},
    {U::SpectrumCannon, U::PrismLance, U::Velocity},
}};
// clang-format on

constexpr std::array<float, static_cast<std::size_t>(Rarity::Count)> kRarityWeights{60.0f, 28.0f, 10.0f, 2.5f};

} // namespace

const UpgradeInfo& GetUpgradeInfo(UpgradeId id) { return kUpgrades[static_cast<std::size_t>(id)]; }

std::span<const EvolutionRecipe> AllEvolutions() { return kEvolutions; }

const EvolutionRecipe* EvolutionForModule(UpgradeId module) {
    for (const EvolutionRecipe& recipe : kEvolutions) {
        if (recipe.module == module) {
            return &recipe;
        }
    }
    return nullptr;
}

std::span<const UpgradeInfo> AllUpgrades() { return kUpgrades; }

Color RarityColor(Rarity rarity) {
    switch (rarity) {
    case Rarity::Common: return {120, 210, 255, 255};
    case Rarity::Rare: return {90, 255, 150, 255};
    case Rarity::Epic: return {200, 110, 255, 255};
    case Rarity::Legendary: return {255, 190, 50, 255};
    case Rarity::Count: break;
    }
    return WHITE;
}

const char* RarityName(Rarity rarity) {
    switch (rarity) {
    case Rarity::Common: return "COMMON";
    case Rarity::Rare: return "RARE";
    case Rarity::Epic: return "EPIC";
    case Rarity::Legendary: return "LEGENDARY";
    case Rarity::Count: break;
    }
    return "";
}

const char* CategoryName(UpgradeCategory category) {
    switch (category) {
    case UpgradeCategory::Weapon: return "WEAPON";
    case UpgradeCategory::Defense: return "DEFENSE";
    case UpgradeCategory::Mobility: return "MOBILITY";
    case UpgradeCategory::Utility: return "UTILITY";
    case UpgradeCategory::Module: return "MODULE";
    case UpgradeCategory::Evolution: return "EVOLUTION";
    }
    return "";
}

const char* UpgradeLevelText(UpgradeId id, int ownedStacks) {
    const int next = ownedStacks + 1;
    switch (id) {
    case U::OrbitBlades:
        switch (next) {
        case 1: return "2 blades orbit you, slicing anything they touch.";
        case 2: return "+1 blade.";
        case 3: return "+35% blade damage and a wider orbit.";
        case 4: return "+1 blade.";
        default: return "+2 blades and faster spin.";
        }
    case U::Drones:
        switch (next) {
        case 1: return "A wingman drone flies with you and shoots.";
        case 2: return "+1 drone.";
        case 3: return "Drones fire 40% faster.";
        case 4: return "+1 drone.";
        default: return "Drones fire piercing twin shots.";
        }
    case U::ArcCoil:
        switch (next) {
        case 1: return "Every 2s, chain lightning jumps between 3 enemies.";
        case 2: return "+2 chain jumps.";
        case 3: return "+40% damage, fires more often.";
        case 4: return "+2 chain jumps.";
        default: return "Fires two arcs at once.";
        }
    case U::SwarmMissiles:
        switch (next) {
        case 1: return "Every 3s, launch 2 homing missiles.";
        case 2: return "+1 missile.";
        case 3: return "+40% blast radius.";
        case 4: return "+2 missiles.";
        default: return "+50% damage, launch faster.";
        }
    case U::PulseNova:
        switch (next) {
        case 1: return "Every 4s, a shockwave knocks back nearby enemies.";
        case 2: return "+30% radius.";
        case 3: return "+50% damage, pulses faster.";
        case 4: return "+30% radius.";
        default: return "Pulses also destroy enemy bullets.";
        }
    case U::ProxMines:
        switch (next) {
        case 1: return "Drop explosive mines behind you while moving.";
        case 2: return "Mines drop 30% more often.";
        case 3: return "+40% blast radius.";
        case 4: return "+50% damage.";
        default: return "Mines split into a cluster blast.";
        }
    case U::PrismLance:
        switch (next) {
        case 1: return "Every 2.5s, fire a piercing laser where you aim.";
        case 2: return "+40% damage.";
        case 3: return "Wider beam, fires more often.";
        case 4: return "+40% damage.";
        default: return "Fires a three-way prism spread.";
        }
    case U::Deflector:
        return ownedStacks == 0 ? "A shield absorbs one hit, recharging every 14s." : "Shield recharges 3s faster.";
    case U::AutoRepair: return ownedStacks == 0 ? "Regenerate 1 integrity every 50s." : "Regenerate faster.";
    case U::ReactiveArmor:
        return ownedStacks == 0 ? "Taking damage unleashes a damaging shockwave." : "+60% shockwave damage and size.";
    case U::PhaseBlade: return ownedStacks == 0 ? "Dashing through enemies deals heavy damage." : "+60% dash damage.";
    case U::SeekerRounds:
        return ownedStacks == 0 ? "Shots curve toward nearby enemies." : "Shots turn harder toward enemies.";
    case U::FragRounds: return ownedStacks == 0 ? "Shots explode on impact for 40% damage." : "+35% explosion radius.";
    case U::CryoRounds: return ownedStacks == 0 ? "15% chance per hit to slow enemies." : "+10% slow chance.";
    default: return GetUpgradeInfo(id).description;
    }
}

void ApplyUpgradeStack(UpgradeId id, int stack, PlayerStats& s) {
    switch (id) {
    case U::Overclock: s.fireRate *= 1.15f; break;
    case U::HighVoltage: s.damage *= 1.20f; break;
    case U::SplitShot:
        s.projectiles += 1;
        s.damage *= 0.92f;
        break;
    case U::PiercingRounds: s.pierce += 1; break;
    case U::Ricochet: s.bounce += 1; break;
    case U::Velocity:
        s.bulletSpeed *= 1.20f;
        s.bulletLifetime *= 1.15f;
        break;
    case U::HeavyCaliber:
        s.bulletSize *= 1.30f;
        s.damage *= 1.10f;
        break;
    case U::CriticalCode: s.critChance += 0.08f; break;
    case U::Exploit: s.critMultiplier += 0.5f; break;
    case U::SeekerRounds: s.homing += stack == 1 ? 3.0f : 2.0f; break;
    case U::FragRounds: s.explosionRadius = stack == 1 ? 70.0f : s.explosionRadius * 1.35f; break;
    case U::CryoRounds: s.slowChance += stack == 1 ? 0.15f : 0.10f; break;
    case U::RearGuns: s.rearGuns += 1; break;
    case U::SideCannons: s.sideGuns += 1; break;

    case U::HullPlating: s.maxHp += 1; break;
    case U::NanoRepair: break; // instant effect handled by the world
    case U::Deflector:
        s.shieldCharges = 1;
        s.shieldRecharge = stack == 1 ? 14.0f : s.shieldRecharge - 3.0f;
        break;
    case U::AutoRepair: s.regenInterval = stack == 1 ? 50.0f : s.regenInterval * 0.7f; break;
    case U::ReactiveArmor: s.thornsDamage = stack == 1 ? 60.0f : s.thornsDamage * 1.6f; break;

    case U::Thrusters: s.moveSpeed *= 1.10f; break;
    case U::Reflex: s.dashCooldown *= 0.82f; break;
    case U::ExtraDash: s.dashCharges += 1; break;
    case U::PhaseBlade: s.dashDamage = stack == 1 ? 80.0f : s.dashDamage * 1.6f; break;

    case U::Magnet: s.magnetRadius *= 1.40f; break;
    case U::DataMiner: s.xpMultiplier += 0.15f; break;
    case U::Capacitor: s.novaChargeRate *= 1.30f; break;
    case U::LuckyBreak: s.luck += 1.0f; break;
    case U::BackupDrive: s.rerolls += 2; break;

    case U::OrbitBlades:
    case U::Drones:
    case U::ArcCoil:
    case U::SwarmMissiles:
    case U::PulseNova:
    case U::ProxMines:
    case U::PrismLance: s.SetModule(GetUpgradeInfo(id).module, stack); break;

    case U::GlassCannon:
        s.damage *= 1.6f;
        s.powerMultiplier *= 1.3f;
        s.maxHp = std::max(1, s.maxHp - 2);
        break;
    case U::BulletStorm:
        s.projectiles += 2;
        s.fireRate *= 1.25f;
        break;
    case U::OverdriveCore: s.novaChargeRate *= 2.0f; break;
    case U::Overcharge: {
        // Additive across stacks (the factors telescope to 1 + 0.06n and
        // 1 + 0.04n): compounding made endless builds grow without bound.
        const float k = static_cast<float>(stack - 1);
        s.damage *= 1.0f + 0.06f / (1.0f + 0.06f * k);
        s.fireRate *= 1.0f + 0.04f / (1.0f + 0.04f * k);
        break;
    }
    case U::SawStorm:
    case U::HiveSwarm:
    case U::StormCore:
    case U::ClusterBarrage:
    case U::ShockReactor:
    case U::GravityMines:
    case U::SpectrumCannon: s.evolved[static_cast<std::size_t>(GetUpgradeInfo(id).module)] = true; break;
    case U::Count: break;
    }
}

int UpgradeInventory::ModuleCount() const {
    int count = 0;
    for (const UpgradeInfo& info : kUpgrades) {
        if (info.category == UpgradeCategory::Module && Stacks(info.id) > 0) {
            ++count;
        }
    }
    return count;
}

bool UpgradeInventory::CanOffer(UpgradeId id, const PlayerStats& stats, int hp) const {
    const UpgradeInfo& info = GetUpgradeInfo(id);
    const int owned = Stacks(id);
    if (info.maxStacks > 0 && owned >= info.maxStacks) {
        return false;
    }
    if (info.category == UpgradeCategory::Module && owned == 0 && ModuleCount() >= kMaxModules) {
        return false;
    }
    if (id == U::NanoRepair && hp >= stats.maxHp) {
        return false;
    }
    if (info.category == UpgradeCategory::Evolution) {
        for (const EvolutionRecipe& recipe : kEvolutions) {
            if (recipe.evolution == id) {
                return Stacks(recipe.module) >= GetUpgradeInfo(recipe.module).maxStacks && Stacks(recipe.catalyst) > 0;
            }
        }
        return false;
    }
    if (id == U::GlassCannon && stats.maxHp <= 3) {
        return false;
    }
    return true;
}

std::vector<UpgradeId> UpgradeInventory::RollOffers(Rng& rng, int count, const PlayerStats& stats, int hp,
                                                    std::span<const UpgradeId> banned) const {
    // Luck moves weight from common cards to the rarer tiers.
    std::array<float, static_cast<std::size_t>(Rarity::Count)> rarityWeights = kRarityWeights;
    rarityWeights[0] = std::max(20.0f, rarityWeights[0] - stats.luck * 15.0f);
    rarityWeights[1] += stats.luck * 6.0f;
    rarityWeights[2] += stats.luck * 5.0f;
    rarityWeights[3] += stats.luck * 2.0f;

    std::vector<UpgradeId> candidates;
    std::vector<float> weights;
    for (const UpgradeInfo& info : kUpgrades) {
        if (std::find(banned.begin(), banned.end(), info.id) != banned.end() || !CanOffer(info.id, stats, hp)) {
            continue;
        }
        float w = rarityWeights[static_cast<std::size_t>(info.rarity)];
        // Already-owned modules are favoured so builds can actually come together.
        if (info.category == UpgradeCategory::Module && Stacks(info.id) > 0) {
            w *= 2.2f;
        }
        // An unlocked evolution should show up almost immediately.
        if (info.category == UpgradeCategory::Evolution) {
            w = 400.0f;
        }
        // Overcharge is a filler: rare while the pool is rich, common once it thins out.
        if (info.id == U::Overcharge) {
            w *= 0.12f;
        }
        // Repairs become likelier the more damaged the player is.
        if (info.id == U::NanoRepair) {
            w *= 0.5f + 2.0f * (1.0f - static_cast<float>(hp) / static_cast<float>(std::max(stats.maxHp, 1)));
        }
        candidates.push_back(info.id);
        weights.push_back(w);
    }

    std::vector<UpgradeId> offers;
    while (static_cast<int>(offers.size()) < count && !candidates.empty()) {
        const int pick = rng.WeightedIndex(weights);
        if (pick < 0) {
            break;
        }
        offers.push_back(candidates[static_cast<std::size_t>(pick)]);
        candidates.erase(candidates.begin() + pick);
        weights.erase(weights.begin() + pick);
    }
    return offers;
}

} // namespace gb
