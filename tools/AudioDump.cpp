// Renders the procedural music and every sound effect to WAV files so the
// audio can be inspected (or listened to) without running the game.
//
//   gridbreaker_audiodump <output directory> [seconds of music per mood]

#include "audio/MusicEngine.h"
#include "audio/SoundBank.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace gb;

namespace {

void WriteWav(const std::filesystem::path& path, const std::vector<float>& samples, int channels, int sampleRate) {
    std::ofstream out(path, std::ios::binary);
    const auto dataBytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
    auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
    out.write("RIFF", 4);
    u32(36 + dataBytes);
    out.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(static_cast<std::uint16_t>(channels));
    u32(static_cast<std::uint32_t>(sampleRate));
    u32(static_cast<std::uint32_t>(sampleRate * channels * 2));
    u16(static_cast<std::uint16_t>(channels * 2));
    u16(16);
    out.write("data", 4);
    u32(dataBytes);
    for (float s : samples) {
        const auto v = static_cast<std::int16_t>(std::max(-1.0f, std::min(1.0f, s)) * 32767.0f);
        out.write(reinterpret_cast<const char*>(&v), 2);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <output dir> [seconds]\n", argv[0]);
        return 1;
    }
    const std::filesystem::path dir = argv[1];
    const float seconds = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 20.0f;
    std::filesystem::create_directories(dir);

    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        WriteWav(dir / ("sfx_" + std::to_string(i) + ".wav"), SynthesizeSfx(static_cast<SfxId>(i)), 1, kSfxSampleRate);
    }

    const struct {
        MusicMood mood;
        float intensity;
        const char* name;
    } moods[] = {{MusicMood::Menu, 0.5f, "menu"},        {MusicMood::Game, 0.45f, "game_low"},
                 {MusicMood::Game, 0.9f, "game_high"},   {MusicMood::Boss, 1.0f, "boss"},
                 {MusicMood::Victory, 0.75f, "victory"}, {MusicMood::GameOver, 0.15f, "gameover"}};
    for (const auto& m : moods) {
        auto music = std::make_unique<MusicEngine>();
        music->SetVolume(0.8f);
        music->SetMood(m.mood, 0);
        music->SetIntensity(m.intensity);
        std::vector<float> samples(static_cast<std::size_t>(seconds * MusicEngine::kSampleRate) * 2);
        constexpr unsigned kBlock = 1024;
        for (std::size_t offset = 0; offset + kBlock * 2 <= samples.size(); offset += kBlock * 2) {
            music->Render(samples.data() + offset, kBlock);
        }
        WriteWav(dir / (std::string("music_") + m.name + ".wav"), samples, 2, MusicEngine::kSampleRate);
    }
    std::printf("wrote audio to %s\n", dir.string().c_str());
    return 0;
}
