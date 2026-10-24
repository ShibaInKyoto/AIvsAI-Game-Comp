#include "core/Storage.h"
#include "game/Challenges.h"
#include "game/Director.h"
#include "game/Profile.h"
#include "game/SpatialHash.h"
#include "game/World.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <vector>

using namespace gb;

namespace {

// Points the storage layer at a throwaway directory for the test's lifetime.
struct TempDataDir {
    std::filesystem::path path;
    TempDataDir() {
        path = std::filesystem::temp_directory_path() / "gridbreaker_test_profile";
        std::filesystem::remove_all(path);
        storage::SetDataDirectory(path);
    }
    ~TempDataDir() { std::filesystem::remove_all(path); }
};

} // namespace

TEST_CASE("Profile persists credits, unlocks, firmware and challenges") {
    TempDataDir dir;
    {
        Profile profile;
        RunSummary run;
        run.stats.score = 300000;
        run.stats.kills = 900;
        run.stats.sectorsCleared = 2;
        run.stats.bossesKilled = 2;
        run.config.difficulty = Difficulty::Standard;
        const int earned = profile.RecordRun(run);
        CHECK(earned > 0);
        CHECK(profile.Credits() == earned);
        CHECK(profile.BestScore(Difficulty::Standard) == 300000);
        CHECK(profile.TryUnlockShip(ShipId::Lancer) == (earned >= GetShipInfo(ShipId::Lancer).cost));
        CHECK(profile.CompleteChallenge(ChallengeId::WardenDown));
        CHECK_FALSE(profile.CompleteChallenge(ChallengeId::WardenDown)); // only once
        CHECK(profile.Save());
    }
    Profile loaded;
    REQUIRE(loaded.Load());
    CHECK(loaded.Runs() == 1);
    CHECK(loaded.BestScore(Difficulty::Standard) == 300000);
    CHECK(loaded.ChallengeCompleted(ChallengeId::WardenDown));
    CHECK_FALSE(loaded.ChallengeCompleted(ChallengeId::CoreDown));
    CHECK(loaded.ShipUnlocked(ShipId::Vector));
}

TEST_CASE("Codex discoveries accumulate across runs and persist") {
    TempDataDir dir;
    {
        World world(RunConfig{});
        world.SpawnEnemy(EnemyKind::Gunner, {0.0f, 0.0f}, 0.0f);
        world.ApplyUpgrade(UpgradeId::Overclock);
        world.ApplyUpgrade(UpgradeId::SpectrumCannon); // highest id: exercises the top of the 64-bit mask
        RunSummary run;
        run.stats = world.Record();
        Profile profile;
        profile.RecordRun(run);
        CHECK(profile.EnemyDiscovered(EnemyKind::Gunner));
        CHECK_FALSE(profile.EnemyDiscovered(EnemyKind::BossCore));
        CHECK(profile.UpgradeDiscovered(UpgradeId::SpectrumCannon));
    }
    Profile loaded;
    REQUIRE(loaded.Load());
    CHECK(loaded.EnemyDiscovered(EnemyKind::Gunner));
    CHECK(loaded.UpgradeDiscovered(UpgradeId::Overclock));
    CHECK(loaded.UpgradeDiscovered(UpgradeId::SpectrumCannon));
    CHECK_FALSE(loaded.UpgradeDiscovered(UpgradeId::HiveSwarm));
}

TEST_CASE("Recent runs are kept newest first, capped and persisted") {
    TempDataDir dir;
    {
        Profile profile;
        for (int i = 0; i < 12; ++i) {
            RunSummary run;
            run.stats.score = 1000 * (i + 1);
            run.stats.killedBy = EnemyKind::Gunner;
            run.config.mode = i == 11 ? GameMode::BossRush : GameMode::Campaign;
            run.sectorReached = 2;
            profile.RecordRun(run);
        }
        REQUIRE(profile.RecentRuns().size() == Profile::kRecentRuns);
        CHECK(profile.RecentRuns().front().score == 12000);
    }
    Profile loaded;
    REQUIRE(loaded.Load());
    REQUIRE(loaded.RecentRuns().size() == Profile::kRecentRuns);
    const RunRecord& newest = loaded.RecentRuns().front();
    CHECK(newest.score == 12000);
    CHECK(newest.mode == GameMode::BossRush);
    CHECK(newest.killedBy == EnemyKind::Gunner);
    CHECK(newest.sector == 2);
    CHECK(loaded.RecentRuns().back().score == 3000);
    CHECK(loaded.BossRushBest() == 12000);
}

