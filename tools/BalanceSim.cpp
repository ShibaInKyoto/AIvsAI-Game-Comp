// Headless balance harness: plays many bot runs and prints survival
// statistics per ship and difficulty. Used to tune pacing without a window.
//
//   gridbreaker_balance [runs] [maxSeconds] [difficulty 0-2] [bot skill 0-1]

#include "game/Bot.h"
#include "game/World.h"

#include <array>
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
    int hits = 0;
    std::array<float, 6> sectorEnd{};  // time each sector's boss died
    std::array<float, 6> bossTime{};   // seconds spent fighting each boss
};

float g_skill = 0.6f;

Result Play(ShipId ship, Difficulty difficulty, std::uint64_t seed, float maxSeconds) {
    RunConfig config;
    config.ship = ship;
    config.difficulty = difficulty;
    config.seed = seed;
    World world(config);
    Bot bot(g_skill);
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
        const int sector = std::min(world.GetDirector().Sector(), 5);
        if (world.GetDirector().Phase() == SectorPhase::Boss) {
            result.bossTime[static_cast<std::size_t>(sector)] += dt;
        }
        if (world.GetDirector().Phase() == SectorPhase::Cleared && result.sectorEnd[static_cast<std::size_t>(sector)] == 0.0f) {
            result.sectorEnd[static_cast<std::size_t>(sector)] = world.Record().time;
        }
    }
    result.time = world.Record().time;
    result.sector = world.GetDirector().Sector() + 1;
    result.level = world.Level();
    result.kills = world.Record().kills;
    result.score = world.Record().score;
    result.hits = world.Record().hitsTaken;
    return result;
}

} // namespace

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 6;
    const float maxSeconds = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1500.0f;
    const Difficulty difficulty = argc > 3 ? static_cast<Difficulty>(std::atoi(argv[3])) : Difficulty::Standard;
    g_skill = argc > 4 ? static_cast<float>(std::atof(argv[4])) : 0.6f;
    std::printf("%-9s %8s %7s %6s %7s %10s %5s %5s %5s  %s\n", "ship", "time", "sector", "level", "kills", "score",
                "win", "peak", "hits", "boss fight seconds per sector");
    for (const ShipInfo& ship : AllShips()) {
        float totalTime = 0.0f;
        int wins = 0;
        for (int i = 0; i < runs; ++i) {
            const Result r = Play(ship.id, difficulty, 1000u + static_cast<std::uint64_t>(i) * 7919u, maxSeconds);
            totalTime += r.time;
            wins += r.victory ? 1 : 0;
            std::printf("%-9s %7.1fs %7d %6d %7d %10lld %5s %5d %5d  ", ship.name, static_cast<double>(r.time), r.sector,
                        r.level, r.kills, r.score, r.victory ? "yes" : "no", r.peakEnemies, r.hits);
            for (float b : r.bossTime) {
                std::printf("%5.0f", static_cast<double>(b));
            }
            std::printf("\n");
        }
        std::printf("  -> avg %.1fs, wins %d/%d\n\n", static_cast<double>(totalTime / static_cast<float>(runs)), wins,
                    runs);
    }
    return 0;
}
