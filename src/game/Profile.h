#pragma once

#include "game/Challenges.h"
#include "game/Ships.h"
#include "game/World.h"

#include <array>
#include <string>

namespace gb {

// Permanent upgrades bought with credits between runs.
enum class FirmwareId : std::uint8_t {
    HullReinforcement,
    WeaponCalibration,
    DataCompression,
    MagneticField,
    BackupSectors,
    CapacitorCharge,
    ThrusterTuning,
    Count
};

struct FirmwareInfo {
    const char* name;
    const char* description;  // per rank
    int maxRank;
    std::array<int, 5> cost;  // cost of rank 1..maxRank
};

const FirmwareInfo& GetFirmwareInfo(FirmwareId id);

struct RunSummary {
    RunStats stats;
    RunConfig config;
    int level = 1;
    int sectorReached = 1;
    bool victory = false;
};

// Persistent player progression and records, stored in profile.sav.
class Profile {
public:
    bool Load();
    bool Save() const;

    int Credits() const { return credits_; }
    bool ShipUnlocked(ShipId id) const { return unlockedShips_[static_cast<std::size_t>(id)]; }
    bool TryUnlockShip(ShipId id);
    int FirmwareRank(FirmwareId id) const { return firmware_[static_cast<std::size_t>(id)]; }
    int FirmwareNextCost(FirmwareId id) const;  // -1 when maxed
    bool TryBuyFirmware(FirmwareId id);
    MetaBonuses Bonuses() const;

    // Applies a finished run; returns the credits it earned.
    int RecordRun(const RunSummary& run);
    static int CreditsForRun(const RunSummary& run);

    long long BestScore(Difficulty difficulty) const { return bestScore_[static_cast<std::size_t>(difficulty)]; }
    long long BestScoreForShip(ShipId id) const { return shipBestScore_[static_cast<std::size_t>(id)]; }
    int BestSector() const { return bestSector_; }
    int Runs() const { return runs_; }
    int Wins() const { return wins_; }
    long long TotalKills() const { return totalKills_; }
    int BossKills() const { return bossKills_; }
    float PlayTime() const { return playTime_; }

    bool ChallengeCompleted(ChallengeId id) const { return (challenges_ >> static_cast<unsigned>(id)) & 1u; }
    // Marks a challenge complete and pays out its reward; false if already done.
    bool CompleteChallenge(ChallengeId id);
    int CompletedChallengeCount() const;
    bool AllShipsUnlocked() const;

    // Codex discovery: enemies met and upgrades drafted in any run.
    bool EnemyDiscovered(EnemyKind kind) const { return (seenEnemies_ >> static_cast<unsigned>(kind)) & 1u; }
    bool UpgradeDiscovered(UpgradeId id) const { return (seenUpgrades_ >> static_cast<unsigned>(id)) & 1u; }

    bool TutorialSeen() const { return tutorialSeen_; }
    void SetTutorialSeen() { tutorialSeen_ = true; }

    ShipId LastShip() const { return lastShip_; }
    Difficulty LastDifficulty() const { return lastDifficulty_; }
    void SetLastSelection(ShipId ship, Difficulty difficulty) {
        lastShip_ = ship;
        lastDifficulty_ = difficulty;
    }

    // Daily challenge bookkeeping (date as YYYYMMDD).
    int DailyDate() const { return dailyDate_; }
    long long DailyBest() const { return dailyBest_; }

    void Reset();

private:
    int credits_ = 0;
    long long lifetimeCredits_ = 0;
    std::array<bool, static_cast<std::size_t>(ShipId::Count)> unlockedShips_{true};
    std::array<int, static_cast<std::size_t>(FirmwareId::Count)> firmware_{};
    std::array<long long, static_cast<std::size_t>(Difficulty::Count)> bestScore_{};
    std::array<long long, static_cast<std::size_t>(ShipId::Count)> shipBestScore_{};
    int bestSector_ = 0;
    int runs_ = 0;
    int wins_ = 0;
    long long totalKills_ = 0;
    int bossKills_ = 0;
    float playTime_ = 0.0f;
    bool tutorialSeen_ = false;
    ShipId lastShip_ = ShipId::Vector;
    Difficulty lastDifficulty_ = Difficulty::Standard;
    int dailyDate_ = 0;
    long long dailyBest_ = 0;
    std::uint32_t challenges_ = 0;
    std::uint32_t seenEnemies_ = 0;
    std::uint64_t seenUpgrades_ = 0;
};

// Today's date as YYYYMMDD in local time, and a seed derived from it.
int TodayStamp();
std::uint64_t DailySeed(int stamp);

} // namespace gb
