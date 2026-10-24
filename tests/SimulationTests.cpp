#include "game/Bot.h"
#include "game/EnemyCatalog.h"
#include "game/World.h"

#include <doctest/doctest.h>

#include <algorithm>

using namespace gb;

namespace {

// Runs the world with the bot for `seconds`, auto-resolving level-ups and
// sector transitions the way the game scene would.
void RunBot(World& world, float seconds) {
    Bot bot;
    const float dt = 1.0f / 60.0f;
    for (float t = 0.0f; t < seconds && world.PlayerAlive(); t += dt) {
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
        if (world.GetDirector().Phase() == SectorPhase::Cleared) {
            world.ChooseSectorModifier(SectorModifier::Respite);
        }
        world.Update(dt, bot.Think(world, dt));
        world.Events().clear();
    }
}

} // namespace

TEST_CASE("World spawns enemies and the bot scores kills") {
    RunConfig config;
    config.seed = 99;
    World world(config);
    RunBot(world, 60.0f);
    CHECK(world.Record().kills > 10);
    CHECK(world.Record().score > 0);
    CHECK(world.Level() > 1);
}

TEST_CASE("Every ship can play a short run without errors") {
    for (const ShipInfo& ship : AllShips()) {
        RunConfig config;
        config.ship = ship.id;
        config.seed = 7;
        World world(config);
        RunBot(world, 30.0f);
        CHECK(world.Record().time > 0.0f);
        CHECK(world.Enemies().size() <= static_cast<std::size_t>(World::kMaxEnemies));
    }
}

TEST_CASE("Upgrades stack and respect their limits") {
    RunConfig config;
    World world(config);
    const int baseProjectiles = world.Stats().projectiles;
    world.ApplyUpgrade(UpgradeId::SplitShot);
    CHECK(world.Stats().projectiles == baseProjectiles + 1);

    const int baseHp = world.Stats().maxHp;
    world.ApplyUpgrade(UpgradeId::HullPlating);
    CHECK(world.Stats().maxHp == baseHp + 1);
    CHECK(world.GetPlayer().hp == world.Stats().maxHp);

    for (int i = 0; i < 5; ++i) {
        world.ApplyUpgrade(UpgradeId::OrbitBlades);
    }
    CHECK(world.Stats().Module(ModuleId::OrbitBlades) == 5);
    CHECK_FALSE(world.Inventory().CanOffer(UpgradeId::OrbitBlades, world.Stats(), world.GetPlayer().hp));
}

TEST_CASE("Module cap limits new modules to four") {
    RunConfig config;
    World world(config);
    world.ApplyUpgrade(UpgradeId::OrbitBlades);
    world.ApplyUpgrade(UpgradeId::Drones);
    world.ApplyUpgrade(UpgradeId::ArcCoil);
    world.ApplyUpgrade(UpgradeId::PulseNova);
    CHECK(world.Inventory().ModuleCount() == 4);
    CHECK_FALSE(world.Inventory().CanOffer(UpgradeId::PrismLance, world.Stats(), world.GetPlayer().hp));
    CHECK(world.Inventory().CanOffer(UpgradeId::OrbitBlades, world.Stats(), world.GetPlayer().hp));
}

TEST_CASE("Offers are distinct and never contain maxed upgrades") {
    RunConfig config;
    World world(config);
    for (int i = 0; i < 6; ++i) {
        world.ApplyUpgrade(UpgradeId::Overclock);
    }
    for (int roll = 0; roll < 200; ++roll) {
        const auto offers = world.RollOffers(3);
        REQUIRE(offers.size() == 3);
        CHECK(offers[0] != offers[1]);
        CHECK(offers[1] != offers[2]);
        CHECK(offers[0] != offers[2]);
        for (UpgradeId id : offers) {
            CHECK(id != UpgradeId::Overclock);
            CHECK(id != UpgradeId::NanoRepair); // player is at full health
        }
    }
}

TEST_CASE("Experience curve increases monotonically") {
    for (int level = 1; level < 60; ++level) {
        CHECK(World::XpForLevel(level + 1) > World::XpForLevel(level));
    }
}

