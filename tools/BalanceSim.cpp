// Headless balance harness: plays many bot runs and prints survival
// statistics per ship and difficulty. Used to tune pacing without a window.
//
//   gridbreaker_balance [runs] [maxSeconds]

#include "game/Bot.h"
#include "game/World.h"

#include <cstdio>
#include <cstdlib>

using namespace gb;

namespace {

struct Result {
    float time = 0.0f;
    int sector = 0;
    int level = 0;
    int kills = 0;
    long long score = 0;
    bool victory = false;
    int peakEnemies = 0;
};

Result Play(ShipId ship, Difficulty difficulty, std::uint64_t seed, float maxSeconds) {
    RunConfig config;
    config.ship = ship;
    config.difficulty = difficulty;
    config.seed = seed;
    World world(config);
    Bot bot;
    Result result;
    const float dt = 1.0f / 60.0f;
    for (float t = 0.0f; t < maxSeconds && world.PlayerAlive(); t += dt) {
        while (world.PendingLevelUps() > 0 || world.PendingCaches() > 0) {
            const auto offers = world.RollOffers(3);
            if (!offers.empty()) {
                world.ApplyUpgrade(Bot::ChooseUpgrade(world, offers));
            }
            if (world.PendingLevelUps() > 0) {
                world.ConsumeLevelUp();
            } else {
                world.ConsumeCache();
            }
        }
        const SectorPhase phase = world.GetDirector().Phase();
        if (phase == SectorPhase::Victory) {
            result.victory = true;
            break;
        }
        if (phase == SectorPhase::Cleared && world.GetDirector().PhaseTime() > 2.6f) {
            world.ChooseSectorModifier(SectorModifier::Respite);
        }
        world.Update(dt, bot.Think(world, dt));
        world.Events().clear();
        result.peakEnemies = std::max(result.peakEnemies, world.ActiveEnemyCount());
    }
    result.time = world.Record().time;
    result.sector = world.GetDirector().Sector() + 1;
    result.level = world.Level();
    result.kills = world.Record().kills;
    result.score = world.Record().score;
    return result;
}

} // namespace

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 6;
    const float maxSeconds = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1500.0f;
    const Difficulty difficulty = argc > 3 ? static_cast<Difficulty>(std::atoi(argv[3])) : Difficulty::Standard;
    std::printf("%-9s %8s %7s %6s %7s %10s %5s %5s\n", "ship", "time", "sector", "level", "kills", "score", "win",
                "peak");
    for (const ShipInfo& ship : AllShips()) {
        float totalTime = 0.0f;
        int wins = 0;
        for (int i = 0; i < runs; ++i) {
            const Result r = Play(ship.id, difficulty, 1000u + static_cast<std::uint64_t>(i) * 7919u, maxSeconds);
            totalTime += r.time;
            wins += r.victory ? 1 : 0;
            std::printf("%-9s %7.1fs %7d %6d %7d %10lld %5s %5d\n", ship.name, static_cast<double>(r.time), r.sector,
                        r.level, r.kills, r.score, r.victory ? "yes" : "no", r.peakEnemies);
        }
        std::printf("  -> avg %.1fs, wins %d/%d\n\n", static_cast<double>(totalTime / static_cast<float>(runs)), wins,
                    runs);
    }
    return 0;
}
