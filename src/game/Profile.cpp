#include "game/Profile.h"

#include "core/Storage.h"

#include <algorithm>
#include <ctime>

namespace gb {

namespace {

constexpr const char* kFileName = "profile.sav";
constexpr int kFormatVersion = 1;

// clang-format off
constexpr std::array<FirmwareInfo, static_cast<std::size_t>(FirmwareId::Count)> kFirmware{{
    {"HULL REINFORCEMENT", "+1 max integrity", 2, {180, 450, 0, 0, 0}},
    {"WEAPON CALIBRATION", "+6% damage", 5, {80, 160, 260, 380, 520}},
    {"DATA COMPRESSION", "+8% experience", 5, {70, 140, 230, 340, 470}},
    {"MAGNETIC FIELD", "+15% pickup radius", 3, {60, 140, 240, 0, 0}},
    {"BACKUP SECTORS", "+1 reroll per run", 3, {100, 220, 380, 0, 0}},
    {"CAPACITOR CHARGE", "Start with +25% nova charge", 3, {90, 200, 330, 0, 0}},
    {"THRUSTER TUNING", "-6% dash cooldown", 3, {70, 150, 260, 0, 0}},
}};
// clang-format on

} // namespace

const FirmwareInfo& GetFirmwareInfo(FirmwareId id) { return kFirmware[static_cast<std::size_t>(id)]; }

void Profile::Reset() { *this = Profile{}; }

bool Profile::Load() {
    const auto text = storage::ReadTextFile(storage::DataDirectory() / kFileName);
    if (!text) {
        return false;
    }
    const auto doc = storage::KeyValueDocument::Parse(*text);
    if (doc.GetInt("version", 0) < 1) {
        return false;
    }
    credits_ = static_cast<int>(std::max(0LL, doc.GetInt("credits", 0)));
    lifetimeCredits_ = doc.GetInt("lifetime_credits", 0);
    for (std::size_t i = 0; i < unlockedShips_.size(); ++i) {
        unlockedShips_[i] = i == 0 || doc.GetBool("ship_" + std::to_string(i), false);
        shipBestScore_[i] = doc.GetInt("ship_best_" + std::to_string(i), 0);
    }
    for (std::size_t i = 0; i < firmware_.size(); ++i) {
        const int maxRank = kFirmware[i].maxRank;
        firmware_[i] = std::clamp(static_cast<int>(doc.GetInt("firmware_" + std::to_string(i), 0)), 0, maxRank);
    }
    for (std::size_t i = 0; i < bestScore_.size(); ++i) {
        bestScore_[i] = doc.GetInt("best_" + std::to_string(i), 0);
    }
    bestSector_ = static_cast<int>(doc.GetInt("best_sector", 0));
    runs_ = static_cast<int>(doc.GetInt("runs", 0));
    wins_ = static_cast<int>(doc.GetInt("wins", 0));
    totalKills_ = doc.GetInt("kills", 0);
    bossKills_ = static_cast<int>(doc.GetInt("boss_kills", 0));
    playTime_ = doc.GetFloat("play_time", 0.0f);
    tutorialSeen_ = doc.GetBool("tutorial_seen", false);
    lastShip_ = static_cast<ShipId>(
        std::clamp(static_cast<int>(doc.GetInt("last_ship", 0)), 0, static_cast<int>(ShipId::Count) - 1));
    lastDifficulty_ = static_cast<Difficulty>(
        std::clamp(static_cast<int>(doc.GetInt("last_difficulty", 1)), 0, static_cast<int>(Difficulty::Count) - 1));
    lastMode_ = static_cast<GameMode>(
        std::clamp(static_cast<int>(doc.GetInt("last_mode", 0)), 0, static_cast<int>(GameMode::Count) - 1));
    if (lastMode_ == GameMode::Daily) {
        lastMode_ = GameMode::Campaign; // the daily run is started from the title screen
    }
    if (!ShipUnlocked(lastShip_)) {
        lastShip_ = ShipId::Vector;
    }
    dailyDate_ = static_cast<int>(doc.GetInt("daily_date", 0));
    dailyBest_ = doc.GetInt("daily_best", 0);
    bossRushBest_ = doc.GetInt("boss_rush_best", 0);
    challenges_ = static_cast<std::uint32_t>(doc.GetInt("challenges", 0));
    seenEnemies_ = static_cast<std::uint32_t>(doc.GetInt("codex_enemies", 0));
    seenUpgrades_ = static_cast<std::uint64_t>(doc.GetInt("codex_upgrades", 0));
    return true;
}

bool Profile::Save() const {
    storage::KeyValueDocument doc;
    doc.SetInt("version", kFormatVersion);
    doc.SetInt("credits", credits_);
    doc.SetInt("lifetime_credits", lifetimeCredits_);
    for (std::size_t i = 0; i < unlockedShips_.size(); ++i) {
        doc.SetBool("ship_" + std::to_string(i), unlockedShips_[i]);
        doc.SetInt("ship_best_" + std::to_string(i), shipBestScore_[i]);
    }
    for (std::size_t i = 0; i < firmware_.size(); ++i) {
        doc.SetInt("firmware_" + std::to_string(i), firmware_[i]);
    }
    for (std::size_t i = 0; i < bestScore_.size(); ++i) {
        doc.SetInt("best_" + std::to_string(i), bestScore_[i]);
    }
    doc.SetInt("best_sector", bestSector_);
    doc.SetInt("runs", runs_);
    doc.SetInt("wins", wins_);
    doc.SetInt("kills", totalKills_);
    doc.SetInt("boss_kills", bossKills_);
    doc.SetFloat("play_time", playTime_);
    doc.SetBool("tutorial_seen", tutorialSeen_);
    doc.SetInt("last_ship", static_cast<int>(lastShip_));
    doc.SetInt("last_difficulty", static_cast<int>(lastDifficulty_));
    doc.SetInt("last_mode", static_cast<int>(lastMode_));
    doc.SetInt("daily_date", dailyDate_);
    doc.SetInt("daily_best", dailyBest_);
    doc.SetInt("boss_rush_best", bossRushBest_);
    doc.SetInt("challenges", challenges_);
    doc.SetInt("codex_enemies", seenEnemies_);
    doc.SetInt("codex_upgrades", static_cast<long long>(seenUpgrades_));
    return storage::WriteTextFileAtomic(storage::DataDirectory() / kFileName, doc.Serialize());
}

bool Profile::TryUnlockShip(ShipId id) {
    const int cost = GetShipInfo(id).cost;
    if (ShipUnlocked(id) || credits_ < cost) {
        return false;
    }
    credits_ -= cost;
    unlockedShips_[static_cast<std::size_t>(id)] = true;
    Save();
    return true;
}

bool Profile::CompleteChallenge(ChallengeId id) {
    if (ChallengeCompleted(id)) {
        return false;
    }
    challenges_ |= 1u << static_cast<unsigned>(id);
    const int reward = GetChallengeInfo(id).reward;
    credits_ += reward;
    lifetimeCredits_ += reward;
    Save();
    return true;
}

int Profile::CompletedChallengeCount() const {
    int count = 0;
    for (const ChallengeInfo& info : AllChallenges()) {
        count += ChallengeCompleted(info.id) ? 1 : 0;
    }
    return count;
}

bool Profile::AllShipsUnlocked() const {
    for (bool unlocked : unlockedShips_) {
        if (!unlocked) {
            return false;
        }
    }
    return true;
}

int Profile::FirmwareNextCost(FirmwareId id) const {
    const FirmwareInfo& info = GetFirmwareInfo(id);
    const int rank = FirmwareRank(id);
    if (rank >= info.maxRank) {
        return -1;
    }
    return info.cost[static_cast<std::size_t>(rank)];
}

bool Profile::TryBuyFirmware(FirmwareId id) {
    const int cost = FirmwareNextCost(id);
    if (cost < 0 || credits_ < cost) {
        return false;
    }
    credits_ -= cost;
    firmware_[static_cast<std::size_t>(id)]++;
    Save();
    return true;
}

MetaBonuses Profile::Bonuses() const {
    MetaBonuses b;
    b.bonusHp = FirmwareRank(FirmwareId::HullReinforcement);
    b.damage = 0.06f * static_cast<float>(FirmwareRank(FirmwareId::WeaponCalibration));
    b.xp = 0.08f * static_cast<float>(FirmwareRank(FirmwareId::DataCompression));
    b.magnet = 0.15f * static_cast<float>(FirmwareRank(FirmwareId::MagneticField));
    b.rerolls = FirmwareRank(FirmwareId::BackupSectors);
    b.novaCharge = 0.25f * static_cast<float>(FirmwareRank(FirmwareId::CapacitorCharge));
    b.dashCooldown = 0.06f * static_cast<float>(FirmwareRank(FirmwareId::ThrusterTuning));
    return b;
}

int Profile::CreditsForRun(const RunSummary& run) {
    const RunStats& s = run.stats;
    float credits = static_cast<float>(s.score) / 1500.0f + static_cast<float>(s.sectorsCleared) * 45.0f +
                    static_cast<float>(s.bossesKilled) * 35.0f + static_cast<float>(s.kills) / 12.0f +
                    static_cast<float>(s.elitesKilled) * 8.0f;
    switch (run.config.difficulty) {
    case Difficulty::Relaxed: credits *= 0.6f; break;
    case Difficulty::Overclocked: credits *= 1.5f; break;
    default: break;
    }
    if (run.victory) {
        credits += 150.0f;
    }
    if (run.config.mode == GameMode::BossRush) {
        credits *= 0.5f; // five guardians in a few minutes: not a farming shortcut
    }
    return std::max(5, static_cast<int>(credits));
}

int Profile::RecordRun(const RunSummary& run) {
    const int earned = CreditsForRun(run);
    credits_ += earned;
    lifetimeCredits_ += earned;
    runs_++;
    wins_ += run.victory ? 1 : 0;
    totalKills_ += run.stats.kills;
    bossKills_ += run.stats.bossesKilled;
    playTime_ += run.stats.time;
    bestSector_ = std::max(bestSector_, run.sectorReached);
    seenEnemies_ |= run.stats.enemiesSeen;
    seenUpgrades_ |= run.stats.upgradesTaken;
    auto& best = run.config.mode == GameMode::BossRush ? bossRushBest_
                                                       : bestScore_[static_cast<std::size_t>(run.config.difficulty)];
    best = std::max(best, run.stats.score);
    auto& shipBest = shipBestScore_[static_cast<std::size_t>(run.config.ship)];
    shipBest = std::max(shipBest, run.stats.score);
    if (run.config.mode == GameMode::Daily) {
        const int today = TodayStamp();
        if (dailyDate_ != today) {
            dailyDate_ = today;
            dailyBest_ = 0;
        }
        dailyBest_ = std::max(dailyBest_, run.stats.score);
    }
    Save();
    return earned;
}

long long Profile::BestScoreFor(const RunConfig& config) const {
    return config.mode == GameMode::BossRush ? bossRushBest_ : BestScore(config.difficulty);
}

int TodayStamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}

std::uint64_t DailySeed(int stamp) {
    std::uint64_t x = static_cast<std::uint64_t>(stamp) * 0x9E3779B97F4A7C15ULL;
    x ^= x >> 31;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 29;
    return x;
}

} // namespace gb
