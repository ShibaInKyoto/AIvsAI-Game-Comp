#include "audio/MusicEngine.h"
#include "audio/SoundBank.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <vector>

using namespace gb;

TEST_CASE("Every sound effect synthesises finite, audible samples") {
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        const auto samples = SynthesizeSfx(static_cast<SfxId>(i));
        REQUIRE_FALSE(samples.empty());
        float peak = 0.0f;
        for (float s : samples) {
            REQUIRE(std::isfinite(s));
            peak = std::max(peak, std::abs(s));
        }
        CHECK(peak > 0.05f);
        CHECK(peak <= 1.0f);
        CHECK(samples.size() < static_cast<std::size_t>(kSfxSampleRate * 4));
    }
}

TEST_CASE("Sound synthesis is deterministic") {
    CHECK(SynthesizeSfx(SfxId::Explosion) == SynthesizeSfx(SfxId::Explosion));
}

TEST_CASE("Music engine renders bounded audio in every mood") {
    auto music = std::make_unique<MusicEngine>();
    music->SetVolume(1.0f);
    std::vector<float> buffer(2048 * 2);
    for (MusicMood mood : {MusicMood::Menu, MusicMood::Game, MusicMood::Boss, MusicMood::Victory,
                           MusicMood::GameOver}) {
        music->SetMood(mood, 3);
        music->SetIntensity(0.9f);
        double energy = 0.0;
        std::size_t count = 0;
        // ~6 seconds per mood, enough to pass the crossfade and several bars.
        for (int block = 0; block < 130; ++block) {
            music->Render(buffer.data(), 2048);
            for (float s : buffer) {
                REQUIRE(std::isfinite(s));
                REQUIRE(std::abs(s) <= 1.0f);
                energy += static_cast<double>(s) * s;
                ++count;
            }
        }
        CHECK(std::sqrt(energy / static_cast<double>(count)) > 0.01);
    }
}
