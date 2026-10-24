#pragma once

#include "core/Random.h"
#include "game/Stats.h"
#include "game/Types.h"

#include <raylib.h>

#include <array>
#include <span>
#include <vector>

namespace gb {

enum class UpgradeId : std::uint8_t {
    // Main gun
    Overclock,
    HighVoltage,
    SplitShot,
    PiercingRounds,
    Ricochet,
    Velocity,
    HeavyCaliber,
    CriticalCode,
    Exploit,
    SeekerRounds,
    FragRounds,
    CryoRounds,
    RearGuns,
    SideCannons,
    // Defence
    HullPlating,
    NanoRepair,
    Deflector,
    AutoRepair,
    ReactiveArmor,
    // Mobility
    Thrusters,
    Reflex,
    ExtraDash,
    PhaseBlade,
    // Utility
    Magnet,
    DataMiner,
    Capacitor,
    LuckyBreak,
    BackupDrive,
    // Modules
    OrbitBlades,
    Drones,
    ArcCoil,
    SwarmMissiles,
    PulseNova,
    ProxMines,
    PrismLance,
    // Legendary
    GlassCannon,
    BulletStorm,
    OverdriveCore,
    // Filler that never runs out (keeps late/endless drafts full)
    Overcharge,
    // Evolutions: a level 5 module plus its catalyst upgrade
    SawStorm,
    HiveSwarm,
    StormCore,
    ClusterBarrage,
    ShockReactor,
    GravityMines,
    SpectrumCannon,
    Count
};

enum class UpgradeCategory : std::uint8_t { Weapon, Defense, Mobility, Utility, Module, Evolution };

// Recipe for an evolution: which module (at max level) and catalyst unlock it.
struct EvolutionRecipe {
    UpgradeId evolution;
    UpgradeId module;
    UpgradeId catalyst;
};

std::span<const EvolutionRecipe> AllEvolutions();
// The evolution a module card leads to (nullptr for non-modules).
const EvolutionRecipe* EvolutionForModule(UpgradeId module);

struct UpgradeInfo {
    UpgradeId id;
    const char* name;
    const char* description;  // shown on the card
    Rarity rarity;
    UpgradeCategory category;
    int maxStacks;            // 0 = unlimited (consumables)
    ModuleId module;          // only meaningful for UpgradeCategory::Module
};

const UpgradeInfo& GetUpgradeInfo(UpgradeId id);
std::span<const UpgradeInfo> AllUpgrades();
Color RarityColor(Rarity rarity);
const char* RarityName(Rarity rarity);
const char* CategoryName(UpgradeCategory category);

// Text describing what the *next* stack of an upgrade does, given how many
// stacks are already owned (module cards show per-level changes).
const char* UpgradeLevelText(UpgradeId id, int ownedStacks);

// Applies one stack of an upgrade to the stats (stack is 1-based).
void ApplyUpgradeStack(UpgradeId id, int stack, PlayerStats& stats);

// Tracks owned upgrades and produces level-up offers.
class UpgradeInventory {
public:
    static constexpr int kMaxModules = 4;

    int Stacks(UpgradeId id) const { return stacks_[static_cast<std::size_t>(id)]; }
    void Add(UpgradeId id) { stacks_[static_cast<std::size_t>(id)]++; order_.push_back(id); }
    const std::vector<UpgradeId>& Order() const { return order_; }
    int ModuleCount() const;
    bool CanOffer(UpgradeId id, const PlayerStats& stats, int hp) const;

    // Rolls `count` distinct offers weighted by rarity (luck shifts weights
    // toward rarer cards). `banned` entries are skipped.
    std::vector<UpgradeId> RollOffers(Rng& rng, int count, const PlayerStats& stats, int hp,
                                      std::span<const UpgradeId> banned = {}) const;

private:
    std::array<int, static_cast<std::size_t>(UpgradeId::Count)> stacks_{};
    std::vector<UpgradeId> order_;
};

} // namespace gb
