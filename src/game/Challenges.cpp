#include "game/Challenges.h"

#include "game/World.h"

#include <array>

namespace gb {

namespace {

using C = ChallengeId;

constexpr std::array<ChallengeInfo, static_cast<std::size_t>(ChallengeId::Count)> kChallenges{{
    {C::Exterminator, "EXTERMINATOR", "Destroy 1,000 enemies in a single run.", 60},
    {C::WardenDown, "GATECRASHER", "Defeat the Warden.", 60},
    {C::HydraDown, "SERPENT TAMER", "Defeat the Hydra.", 90},
    {C::HorizonDown, "ESCAPE VELOCITY", "Defeat the Event Horizon.", 120},
    {C::ArchitectDown, "FIREWALL BREACH", "Defeat the Architect.", 150},
    {C::CoreDown, "GRIDBREAKER", "Destroy the Core and liberate the system.", 250},
    {C::ComboArtist, "COMBO ARTIST", "Reach a 250 kill combo.", 100},
    {C::Untouchable, "UNTOUCHABLE", "Clear a sector without taking damage.", 150},
    {C::Modular, "MODULAR", "Run four weapon modules at once.", 80},
    {C::MaxedOut, "MAXED OUT", "Raise any weapon module to level 5.", 80},
    {C::Overclocked, "OVERCLOCKED", "Clear three sectors on Overclocked difficulty.", 200},
    {C::HighRoller, "HIGH ROLLER", "Score 1,000,000 points in a single run.", 150},
    {C::Veteran, "VETERAN", "Reach level 30 in a single run.", 100},
    {C::Marathon, "MARATHON", "Reach the second loop in Endless mode.", 200},
    {C::DailyDriver, "DAILY DRIVER", "Clear two sectors in a Daily run.", 100},
    {C::FleetCommander, "FLEET COMMANDER", "Unlock every ship in the hangar.", 150},
}};

bool BossDown(const World& world, EnemyKind kind) {
    return (world.Record().bossMask & (1u << static_cast<unsigned>(kind))) != 0;
}

} // namespace

const ChallengeInfo& GetChallengeInfo(ChallengeId id) { return kChallenges[static_cast<std::size_t>(id)]; }

std::span<const ChallengeInfo> AllChallenges() { return kChallenges; }

bool ChallengeMet(ChallengeId id, const World& world) {
    const RunStats& s = world.Record();
    switch (id) {
    case C::Exterminator: return s.kills >= 1000;
    case C::WardenDown: return BossDown(world, EnemyKind::BossWarden);
    case C::HydraDown: return BossDown(world, EnemyKind::BossHydra);
    case C::HorizonDown: return BossDown(world, EnemyKind::BossSingularity);
    case C::ArchitectDown: return BossDown(world, EnemyKind::BossArchitect);
    case C::CoreDown: return BossDown(world, EnemyKind::BossCore);
    case C::ComboArtist: return s.bestCombo >= 250;
    case C::Untouchable: return s.flawlessSectors > 0;
    case C::Modular: return world.Inventory().ModuleCount() >= UpgradeInventory::kMaxModules;
    case C::MaxedOut:
        for (int m = 0; m < static_cast<int>(ModuleId::Count); ++m) {
            if (world.Stats().Module(static_cast<ModuleId>(m)) >= 5) {
                return true;
            }
        }
        return false;
    case C::Overclocked: return world.Config().difficulty == Difficulty::Overclocked && s.sectorsCleared >= 3;
    case C::HighRoller: return s.score >= 1'000'000;
    case C::Veteran: return world.Level() >= 30;
    case C::Marathon: return world.GetDirector().Loop() >= 1;
    case C::DailyDriver: return world.Config().mode == GameMode::Daily && s.sectorsCleared >= 2;
    case C::FleetCommander:
    case C::Count: break;
    }
    return false;
}

} // namespace gb