TEST_CASE("Damage kills enemies and drops experience") {
    RunConfig config;
    World world(config);
    Enemy& spawned = world.SpawnEnemy(EnemyKind::Drifter, {500.0f, 0.0f}, 0.0f);
    const EntityId id = spawned.id;
    world.Update(1.0f / 60.0f, PlayerInput{});
    Enemy* enemy = world.FindEnemy(id);
    REQUIRE(enemy != nullptr);
    CHECK(world.DamageEnemy(*enemy, 1e6f, false, {1.0f, 0.0f}));
    world.Update(1.0f / 60.0f, PlayerInput{});
    CHECK(world.Record().kills == 1);
    CHECK_FALSE(world.Pickups().empty());
}

TEST_CASE("Player takes damage, gets invulnerability and can die") {
    RunConfig config;
    World world(config);
    const int hp = world.GetPlayer().hp;
    world.DamagePlayer({100.0f, 0.0f});
    CHECK(world.GetPlayer().hp == hp - 1);
    world.DamagePlayer({100.0f, 0.0f}); // invulnerable now
    CHECK(world.GetPlayer().hp == hp - 1);
    for (int i = 0; i < hp + 2; ++i) {
        world.Update(2.0f, PlayerInput{}); // let invulnerability lapse
        world.DamagePlayer({100.0f, 0.0f}, EnemyKind::Lancer);
    }
    CHECK_FALSE(world.PlayerAlive());
    CHECK(world.Record().killedBy == EnemyKind::Lancer);
}

TEST_CASE("Tail segments are attributed to the head of their chain") {
    World world(RunConfig{});
    world.SpawnEnemy(EnemyKind::Weaver, {300.0f, 0.0f}, 0.0f);
    world.Update(0.1f, PlayerInput{}); // the head grows its tail on its first update
    world.Update(0.1f, PlayerInput{});
    int segments = 0;
    for (const Enemy& e : world.Enemies()) {
        if (e.alive && e.kind == EnemyKind::WeaverBody) {
            ++segments;
            CHECK(world.ChainHeadKind(e) == EnemyKind::Weaver);
        }
    }
    CHECK(segments > 0);
}

TEST_CASE("Bosses can be spawned and defeated") {
    for (EnemyKind boss : {EnemyKind::BossWarden, EnemyKind::BossHydra, EnemyKind::BossSingularity,
                           EnemyKind::BossArchitect, EnemyKind::BossCore}) {
        RunConfig config;
        World world(config);
        const EntityId id = world.SpawnEnemy(boss, {0.0f, -300.0f}, 0.0f).id;
        for (int i = 0; i < 600; ++i) {
            world.Update(1.0f / 60.0f, PlayerInput{});
            world.Events().clear();
        }
        Enemy* e = world.FindEnemy(id);
        REQUIRE(e != nullptr);
        // The Core is shielded by nodes; clear them first.
        for (Enemy& other : world.Enemies()) {
            if (other.alive && other.kind == EnemyKind::BossMinion) {
                world.KillEnemy(other, false);
            }
        }
        world.Update(1.0f / 60.0f, PlayerInput{});
        e = world.FindEnemy(id);
        REQUIRE(e != nullptr);
        world.DamageEnemy(*e, 1e7f, false, {0.0f, 1.0f});
        world.Update(1.0f / 60.0f, PlayerInput{});
        CHECK(world.Record().bossesKilled == 1);
    }
}

TEST_CASE("Evolutions unlock from a maxed module plus its catalyst") {
    RunConfig config;
    World world(config);
    const auto canOffer = [&](UpgradeId id) {
        return world.Inventory().CanOffer(id, world.Stats(), world.GetPlayer().hp);
    };
    CHECK_FALSE(canOffer(UpgradeId::SawStorm));
    for (int i = 0; i < 5; ++i) {
        world.ApplyUpgrade(UpgradeId::OrbitBlades);
    }
    CHECK_FALSE(canOffer(UpgradeId::SawStorm)); // catalyst missing
    world.ApplyUpgrade(UpgradeId::Thrusters);
    CHECK(canOffer(UpgradeId::SawStorm));

    const auto offers = world.RollOffers(3);
    CHECK(std::find(offers.begin(), offers.end(), UpgradeId::SawStorm) != offers.end());

    world.ApplyUpgrade(UpgradeId::SawStorm);
    CHECK(world.Stats().Evolved(ModuleId::OrbitBlades));
    CHECK(world.OrbitBladeCount() == 12);
    CHECK_FALSE(canOffer(UpgradeId::SawStorm)); // only once
}
