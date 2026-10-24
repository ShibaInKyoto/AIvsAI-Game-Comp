#include "core/MathUtil.h"
#include "core/Random.h"
#include "core/Storage.h"

#include <doctest/doctest.h>

#include <array>
#include <filesystem>

using namespace gb;

TEST_CASE("Rng is deterministic for a given seed") {
    Rng a(1234);
    Rng b(1234);
    for (int i = 0; i < 100; ++i) {
        CHECK(a.Next() == b.Next());
    }
    Rng c(1235);
    bool anyDifferent = false;
    Rng d(1234);
    for (int i = 0; i < 10; ++i) {
        anyDifferent = anyDifferent || (c.Next() != d.Next());
    }
    CHECK(anyDifferent);
}

TEST_CASE("Rng ranges stay within bounds") {
    Rng rng(42);
    for (int i = 0; i < 10000; ++i) {
        const float f = rng.Float();
        CHECK(f >= 0.0f);
        CHECK(f < 1.0f);
        const int n = rng.RangeInt(-3, 5);
        CHECK(n >= -3);
        CHECK(n <= 5);
    }
    CHECK(rng.RangeInt(7, 7) == 7);
}

TEST_CASE("WeightedIndex respects zero weights") {
    Rng rng(7);
    const std::array<float, 4> weights{0.0f, 1.0f, 0.0f, 3.0f};
    std::array<int, 4> hits{};
    for (int i = 0; i < 4000; ++i) {
        const int idx = rng.WeightedIndex(weights);
        REQUIRE(idx >= 0);
        hits[static_cast<std::size_t>(idx)]++;
    }
    CHECK(hits[0] == 0);
    CHECK(hits[2] == 0);
    CHECK(hits[3] > hits[1] * 2);

    const std::array<float, 2> none{0.0f, 0.0f};
    CHECK(rng.WeightedIndex(none) == -1);
}

TEST_CASE("Angle helpers wrap correctly") {
    CHECK(WrapAngle(kTau + 0.5f) == doctest::Approx(0.5f).epsilon(1e-4));
    CHECK(WrapAngle(-kTau - 0.5f) == doctest::Approx(-0.5f).epsilon(1e-4));
    CHECK(AngleDelta(kPi - 0.1f, -kPi + 0.1f) == doctest::Approx(0.2f).epsilon(1e-3));
    CHECK(RotateToward(0.0f, 1.0f, 0.25f) == doctest::Approx(0.25f));
    CHECK(RotateToward(0.0f, 0.1f, 0.25f) == doctest::Approx(0.1f));
}

TEST_CASE("Damp converges toward the target") {
    float v = 0.0f;
    for (int i = 0; i < 600; ++i) {
        v = Damp(v, 10.0f, 0.1f, 1.0f / 60.0f);
    }
    CHECK(v == doctest::Approx(10.0f).epsilon(1e-3));
    // Halving per half-life.
    CHECK(Damp(0.0f, 1.0f, 0.5f, 0.5f) == doctest::Approx(0.5f).epsilon(1e-4));
}

TEST_CASE("KeyValueDocument round-trips values") {
    storage::KeyValueDocument doc;
    doc.SetInt("score", 123456789012LL);
    doc.SetFloat("volume", 0.25f);
    doc.SetBool("fullscreen", true);
    doc.Set("name", "VECTOR");

    const auto parsed = storage::KeyValueDocument::Parse(doc.Serialize());
    CHECK(parsed.GetInt("score", 0) == 123456789012LL);
    CHECK(parsed.GetFloat("volume", 0.0f) == doctest::Approx(0.25f));
    CHECK(parsed.GetBool("fullscreen", false));
    CHECK(parsed.Get("name").value_or("") == "VECTOR");
    CHECK(parsed.GetInt("missing", -1) == -1);
}

TEST_CASE("KeyValueDocument tolerates junk lines") {
    const auto doc = storage::KeyValueDocument::Parse("# comment\n\n  key = value \r\nbroken line\n=nokey\nnum=abc\n");
    CHECK(doc.Get("key").value_or("") == "value");
    CHECK(doc.GetInt("num", 5) == 5);
    CHECK(doc.Values().size() == 2);
}

TEST_CASE("Atomic writes replace file contents") {
    const auto dir = std::filesystem::temp_directory_path() / "gridbreaker_test_storage";
    std::filesystem::create_directories(dir);
    const auto path = dir / "file.txt";
    REQUIRE(storage::WriteTextFileAtomic(path, "first"));
    REQUIRE(storage::WriteTextFileAtomic(path, "second"));
    CHECK(storage::ReadTextFile(path).value_or("") == "second");
    std::filesystem::remove_all(dir);
}
