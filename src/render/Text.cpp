#include "render/Text.h"

#include "core/EmbeddedAssets.h"
#include "core/MathUtil.h"

#include <string>
#include <vector>

namespace gb {

namespace {

constexpr int kSmallSize = 48;
constexpr int kLargeSize = 128;
constexpr float kLargeThreshold = 52.0f;

struct FontSource {
    const char* path;
};

constexpr std::array<FontSource, static_cast<std::size_t>(FontId::Count)> kSources{{
    {"fonts/Orbitron-Black.ttf"},
    {"fonts/Orbitron-Bold.ttf"},
    {"fonts/Rajdhani-SemiBold.ttf"},
    {"fonts/Rajdhani-Bold.ttf"},
}};

std::vector<int> BuildCodepoints() {
    std::vector<int> codepoints;
    for (int c = 32; c < 127; ++c) {
        codepoints.push_back(c);
    }
    // A few Latin-1 extras used by the UI.
    for (int c : {0xB0, 0xB7, 0xD7}) {
        codepoints.push_back(c);
    }
    return codepoints;
}

Font LoadEmbeddedFont(const char* path, int size, std::vector<int>& codepoints) {
    const auto bytes = assets::Find(path);
    if (bytes.empty()) {
        TraceLog(LOG_ERROR, "FONT: missing embedded font %s", path);
        return GetFontDefault();
    }
    Font font = LoadFontFromMemory(".ttf", bytes.data(), static_cast<int>(bytes.size()), size, codepoints.data(),
                                   static_cast<int>(codepoints.size()));
    if (font.texture.id == 0) {
        return GetFontDefault();
    }
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    return font;
}

// raylib wants null-terminated strings; keep one scratch buffer per thread.
const char* Terminated(std::string_view text) {
    thread_local std::string buffer;
    buffer.assign(text.data(), text.size());
    return buffer.c_str();
}

} // namespace

bool TextRenderer::Init() {
    std::vector<int> codepoints = BuildCodepoints();
    for (std::size_t i = 0; i < kSources.size(); ++i) {
        fonts_[i].small = LoadEmbeddedFont(kSources[i].path, kSmallSize, codepoints);
        fonts_[i].large = LoadEmbeddedFont(kSources[i].path, kLargeSize, codepoints);
    }
    return true;
}

void TextRenderer::Shutdown() {
    const Font defaultFont = GetFontDefault();
    for (auto& pair : fonts_) {
        for (Font* font : {&pair.small, &pair.large}) {
            if (font->texture.id != 0 && font->texture.id != defaultFont.texture.id) {
                UnloadFont(*font);
            }
            *font = {};
        }
    }
}

const Font& TextRenderer::Pick(FontId font, float size) const {
    const FontPair& pair = fonts_[static_cast<std::size_t>(font)];
    return size > kLargeThreshold ? pair.large : pair.small;
}

Vector2 TextRenderer::Measure(FontId font, std::string_view text, float size, float spacing) const {
    return MeasureTextEx(Pick(font, size), Terminated(text), size, spacing * size);
}

void TextRenderer::Draw(FontId font, std::string_view text, Vector2 position, float size, Color color, Align align,
                        float spacing) const {
    const Font& f = Pick(font, size);
    const char* str = Terminated(text);
    if (align != Align::Left) {
        const float width = MeasureTextEx(f, str, size, spacing * size).x;
        position.x -= align == Align::Center ? width * 0.5f : width;
    }
    DrawTextEx(f, str, position, size, spacing * size, color);
}

void TextRenderer::DrawGlow(FontId font, std::string_view text, Vector2 position, float size, Color color, Color glow,
                            Align align, float spacing) const {
    // Cheap halo: a ring of faint offset copies under the main text.
    const float radius = std::max(2.0f, size * 0.06f);
    const Color halo = WithAlpha(glow, static_cast<float>(glow.a) / 255.0f * 0.22f);
    for (int i = 0; i < 8; ++i) {
        const Vector2 offset = FromAngle(kTau * static_cast<float>(i) / 8.0f, radius);
        Draw(font, text, position + offset, size, halo, align, spacing);
    }
    Draw(font, text, position, size, color, align, spacing);
}

namespace {

// Splits text into lines that fit `width`, honouring explicit newlines.
template <typename MeasureFn>
std::vector<std::string> WrapLines(std::string_view text, float width, MeasureFn&& measure) {
    std::vector<std::string> lines;
    std::string current;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = text.find_first_of(" \n", pos);
        const std::size_t wordEnd = end == std::string_view::npos ? text.size() : end;
        const std::string_view word = text.substr(pos, wordEnd - pos);

        std::string candidate = current.empty() ? std::string(word) : current + " " + std::string(word);
        if (!current.empty() && measure(candidate) > width) {
            lines.push_back(current);
            current = std::string(word);
        } else {
            current = std::move(candidate);
        }

        if (end == std::string_view::npos) {
            break;
        }
        if (text[end] == '\n') {
            lines.push_back(current);
            current.clear();
        }
        pos = end + 1;
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

} // namespace

float TextRenderer::MeasureWrapped(FontId font, std::string_view text, float width, float size,
                                   float lineSpacing) const {
    const auto lines = WrapLines(text, width, [&](const std::string& s) { return Measure(font, s, size).x; });
    return static_cast<float>(lines.size()) * size * lineSpacing;
}

float TextRenderer::DrawWrapped(FontId font, std::string_view text, Vector2 position, float width, float size,
                                Color color, float lineSpacing, Align align) const {
    const auto lines = WrapLines(text, width, [&](const std::string& s) { return Measure(font, s, size).x; });
    float y = position.y;
    for (const auto& line : lines) {
        float x = position.x;
        if (align == Align::Center) {
            x += width * 0.5f;
        } else if (align == Align::Right) {
            x += width;
        }
        Draw(font, line, {x, y}, size, color, align);
        y += size * lineSpacing;
    }
    return y - position.y;
}

} // namespace gb
