// Deliberately does not include raylib.h: windows.h defines names
// (CloseWindow, ShowCursor, Rectangle...) that clash with raylib's API.
#include "platform/Rumble.h"

#include <algorithm>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xinput.h>
#endif

namespace gb {

namespace {

#if defined(_WIN32)
using XInputSetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

XInputSetStateFn ResolveXInput() {
    static const XInputSetStateFn fn = []() -> XInputSetStateFn {
        for (const char* dll : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"}) {
            if (HMODULE module = LoadLibraryA(dll)) {
                if (FARPROC proc = GetProcAddress(module, "XInputSetState")) {
                    // Going through void(*)() is the sanctioned way to convert
                    // between unrelated function pointer types.
                    return reinterpret_cast<XInputSetStateFn>(reinterpret_cast<void (*)()>(proc));
                }
            }
        }
        return nullptr;
    }();
    return fn;
}

void SetMotors(int gamepad, float low, float high) {
    const XInputSetStateFn setState = ResolveXInput();
    if (setState == nullptr || gamepad < 0 || gamepad > 3) {
        return;
    }
    XINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(low, 0.0f, 1.0f) * 65535.0f);
    vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(high, 0.0f, 1.0f) * 65535.0f);
    setState(static_cast<DWORD>(gamepad), &vibration);
}
#else
void SetMotors(int, float, float) {}
#endif

} // namespace

void Rumble::Pulse(float lowFrequency, float highFrequency, float duration) {
    low_ = std::max(low_, lowFrequency);
    high_ = std::max(high_, highFrequency);
    remaining_ = std::max(remaining_, duration);
}

void Rumble::Update(float dt, int gamepad, bool enabled) {
    if (!enabled || gamepad < 0) {
        if (active_) {
            Stop(std::max(gamepad, 0));
        }
        low_ = high_ = remaining_ = 0.0f;
        return;
    }
    if (remaining_ > 0.0f) {
        SetMotors(gamepad, low_, high_);
        active_ = true;
        remaining_ -= dt;
        // Fade out so stacked pulses decay naturally.
        low_ *= 0.92f;
        high_ *= 0.92f;
    } else if (active_) {
        Stop(gamepad);
    }
}

void Rumble::Stop(int gamepad) {
    SetMotors(gamepad, 0.0f, 0.0f);
    low_ = high_ = remaining_ = 0.0f;
    active_ = false;
}

} // namespace gb
