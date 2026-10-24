#include "game/EnemyCatalog.h"

#include <array>

namespace gb {

namespace {

// clang-format off
constexpr std::array<EnemyInfo, static_cast<std::size_t>(EnemyKind::Count)> kEnemies{{
    //  name             hp     speed  radius  color                    xp   score  threat mass
    {"Drifter",          20.0f, 150.0f, 18.0f, {70, 225, 255, 255},     1,   50,    1.0f,  1.0f},
    {"Dart",             20.0f, 900.0f, 16.0f, {120, 255, 110, 255},    1,   75,    1.4f,  0.8f},
    {"Pinwheel",         30.0f, 210.0f, 20.0f, {170, 110, 255, 255},    1,   60,    1.2f,  1.0f},
    {"Splitter",         50.0f, 140.0f, 24.0f, {255, 90, 200, 255},     2,  100,    2.2f,  1.4f},
    {"Mini",             14.0f, 260.0f, 12.0f, {255, 130, 220, 255},    1,   25,    0.5f,  0.6f},
    {"Gunner",           40.0f, 170.0f, 20.0f, {255, 150, 50, 255},     2,  120,    2.4f,  1.2f},
    {"Swarmer",           8.0f, 330.0f, 10.0f, {255, 80, 120, 255},     1,   20,    0.35f, 0.4f},
    {"Hive",            160.0f,  45.0f, 34.0f, {255, 200, 60, 255},     8,  400,    6.0f,  6.0f},
    {"Singularity",     260.0f,  30.0f, 30.0f, {255, 60, 70, 255},     10,  500,    7.0f, 50.0f},
    {"Weaver",          120.0f, 260.0f, 20.0f, {255, 235, 80, 255},     6,  350,    5.0f,  3.0f},
    {"Weaver Body",       1.0f, 260.0f, 15.0f, {255, 235, 80, 255},     0,    0,    0.0f, 50.0f},
    {"Shielder",         90.0f, 120.0f, 24.0f, {90, 140, 255, 255},     4,  250,    3.5f,  3.0f},
    {"Lancer",           60.0f, 160.0f, 20.0f, {255, 70, 220, 255},     3,  220,    3.2f,  1.5f},
    {"Bomber",           70.0f, 230.0f, 22.0f, {60, 255, 200, 255},     3,  200,    3.0f,  2.0f},
    {"Mine",             10.0f,   0.0f, 14.0f, {60, 255, 200, 255},     0,   10,    0.0f, 50.0f},
    // Bosses: hp is scaled by the director per sector/difficulty.
    {"WARDEN",        10500.0f,  90.0f, 70.0f, {80, 220, 255, 255},   120, 10000,   0.0f, 999.0f},
    {"HYDRA",         17000.0f, 300.0f, 34.0f, {255, 220, 70, 255},   150, 15000,   0.0f, 999.0f},
    {"EVENT HORIZON", 22000.0f,  60.0f, 64.0f, {255, 60, 80, 255},    180, 20000,   0.0f, 999.0f},
    {"ARCHITECT",     27000.0f, 160.0f, 56.0f, {200, 120, 255, 255},  210, 25000,   0.0f, 999.0f},
    {"THE CORE",      36000.0f,  60.0f, 80.0f, {255, 255, 255, 255},  300, 50000,   0.0f, 999.0f},
    {"Turret",          160.0f,   0.0f, 22.0f, {255, 120, 120, 255},    3,  150,    0.0f, 999.0f},
}};
// clang-format on

} // namespace

const EnemyInfo& GetEnemyInfo(EnemyKind kind) { return kEnemies[static_cast<std::size_t>(kind)]; }

} // namespace gb
