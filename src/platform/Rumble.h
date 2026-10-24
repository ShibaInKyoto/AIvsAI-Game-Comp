#pragma once

namespace gb {

// Gamepad force feedback. raylib's GLFW backend has no vibration support, so
// on Windows this talks to XInput directly (loaded at runtime, so a missing
// DLL simply disables rumble). Elsewhere it is a no-op.
class Rumble {
public:
    // Requests a vibration; overlapping pulses keep the strongest values.
    void Pulse(float lowFrequency, float highFrequency, float duration);
    void Update(float dt, int gamepad, bool enabled);
    void Stop(int gamepad);

private:
    float low_ = 0.0f;
    float high_ = 0.0f;
    float remaining_ = 0.0f;
    bool active_ = false;
};

} // namespace gb