TEST_CASE("Firmware purchases respect cost and rank limits") {
    TempDataDir dir;
    Profile profile;
    CHECK_FALSE(profile.TryBuyFirmware(FirmwareId::WeaponCalibration)); // no credits yet
    profile.CompleteChallenge(ChallengeId::CoreDown);                   // +250 credits
    const int before = profile.Credits();
    const int cost = profile.FirmwareNextCost(FirmwareId::WeaponCalibration);
    REQUIRE(cost > 0);
    CHECK(profile.TryBuyFirmware(FirmwareId::WeaponCalibration));
    CHECK(profile.Credits() == before - cost);
    CHECK(profile.FirmwareRank(FirmwareId::WeaponCalibration) == 1);
    CHECK(profile.Bonuses().damage == doctest::Approx(0.06f));
}

TEST_CASE("Run credits scale with difficulty and victory") {
    RunSummary run;
    run.stats.score = 500000;
    run.stats.kills = 2000;
    run.stats.sectorsCleared = 3;
    run.config.difficulty = Difficulty::Relaxed;
    const int relaxed = Profile::CreditsForRun(run);
    run.config.difficulty = Difficulty::Standard;
    const int standard = Profile::CreditsForRun(run);
    run.config.difficulty = Difficulty::Overclocked;
    const int overclocked = Profile::CreditsForRun(run);
    CHECK(relaxed < standard);
    CHECK(standard < overclocked);
    run.victory = true;
    CHECK(Profile::CreditsForRun(run) > overclocked);
}

TEST_CASE("Daily seed is stable per date and differs across dates") {
    CHECK(DailySeed(20261024) == DailySeed(20261024));
    CHECK(DailySeed(20261024) != DailySeed(20261025));
    const int today = TodayStamp();
    CHECK(today > 20000000);
}

TEST_CASE("Director walks a sector from intro to boss") {
    RunConfig config;
    World world(config);
    Director& director = world.GetDirector();
    CHECK(director.Phase() == SectorPhase::Intro);
    world.Update(4.0f, PlayerInput{});
    CHECK(director.Phase() == SectorPhase::Waves);
    CHECK(world.ActiveEnemyCount() >= 0);

    director.DebugJump(0, true);
    world.Update(0.5f, PlayerInput{});
    CHECK(director.Phase() == SectorPhase::Boss);
    const Enemy* boss = world.FindEnemy(director.BossId());
    REQUIRE(boss != nullptr);
    CHECK(boss->kind == EnemyKind::BossWarden);
}

TEST_CASE("Sector modifiers advance the run") {
    RunConfig config;
    World world(config);
    world.GetDirector().DebugJump(0, false);
    world.ChooseSectorModifier(SectorModifier::Respite);
    CHECK(world.GetDirector().Sector() == 1);
    CHECK(world.GetDirector().Phase() == SectorPhase::Intro);
    const int caches = world.PendingCaches();
    world.ChooseSectorModifier(SectorModifier::Armory);
    CHECK(world.PendingCaches() == caches + 1);
    CHECK(world.GetDirector().Sector() == 2);
}

TEST_CASE("Sector events stage their set pieces") {
    World world(RunConfig{});
    Director& director = world.GetDirector();
    director.JumpToWaves(1, 10.0f);
    const auto darts = [&] {
        return std::count_if(world.Enemies().begin(), world.Enemies().end(),
                             [](const Enemy& e) { return e.alive && e.kind == EnemyKind::Dart; });
    };
    const auto before = darts();
    director.DebugTriggerEvent(world, SectorEvent::Crossfire);
    world.Update(1.0f / 60.0f, PlayerInput{}); // spawns are queued until the next tick
    CHECK(darts() >= before + 9);              // 7 + 2 per sector index
    CHECK(director.ActiveEvent() == SectorEvent::Crossfire);

    const std::size_t pickups = world.Pickups().size();
    director.DebugTriggerEvent(world, SectorEvent::DataStorm);
    CHECK(world.Pickups().size() >= pickups + 30);
    for (const Pickup& p : world.Pickups()) {
        CHECK(CheckCollisionPointRec(p.position, World::kArena));
    }
}

