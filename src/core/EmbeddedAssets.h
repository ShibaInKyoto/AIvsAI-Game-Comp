#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace gb::assets {

struct EmbeddedFile {
    const char* path;
    const unsigned char* data;
    std::size_t size;
};

// Returns the bytes of an asset compiled into the executable (path relative to
// the assets/ directory), or an empty span when it does not exist.
std::span<const unsigned char> Find(std::string_view path);

// Returns a text asset as a null-terminated string, or nullptr when missing.
const char* FindText(std::string_view path);

namespace detail {
extern const EmbeddedFile kFiles[];
extern const std::size_t kFileCount;
} // namespace detail

} // namespace gb::assets
