#pragma once

#include "core/MathUtil.h"

#include <raylib.h>

#include <array>

namespace gb {

enum class InputDevice { KeyboardMouse, Gamepad };

// Logical actions, decoupled from physical bindings.
enum class Action {
    // Gameplay
    Fire,
    Dash,
    Special,
    Pause,
    // Menus
    Confirm,
    Back,
    Up,
    Down,
    Left,
    Right,
    TabPrev,
    TabNext,
    Alt,  // secondary menu action (e.g. reroll)
    Count
};

// Polls keyboard, mouse and the first gamepad once per frame and exposes
// device-agnostic state. Tracks which device was used last so prompts and aim
// mode can adapt automatically.
class Input {
public:
    void Update(float dt);

    // Movement vector with radial deadzone, length in [0, 1].
    Vector2 Move() const { return move_; }
    // Right stick aim direction (zero when idle); mouse aim is resolved by the
    // game from MousePosition().
    Vector2 AimStick() const { return aimStick_; }
    bool AimStickActive() const { return LengthSq(aimStick_) > 0.0f; }

    bool Down(Action action) const { return down_[Index(action)]; }
    bool Pressed(Action action) const { return pressed_[Index(action)]; }
    // Like Pressed, but auto-repeats while held (menu navigation).
    bool Repeated(Action action) const { return repeated_[Index(action)]; }

    Vector2 MousePosition() const { return mouse_; }
    Vector2 MouseDelta() const { return mouseDelta_; }
    bool MouseMoved() const { return LengthSq(mouseDelta_) > 0.25f; }
    bool MouseClicked() const { return mouseClicked_; }
    bool MouseDown() const { return mouseDown_; }
    float MouseWheel() const { return wheel_; }

    InputDevice LastDevice() const { return lastDevice_; }
    bool GamepadConnected() const { return gamepad_ >= 0; }

    // Suppresses all input until every button has been released, so a press
    // that closes one screen does not leak into the next.
    void ConsumeAll();

private:
    static std::size_t Index(Action a) { return static_cast<std::size_t>(a); }
    bool RawDown(Action action) const;
    void PollGamepad();

    static constexpr std::size_t kCount = static_cast<std::size_t>(Action::Count);
    std::array<bool, kCount> down_{};
    std::array<bool, kCount> pressed_{};
    std::array<bool, kCount> repeated_{};
    std::array<float, kCount> heldTime_{};
    std::array<float, kCount> repeatTimer_{};
    bool blocked_ = false;

    Vector2 move_{};
    Vector2 aimStick_{};
    Vector2 leftStick_{};
    Vector2 mouse_{};
    Vector2 mouseDelta_{};
    bool mouseClicked_ = false;
    bool mouseDown_ = false;
    float wheel_ = 0.0f;
    int gamepad_ = -1;
    InputDevice lastDevice_ = InputDevice::KeyboardMouse;
};

} // namespace gb
