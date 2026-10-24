#pragma once

#include "ui/Widgets.h"

namespace gb {

class App;

// Settings editor shared by the title screen and the pause menu. Changes
// apply immediately and are saved when the panel closes.
class SettingsPanel {
public:
    void Open(App& app);
    bool Active() const { return active_; }
    // Returns true when the panel closed this frame.
    bool Update(App& app);
    void Draw(App& app);

private:
    enum Item : int {
        MasterVolume,
        MusicVolume,
        SfxVolume,
        Fullscreen,
        VSync,
        ScreenShake,
        Bloom,
        Aberration,
        FilmGrain,
        Scanlines,
        ReduceFlashing,
        DamageNumbers,
        AutoFire,
        AimAssist,
        ShowFps,
        Back,
        ItemCount
    };

    void Refresh(App& app);
    void Adjust(App& app, int item, int direction);

    ui::Menu menu_;
    bool active_ = false;
};

} // namespace gb
