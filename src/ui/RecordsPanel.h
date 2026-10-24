#pragma once

#include <raylib.h>

namespace gb {

class App;

// Records screen: lifetime statistics and best scores beside either the
// challenge list or the recent run history (toggled with Q/E or left/right).
class RecordsPanel {
public:
    void Open(App& app);
    bool Active() const { return active_; }
    // Returns true when the panel closed this frame.
    bool Update(App& app);
    void Draw(App& app);

private:
    void DrawChallenges(App& app, Rectangle area);
    void DrawRecentRuns(App& app, Rectangle area);

    bool active_ = false;
    bool showRecentRuns_ = false;
};

} // namespace gb
