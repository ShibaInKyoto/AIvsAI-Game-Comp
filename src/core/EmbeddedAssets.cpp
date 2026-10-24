#include "core/EmbeddedAssets.h"

namespace gb::assets {

namespace {

const EmbeddedFile* Lookup(std::string_view path) {
    for (std::size_t i = 0; i < detail::kFileCount; ++i) {
        if (path == detail::kFiles[i].path) {
            return &detail::kFiles[i];
        }
    }
    return nullptr;
}

} // namespace

std::span<const unsigned char> Find(std::string_view path) {
    const EmbeddedFile* file = Lookup(path);
    if (file == nullptr) {
        return {};
    }
    return {file->data, file->size};
}

const char* FindText(std::string_view path) {
    const EmbeddedFile* file = Lookup(path);
    // Every embedded blob carries a trailing zero byte, so it is a valid C string.
    return file != nullptr ? reinterpret_cast<const char*>(file->data) : nullptr;
}

} // namespace gb::assets
