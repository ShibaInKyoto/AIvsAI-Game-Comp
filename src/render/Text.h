#pragma once

#include <raylib.h>

#include <array>
#include <string_view>

namespace gb {

enum class FontId {
    Display,  // Orbitron Black: logo and big headings
    Heading,  // Orbitron Bold: titles, numbers
    Body,     // Rajdhani SemiBold: descriptions
    BodyBold, // Rajdhani Bold: emphasised body text, buttons
    Count
};

enum class Align {
    Left,
    Center,
    Right
};

// Owns the embedded fonts (each rasterised at two sizes with mipmaps so text
// stays crisp from tiny HUD labels to the title logo).
class TextRenderer {
public:
    bool Init();
    void Shutdown();

    void Draw(FontId font, std::string_view text, Vector2 position, float size, Color color, Align align = Align::Left,
              float spacing = 0.0f) const;

    // Draws text with a soft coloured halo behind it.
    void DrawGlow(FontId font, std::string_view text, Vector2 position, float size, Color color, Color glow,
                  Align align = Align::Left, float spacing = 0.0f) const;

    Vector2 Measure(FontId font, std::string_view text, float size, float spacing = 0.0f) const;

    // Draws word-wrapped text inside `width`; returns the total height used.
    float DrawWrapped(FontId font, std::string_view text, Vector2 position, float width, float size, Color color,
                      float lineSpacing = 1.1f, Align align = Align::Left) const;
    float MeasureWrapped(FontId font, std::string_view text, float width, float size, float lineSpacing = 1.1f) const;

private:
    struct FontPair {
        Font small{};
        Font large{};
    };

    const Font& Pick(FontId font, float size) const;

    std::array<FontPair, static_cast<std::size_t>(FontId::Count)> fonts_{};
};

} // namespace gb
