#include "core/App.h"
#include "scenes/SandboxScene.h"

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
        "  --scene NAME            start in a specific scene (title, game)\n"
        "  --seed N                fixed run seed\n"
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
    return app.Run([](gb::App& a) { return std::make_unique<gb::SandboxScene>(a); });
}
