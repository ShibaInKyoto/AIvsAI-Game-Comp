#pragma once

#include "game/Stats.h"
#include "game/Upgrades.h"

#include <raylib.h>

#include <span>

namespace gb {

enum class ShipId : std::uint8_t {
    Vector,
    Lancer,
    Bruiser,
    Warden,
    Specter,
    Hivemind,
    Count
};

struct ShipInfo {
    ShipId id;
    const char* name;
    const char* role;        // one-line summary
    const char* description; // hangar blurb
    Color color;
    int cost; // credits to unlock (0 = available from the start)
    UpgradeId startingUpgrade;
    int startingUpgradeLevel; // 0 = none
};

const ShipInfo& GetShipInfo(ShipId id);
std::span<const ShipInfo> AllShips();

// Base stats before any upgrades.
PlayerStats ShipBaseStats(ShipId id);

} // namespace gb
