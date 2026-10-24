#include "game/Ships.h"

#include <array>

namespace gb {

namespace {

// clang-format off
constexpr std::array<ShipInfo, static_cast<std::size_t>(ShipId::Count)> kShips{{
    {ShipId::Vector, "VECTOR", "Balanced interceptor",
     "Reliable twin blaster and solid handling. A clean slate for any build.", {120, 230, 255, 255}, 0,
     UpgradeId::Count, 0},
    {ShipId::Lancer, "LANCER", "Piercing railgun",
     "Slow, devastating rail shots that punch through lines of enemies.", {255, 90, 220, 255}, 350,
     UpgradeId::Count, 0},
    {ShipId::Bruiser, "BRUISER", "Close-range scattergun",
     "A wide shotgun blast and a thick hull. Get in close and stay there.", {255, 160, 60, 255}, 450,
     UpgradeId::Count, 0},
    {ShipId::Bastion, "BASTION", "Orbital defender",
     "Starts with Orbit Blades. Weaker gun, sturdier frame.", {110, 255, 140, 255}, 550,
     UpgradeId::OrbitBlades, 2},
    {ShipId::Specter, "SPECTER", "Phase assassin",
     "Two dash charges with Phase Blade. Rapid fire, fragile hull.", {180, 120, 255, 255}, 700,
     UpgradeId::PhaseBlade, 1},
    {ShipId::Hivemind, "HIVEMIND", "Drone carrier",
     "Starts with two Wingmen drones. Let the swarm do the work.", {255, 225, 80, 255}, 850, UpgradeId::Drones, 2},
}};
// clang-format on

} // namespace

const ShipInfo& GetShipInfo(ShipId id) { return kShips[static_cast<std::size_t>(id)]; }

std::span<const ShipInfo> AllShips() { return kShips; }

PlayerStats ShipBaseStats(ShipId id) {
    PlayerStats s;
    switch (id) {
    case ShipId::Vector:
        s.projectiles = 2;
        s.spread = 0.07f;
        s.damage = 9.0f;
        break;
    case ShipId::Lancer:
        s.maxHp = 5;
        s.fireRate = 3.3f;
        s.damage = 38.0f;
        s.pierce = 3;
        s.bulletSpeed = 2600.0f;
        s.bulletLifetime = 0.5f;
        s.bulletSize = 1.25f;
        s.inaccuracy = 0.0f;
        break;
    case ShipId::Bruiser:
        s.maxHp = 7;
        s.moveSpeed = 400.0f;
        s.projectiles = 6;
        s.spread = 0.13f;
        s.inaccuracy = 0.06f;
        s.damage = 8.5f;
        s.fireRate = 3.2f;
        s.bulletSpeed = 1250.0f;
        s.bulletLifetime = 0.36f;
        break;
    case ShipId::Bastion:
        s.maxHp = 6;
        s.damage = 9.0f;
        s.fireRate = 7.0f;
        break;
    case ShipId::Specter:
        s.maxHp = 4;
        s.moveSpeed = 480.0f;
        s.fireRate = 10.0f;
        s.damage = 9.0f;
        s.dashCharges = 2;
        s.dashCooldown = 0.75f;
        break;
    case ShipId::Hivemind:
        s.damage = 9.0f;
        s.fireRate = 6.5f;
        break;
    case ShipId::Count: break;
    }
    return s;
}

} // namespace gb
