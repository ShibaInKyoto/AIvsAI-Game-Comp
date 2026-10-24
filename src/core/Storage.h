#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace gb::storage {

// Per-user writable directory for settings and save data
// (%APPDATA%\Gridbreaker on Windows, $XDG_DATA_HOME/gridbreaker elsewhere).
// Created on first use; falls back to ./save when the platform location fails.
const std::filesystem::path& DataDirectory();

// Overrides the data directory (used by tests and the --data-dir flag).
void SetDataDirectory(std::filesystem::path directory);

std::optional<std::string> ReadTextFile(const std::filesystem::path& path);

// Writes via a temporary file and rename so a crash mid-write never corrupts
// the previous save.
bool WriteTextFileAtomic(const std::filesystem::path& path, std::string_view contents);

// Minimal "key=value" document used by the settings and profile files.
class KeyValueDocument {
public:
    static KeyValueDocument Parse(std::string_view text);
    std::string Serialize() const;

    void Set(const std::string& key, std::string value) { values_[key] = std::move(value); }
    void SetInt(const std::string& key, long long value) { Set(key, std::to_string(value)); }
    void SetFloat(const std::string& key, float value);
    void SetBool(const std::string& key, bool value) { Set(key, value ? "1" : "0"); }

    std::optional<std::string> Get(const std::string& key) const;
    long long GetInt(const std::string& key, long long fallback) const;
    float GetFloat(const std::string& key, float fallback) const;
    bool GetBool(const std::string& key, bool fallback) const;

    const std::map<std::string, std::string>& Values() const { return values_; }

private:
    std::map<std::string, std::string> values_;
};

} // namespace gb::storage
