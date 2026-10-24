#pragma once

#include "render/Text.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace gb {

class App;

namespace ui {

// Dark translucent panel with a neon border and corner accents.
void Panel(App& app, Rectangle rect, Color accent, float alpha = 1.0f);
bool MouseOver(const App& app, Rectangle rect);
// Dims the whole screen (used behind overlays).
void Dim(App& app, float alpha);
// Small "[KEY] label" prompt that adapts to the active input device.
void Prompt(App& app, Vector2 position, const char* keyboardKey, const char* padButton, const char* label,
            Align align = Align::Left, float size = 22.0f);

// A vertical, device-agnostic menu: arrows/stick/d-pad move the cursor,
// the mouse hovers, confirm/click activates, left/right adjust values.
class Menu {
public:
    struct Item {
        std::string label;
        std::string value;  // optional right-aligned value (settings)
        bool enabled = true;
    };

    void SetItems(std::vector<Item> items);
    void SetValue(std::size_t index, std::string value);
    void SetSelected(int index) { selected_ = index; }
    int Selected() const { return selected_; }
    std::size_t Count() const { return items_.size(); }

    // Returns the index activated this frame, or -1.
    int Update(App& app);
    // -1 / +1 when the selected item was adjusted with left/right this frame.
    int Adjustment() const { return adjustment_; }

    void Draw(App& app, Vector2 position, float width, float itemHeight, Color accent);

private:
    std::vector<Item> items_;
    std::vector<Rectangle> rects_;
    std::vector<float> highlight_;
    int selected_ = 0;
    int adjustment_ = 0;
};

} // namespace ui
} // namespace gb
