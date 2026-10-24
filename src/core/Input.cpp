#include "core/Input.h"

#include <initializer_list>

namespace gb {

namespace {

constexpr float kStickDeadzone = 0.22f;
constexpr float kTriggerThreshold = 0.35f;
constexpr float kRepeatDelay = 0.38f;
constexpr float kRepeatInterval = 0.085f;

bool AnyKeyDown(std::initializer_list<int> keys) {
    for (int key : keys) {
        if (IsKeyDown(key)) {
            return true;
        }
    }
    return false;
}

// Radial deadzone with rescaling so motion starts smoothly at the edge.
Vector2 ApplyDeadzone(Vector2 v) {
    const float len = Length(v);
    if (len < kStickDeadzone) {
        return {0.0f, 0.0f};
    }
    const float scaled = std::min(1.0f, (len - kStickDeadzone) / (1.0f - kStickDeadzone));
    return v * (scaled / len);
}

bool PadDown(int pad, int button) { return pad >= 0 && IsGamepadButtonDown(pad, button); }

float PadAxis(int pad, int axis) { return pad >= 0 ? GetGamepadAxisMovement(pad, axis) : 0.0f; }

} // namespace

void Input::PollGamepad() {
    gamepad_ = -1;
    for (int i = 0; i < 4; ++i) {
        if (IsGamepadAvailable(i)) {
            gamepad_ = i;
            break;
        }
    }
    if (gamepad_ < 0) {
        leftStick_ = aimStick_ = {};
        return;
    }
    leftStick_ = ApplyDeadzone({PadAxis(gamepad_, GAMEPAD_AXIS_LEFT_X), PadAxis(gamepad_, GAMEPAD_AXIS_LEFT_Y)});
    aimStick_ = ApplyDeadzone({PadAxis(gamepad_, GAMEPAD_AXIS_RIGHT_X), PadAxis(gamepad_, GAMEPAD_AXIS_RIGHT_Y)});
}

bool Input::RawDown(Action action) const {
    const int pad = gamepad_;
    // Triggers report -1 at rest on most drivers, +1 fully pressed.
    const bool leftTrigger = pad >= 0 && PadAxis(pad, GAMEPAD_AXIS_LEFT_TRIGGER) > kTriggerThreshold;
    const bool rightTrigger = pad >= 0 && PadAxis(pad, GAMEPAD_AXIS_RIGHT_TRIGGER) > kTriggerThreshold;

    switch (action) {
    case Action::Fire:
        return IsMouseButtonDown(MOUSE_BUTTON_LEFT) || rightTrigger || PadDown(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
    case Action::Dash:
        return AnyKeyDown({KEY_SPACE, KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT}) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
               leftTrigger || PadDown(pad, GAMEPAD_BUTTON_LEFT_TRIGGER_2) ||
               PadDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    case Action::Special:
        return AnyKeyDown({KEY_Q, KEY_E, KEY_F}) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
               PadDown(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) || PadDown(pad, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ||
               PadDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    case Action::Pause:
        return AnyKeyDown({KEY_ESCAPE, KEY_P}) || PadDown(pad, GAMEPAD_BUTTON_MIDDLE_RIGHT);
    case Action::Confirm:
        return AnyKeyDown({KEY_ENTER, KEY_KP_ENTER, KEY_SPACE}) || PadDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    case Action::Back:
        return AnyKeyDown({KEY_ESCAPE, KEY_BACKSPACE}) || PadDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    case Action::Up:
        return AnyKeyDown({KEY_UP, KEY_W}) || PadDown(pad, GAMEPAD_BUTTON_LEFT_FACE_UP) || leftStick_.y < -0.5f;
    case Action::Down:
        return AnyKeyDown({KEY_DOWN, KEY_S}) || PadDown(pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN) || leftStick_.y > 0.5f;
    case Action::Left:
        return AnyKeyDown({KEY_LEFT, KEY_A}) || PadDown(pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || leftStick_.x < -0.5f;
    case Action::Right:
        return AnyKeyDown({KEY_RIGHT, KEY_D}) || PadDown(pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || leftStick_.x > 0.5f;
    case Action::TabPrev:
        return AnyKeyDown({KEY_Q, KEY_PAGE_UP}) || PadDown(pad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    case Action::TabNext:
        return AnyKeyDown({KEY_E, KEY_PAGE_DOWN, KEY_TAB}) || PadDown(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    case Action::Alt:
        return AnyKeyDown({KEY_R}) || PadDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_UP);
    case Action::Count:
        break;
    }
    return false;
}

void Input::Update(float dt) {
    PollGamepad();

    const Vector2 mouse = GetMousePosition();
    mouseDelta_ = mouse - mouse_;
    mouse_ = mouse;
    wheel_ = GetMouseWheelMove();

    // Keyboard movement, normalised so diagonals are not faster.
    Vector2 keys{};
    if (AnyKeyDown({KEY_A, KEY_LEFT})) keys.x -= 1.0f;
    if (AnyKeyDown({KEY_D, KEY_RIGHT})) keys.x += 1.0f;
    if (AnyKeyDown({KEY_W, KEY_UP})) keys.y -= 1.0f;
    if (AnyKeyDown({KEY_S, KEY_DOWN})) keys.y += 1.0f;
    keys = LengthSq(keys) > 0.0f ? Normalized(keys) : keys;

    const bool padActive = LengthSq(leftStick_) > 0.0f || LengthSq(aimStick_) > 0.0f ||
                           (gamepad_ >= 0 && GetGamepadButtonPressed() != GAMEPAD_BUTTON_UNKNOWN);
    const bool kbmActive = LengthSq(keys) > 0.0f || MouseMoved() || GetKeyPressed() != 0 ||
                           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (padActive) {
        lastDevice_ = InputDevice::Gamepad;
    } else if (kbmActive) {
        lastDevice_ = InputDevice::KeyboardMouse;
    }

    move_ = LengthSq(leftStick_) > LengthSq(keys) ? leftStick_ : keys;

    bool anyDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    for (std::size_t i = 0; i < kCount; ++i) {
        const bool raw = RawDown(static_cast<Action>(i));
        anyDown = anyDown || raw;
        const bool now = raw && !blocked_;
        pressed_[i] = now && !down_[i];
        down_[i] = now;

        repeated_[i] = pressed_[i];
        if (now) {
            heldTime_[i] += dt;
            if (heldTime_[i] > kRepeatDelay) {
                repeatTimer_[i] -= dt;
                if (repeatTimer_[i] <= 0.0f) {
                    repeated_[i] = true;
                    repeatTimer_[i] = kRepeatInterval;
                }
            }
        } else {
            heldTime_[i] = 0.0f;
            repeatTimer_[i] = 0.0f;
        }
    }

    mouseDown_ = !blocked_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    mouseClicked_ = !blocked_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (blocked_ && !anyDown) {
        blocked_ = false;
    }
}

void Input::ConsumeAll() {
    blocked_ = true;
    pressed_.fill(false);
    repeated_.fill(false);
    down_.fill(false);
    mouseClicked_ = false;
    mouseDown_ = false;
}

} // namespace gb