TEST_CASE("Boss rush starts with a head-start build and skips the waves") {
    RunConfig config;
    config.mode = GameMode::BossRush;
    World world(config);
    CHECK(world.PendingLevelUps() == World::kBossRushStartLevels);
    for (int i = 0; i < 60 * 15 && world.GetDirector().Phase() != SectorPhase::Boss; ++i) {
        world.Update(1.0f / 60.0f, PlayerInput{});
    }
    CHECK(world.GetDirector().Phase() == SectorPhase::Boss);
    CHECK(world.Record().time < 10.0f);
    CHECK(world.Record().kills == 0);
}

TEST_CASE("Spatial hash finds every nearby item") {
    SpatialHash hash;
    hash.Configure({-1000.0f, -1000.0f, 2000.0f, 2000.0f}, 64.0f);
    std::vector<Vector2> points;
    Rng rng(5);
    for (int i = 0; i < 400; ++i) {
        points.push_back({rng.Range(-1000.0f, 1000.0f), rng.Range(-1000.0f, 1000.0f)});
    }
    hash.Build(static_cast<int>(points.size()), [&](int i, Vector2& p, float& r) {
        p = points[static_cast<std::size_t>(i)];
        r = 5.0f;
        return true;
    });
    for (int q = 0; q < 50; ++q) {
        const Vector2 center{rng.Range(-900.0f, 900.0f), rng.Range(-900.0f, 900.0f)};
        const float radius = rng.Range(20.0f, 250.0f);
        std::vector<int> found;
        hash.Query(center, radius, [&](int i) {
            if (Distance(points[static_cast<std::size_t>(i)], center) <= radius) {
                found.push_back(i);
            }
        });
        int expected = 0;
        for (const Vector2& p : points) {
            expected += Distance(p, center) <= radius ? 1 : 0;
        }
        CHECK(static_cast<int>(found.size()) == expected);
    }
}

TEST_CASE("Challenges evaluate against run state") {
    RunConfig config;
    World world(config);
    CHECK_FALSE(ChallengeMet(ChallengeId::Modular, world));
    for (UpgradeId id : {UpgradeId::OrbitBlades, UpgradeId::Drones, UpgradeId::ArcCoil, UpgradeId::PulseNova}) {
        world.ApplyUpgrade(id);
    }
    CHECK(ChallengeMet(ChallengeId::Modular, world));
    CHECK_FALSE(ChallengeMet(ChallengeId::MaxedOut, world));
    for (int i = 0; i < 4; ++i) {
        world.ApplyUpgrade(UpgradeId::ArcCoil);
    }
    CHECK(ChallengeMet(ChallengeId::MaxedOut, world));
    CHECK_FALSE(ChallengeMet(ChallengeId::WardenDown, world));

    const EntityId boss = world.SpawnEnemy(EnemyKind::BossWarden, {0.0f, -300.0f}, 0.0f).id;
    world.Update(1.0f / 60.0f, PlayerInput{});
    Enemy* e = world.FindEnemy(boss);
    REQUIRE(e != nullptr);
    world.DamageEnemy(*e, 1e9f, false, {0.0f, 1.0f});
    CHECK(ChallengeMet(ChallengeId::WardenDown, world));
    CHECK_FALSE(ChallengeMet(ChallengeId::FleetCommander, world)); // profile-based
}

TEST_CASE("Final guardian ends campaign and daily runs, but not endless") {
    for (GameMode mode : {GameMode::Campaign, GameMode::Daily, GameMode::Endless}) {
        RunConfig config;
        config.mode = mode;
        World world(config);
        Director& director = world.GetDirector();
        director.DebugJump(Director::kCampaignSectors - 1, true);
        world.Update(0.5f, PlayerInput{});
        REQUIRE(director.Phase() == SectorPhase::Boss);
        world.Update(2.5f, PlayerInput{}); // let the Core materialise and deploy its nodes
        // Shield nodes protect the Core; remove them, then the Core itself.
        for (Enemy& e : world.Enemies()) {
            if (e.alive && e.kind == EnemyKind::BossMinion) {
                world.KillEnemy(e, false);
            }
        }
        world.Update(0.1f, PlayerInput{});
        Enemy* core = world.FindEnemy(director.BossId());
        REQUIRE(core != nullptr);
        world.DamageEnemy(*core, 1e9f, false, {0.0f, 1.0f});
        world.Update(0.1f, PlayerInput{});
        CHECK(director.Phase() == SectorPhase::Cleared);
        world.Update(3.0f, PlayerInput{});
        if (mode == GameMode::Endless) {
            CHECK(director.Phase() == SectorPhase::Cleared);
        } else {
            CHECK(director.Phase() == SectorPhase::Victory);
        }
    }
}
