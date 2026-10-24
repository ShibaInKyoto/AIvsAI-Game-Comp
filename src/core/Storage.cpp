#include "core/Storage.h"

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace gb::storage {

namespace {

std::optional<std::string> GetEnv(const char* name) {
#if defined(_MSC_VER)
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
    return value.empty() ? std::nullopt : std::optional<std::string>(value);
#else
    const char* value = std::getenv(name);
    return (value != nullptr && *value != '\0') ? std::optional<std::string>(value) : std::nullopt;
#endif
}

std::filesystem::path PlatformDirectory() {
#if defined(_WIN32)
    if (auto appData = GetEnv("APPDATA")) {
        return std::filesystem::path(*appData) / "Gridbreaker";
    }
#else
    if (auto xdg = GetEnv("XDG_DATA_HOME")) {
        return std::filesystem::path(*xdg) / "gridbreaker";
    }
    if (auto home = GetEnv("HOME")) {
        return std::filesystem::path(*home) / ".local" / "share" / "gridbreaker";
    }
#endif
    return std::filesystem::path("save");
}

std::filesystem::path& DirectoryStorage() {
    static std::filesystem::path directory;
    return directory;
}

std::string_view Trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

} // namespace

const std::filesystem::path& DataDirectory() {
    std::filesystem::path& directory = DirectoryStorage();
    if (directory.empty()) {
        directory = PlatformDirectory();
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) {
            directory = std::filesystem::path("save");
            std::filesystem::create_directories(directory, ec);
        }
    }
    return directory;
}

void SetDataDirectory(std::filesystem::path directory) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    DirectoryStorage() = std::move(directory);
}

std::optional<std::string> ReadTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

bool WriteTextFileAtomic(const std::filesystem::path& path, std::string_view contents) {
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            return false;
        }
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!file) {
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        // Some filesystems refuse to rename over an existing file.
        std::filesystem::remove(path, ec);
        std::filesystem::rename(temp, path, ec);
    }
    return !ec;
}

KeyValueDocument KeyValueDocument::Parse(std::string_view text) {
    KeyValueDocument doc;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        const std::string_view line = Trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        const std::string_view key = Trim(line.substr(0, eq));
        const std::string_view value = Trim(line.substr(eq + 1));
        if (!key.empty()) {
            doc.values_[std::string(key)] = std::string(value);
        }
    }
    return doc;
}

std::string KeyValueDocument::Serialize() const {
    std::string out;
    for (const auto& [key, value] : values_) {
        out += key;
        out += '=';
        out += value;
        out += '\n';
    }
    return out;
}

void KeyValueDocument::SetFloat(const std::string& key, float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.4f", static_cast<double>(value));
    Set(key, buffer);
}

std::optional<std::string> KeyValueDocument::Get(const std::string& key) const {
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return std::nullopt;
    }
    return it->second;
}

long long KeyValueDocument::GetInt(const std::string& key, long long fallback) const {
    const auto value = Get(key);
    if (!value) {
        return fallback;
    }
    long long result = fallback;
    const auto [ptr, ec] = std::from_chars(value->data(), value->data() + value->size(), result);
    return ec == std::errc() ? result : fallback;
}

float KeyValueDocument::GetFloat(const std::string& key, float fallback) const {
    const auto value = Get(key);
    if (!value) {
        return fallback;
    }
    // strtof is locale-sensitive but our writer always uses '.', and the game
    // never changes the C locale.
    char* end = nullptr;
    const float result = std::strtof(value->c_str(), &end);
    return end != value->c_str() ? result : fallback;
}

bool KeyValueDocument::GetBool(const std::string& key, bool fallback) const {
    const auto value = Get(key);
    if (!value) {
        return fallback;
    }
    return *value == "1" || *value == "true";
}

} // namespace gb::storage
