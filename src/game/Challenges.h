#pragma once

#include <cstdint>
#include <span>

namespace gb {

class World;

enum class ChallengeId : std::uint8_t {
    Exterminator,
    WardenDown,
    HydraDown,
    HorizonDown,
    ArchitectDown,
    CoreDown,
    ComboArtist,
    Untouchable,
    Modular,
    MaxedOut,
    Overclocked,
    HighRoller,
    Veteran,
    Marathon,
    DailyDriver,
    FleetCommander,
    Count
};

struct ChallengeInfo {
    ChallengeId id;
    const char* name;
    const char* description;
    int reward;  // credits
};

const ChallengeInfo& GetChallengeInfo(ChallengeId id);
std::span<const ChallengeInfo> AllChallenges();

// Whether a run-based challenge is satisfied by the current state of a run.
// (FleetCommander is profile-based and always returns false here.)
bool ChallengeMet(ChallengeId id, const World& world);

} // namespace gb
