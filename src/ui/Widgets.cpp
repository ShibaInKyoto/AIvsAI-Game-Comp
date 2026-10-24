#include "ui/Widgets.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "view/Palette.h"

#include <algorithm>

namespace gb::ui {

void Panel(App& app, Rectangle r, Color accent, float alpha) {
    DrawRectangleRec(r, WithAlpha(palette::kUiPanel, 0.96f * alpha));
    NeonRenderer& neon = app.Gfx().Neon();
    neon.Begin();
    const std::array<Vector2, 4> corners{Vector2{r.x, r.y}, Vector2{r.x + r.width, r.y},
                                         Vector2{r.x + r.width, r.y + r.height}, Vector2{r.x, r.y + r.height}};
    neon.Polyline(corners, true, 1.5f, WithAlpha(palette::kUiBorder, 0.7f * alpha));
    const float len = std::min(28.0f, std::min(r.width, r.height) * 0.3f);
    for (std::size_t i = 0; i < 4; ++i) {
        const Vector2 c = corners[i];
        const Vector2 a = Normalized(corners[(i + 1) % 4] - c);
        const Vector2 b = Normalized(corners[(i + 3) % 4] - c);
        neon.Line(c, c + a * len, 3.0f, WithAlpha(accent, alpha));
        neon.Line(c, c + b * len, 3.0f, WithAlpha(accent, alpha));
    }
    neon.End();
}

bool MouseOver(const App& app, Rectangle r) { return CheckCollisionPointRec(app.UiMouse(), r); }

void Dim(App& app, float alpha) {
    const Vector2 ui = app.Gfx().UiSize();
    DrawRectangleRec({0.0f, 0.0f, ui.x, ui.y}, WithAlpha(Color{2, 4, 12, 255}, alpha));
}

void Prompt(App& app, Vector2 position, const char* keyboardKey, const char* padButton, const char* label,
            Align align, float size) {
    const bool pad = app.In().LastDevice() == InputDevice::Gamepad;
    const char* key = pad ? padButton : keyboardKey;
    TextRenderer& text = app.Gfx().Text();
    const std::string full = std::string("[") + key + "]  " + label;
    const float width = text.Measure(FontId::BodyBold, full, size).x;
    float x = position.x;
    if (align == Align::Center) {
        x -= width * 0.5f;
    } else if (align == Align::Right) {
        x -= width;
    }
    const std::string keyPart = std::string("[") + key + "]  ";
    text.Draw(FontId::BodyBold, keyPart, {x, position.y}, size, palette::kUiAccent);
    text.Draw(FontId::BodyBold, label, {x + text.Measure(FontId::BodyBold, keyPart, size).x, position.y}, size,
              palette::kUiDim);
}

void Menu::SetItems(std::vector<Item> items) {
    items_ = std::move(items);
    rects_.assign(items_.size(), Rectangle{});
    highlight_.assign(items_.size(), 0.0f);
    selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(items_.size()) - 1));
}

void Menu::SetValue(std::size_t index, std::string value) {
    if (index < items_.size()) {
        items_[index].value = std::move(value);
    }
}

int Menu::Update(App& app) {
    adjustment_ = 0;
    if (items_.empty()) {
        return -1;
    }
    Input& in = app.In();
    const int count = static_cast<int>(items_.size());
    auto move = [&](int dir) {
        for (int i = 0; i < count; ++i) {
            selected_ = (selected_ + dir + count) % count;
            if (items_[static_cast<std::size_t>(selected_)].enabled) {
                break;
            }
        }
        app.Audio().Play(SfxId::UiMove, 0.6f);
    };
    if (in.Repeated(Action::Up)) {
        move(-1);
    }
    if (in.Repeated(Action::Down)) {
        move(1);
    }
    if (in.Repeated(Action::Left)) {
        adjustment_ = -1;
    }
    if (in.Repeated(Action::Right)) {
        adjustment_ = 1;
    }

    // Mouse hover selects; clicking activates.
    int hovered = -1;
    for (int i = 0; i < count; ++i) {
        if (items_[static_cast<std::size_t>(i)].enabled && MouseOver(app, rects_[static_cast<std::size_t>(i)])) {
            hovered = i;
        }
    }
    if (hovered >= 0 && in.MouseMoved() && hovered != selected_) {
        selected_ = hovered;
        app.Audio().Play(SfxId::UiMove, 0.4f);
    }
    if (hovered >= 0 && in.MouseClicked()) {
        selected_ = hovered;
        if (!items_[static_cast<std::size_t>(selected_)].value.empty()) {
            adjustment_ = 1;  // clicking a value item cycles it
            return -1;
        }
        return selected_;
    }
    if (in.Pressed(Action::Confirm) && items_[static_cast<std::size_t>(selected_)].enabled) {
        return selected_;
    }
    return -1;
}

void Menu::Draw(App& app, Vector2 position, float width, float itemHeight, Color accent) {
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const float dt = GetFrameTime();
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const bool selected = static_cast<int>(i) == selected_;
        highlight_[i] = Damp(highlight_[i], selected ? 1.0f : 0.0f, 0.05f, dt);
        rects_[i] = {position.x, position.y + static_cast<float>(i) * itemHeight, width, itemHeight - 6.0f};
    }
    // Selection bars.
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const float h = highlight_[i];
        if (h > 0.01f) {
            const Rectangle r = rects_[i];
            DrawRectangleRec({r.x, r.y, r.width * (0.6f + 0.4f * h), r.height}, WithAlpha(accent, 0.12f * h));
        }
    }
    neon.Begin();
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const float h = highlight_[i];
        if (h > 0.01f) {
            const Rectangle r = rects_[i];
            neon.Line({r.x, r.y}, {r.x, r.y + r.height}, 4.0f, WithAlpha(accent, h));
        }
    }
    neon.End();
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Item& item = items_[i];
        const Rectangle r = rects_[i];
        const float h = highlight_[i];
        const float textSize = itemHeight * 0.48f;
        const float y = r.y + (r.height - textSize) * 0.5f;
        Color c = item.enabled ? LerpColor(palette::kUiDim, WHITE, h) : palette::kUiFaint;
        text.Draw(FontId::Heading, item.label, {r.x + 22.0f + 10.0f * h, y}, textSize, c, Align::Left, 0.08f);
        if (!item.value.empty()) {
            const std::string value = h > 0.5f ? "<  " + item.value + "  >" : item.value;
            text.Draw(FontId::BodyBold, value, {r.x + r.width - 16.0f, y - 2.0f}, textSize * 1.1f,
                      h > 0.5f ? accent : palette::kUiDim, Align::Right);
        }
    }
}

} // namespace gb::ui
