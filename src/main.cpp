#include "core/App.h"
#include "game/Profile.h"
#include "scenes/BootScene.h"
#include "scenes/GameScene.h"
#include "scenes/HangarScene.h"
#include "scenes/ResultsScene.h"
#include "scenes/TitleScene.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void PrintUsage() {
    std::printf(
        "Gridbreaker options:\n"
        "  --windowed              start in a window instead of borderless fullscreen\n"
        "  --size WxH              window size (default 1600x900)\n"
        "  --no-audio              disable the audio device\n"
        "  --autotest SECONDS      let the bot play for SECONDS of game time, then exit\n"
        "  --screenshot PATH       save a screenshot to PATH and exit\n"
        "  --screenshot-at SECONDS when to take the screenshot (default 3)\n"
        "  --scene NAME            start in a specific scene (title, game, hangar, codex, ...)\n"
        "  --seed N                fixed run seed\n"
        "  --sector N              (debug) start runs in sector N\n"
        "  --boss                  (debug) start runs at the sector boss\n"
        "  --perf                  show the performance overlay (toggle with F3)\n"
        "  --data-dir PATH         override the settings/save directory\n");
}

gb::LaunchOptions ParseArgs(int argc, char** argv) {
    gb::LaunchOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--windowed") {
            options.windowed = true;
        } else if (arg == "--size" && hasValue) {
            int w = 0;
            int h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                options.width = w;
                options.height = h;
                options.windowed = true;
            }
        } else if (arg == "--no-audio") {
            options.noAudio = true;
        } else if (arg == "--autotest" && hasValue) {
            options.autotestSeconds = std::strtof(argv[++i], nullptr);
        } else if (arg == "--screenshot" && hasValue) {
            options.screenshotPath = argv[++i];
        } else if (arg == "--screenshot-at" && hasValue) {
            options.screenshotAt = std::strtof(argv[++i], nullptr);
        } else if (arg == "--scene" && hasValue) {
            options.startScene = argv[++i];
        } else if (arg == "--seed" && hasValue) {
            options.seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--sector" && hasValue) {
            options.startSector = std::max(0, std::atoi(argv[++i]) - 1);
        } else if (arg == "--boss") {
            options.startAtBoss = true;
        } else if (arg == "--perf") {
            options.perfOverlay = true;
        } else if (arg == "--data-dir" && hasValue) {
            options.dataDirectory = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage();
            std::exit(0);
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    gb::App app(ParseArgs(argc, argv));
    return app.Run([](gb::App& a) -> std::unique_ptr<gb::Scene> {
        const gb::LaunchOptions& options = a.Options();
        const std::string& scene = options.startScene;
        // Automated runs and "--scene game|draft|pause" jump straight into a run.
        if (options.autotestSeconds > 0.0f || scene == "game" || scene == "draft" || scene == "evolution" ||
            scene == "pause" || scene == "nova") {
            gb::RunConfig config;
            config.seed = options.seed != 0 ? options.seed : 1234u;
            config.meta = a.Progress().Bonuses();
            return std::make_unique<gb::GameScene>(a, config);
        }
        if (scene == "hangar" || scene == "firmware") {
            return std::make_unique<gb::HangarScene>(a, scene == "firmware");
        }
        if (scene == "results") {
            gb::RunSummary summary;
            summary.stats.time = 754.0f;
            summary.stats.kills = 4210;
            summary.stats.score = 1284500;
            summary.stats.bossesKilled = 3;
            summary.stats.elitesKilled = 9;
            summary.stats.bestCombo = 412;
            summary.stats.damageDealt = 2.4e6;
            summary.stats.hitsTaken = 6;
            summary.stats.killedBy = gb::EnemyKind::Gunner;
            summary.level = 31;
            summary.sectorReached = 4;
            // A representative late-run build for screenshots.
            using U = gb::UpgradeId;
            for (U id : {U::Overclock, U::OrbitBlades, U::OrbitBlades, U::HighVoltage, U::SplitShot, U::Drones,
                         U::OrbitBlades, U::Thrusters, U::Drones, U::OrbitBlades, U::PiercingRounds, U::OrbitBlades,
                         U::SawStorm, U::ArcCoil, U::Overclock, U::ArcCoil, U::HullPlating, U::Overcharge}) {
                summary.build.Add(id);
            }
            return std::make_unique<gb::ResultsScene>(a, summary, 512);
        }
        if (scene == "title" || scene == "records" || scene == "codex") {
            return std::make_unique<gb::TitleScene>(a);
        }
        return std::make_unique<gb::BootScene>(a);
    });
}
