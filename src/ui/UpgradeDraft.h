#pragma once

#include "game/Upgrades.h"

#include <raylib.h>

#include <vector>

namespace gb {

class App;
class World;

// Level-up / data-cache card picker. Pauses the run while open.
class UpgradeDraft {
public:
    void Open(App& app, World& world, bool cache);
    bool Active() const { return active_; }

    // Returns true when the draft finished this frame.
    bool Update(App& app, World& world, float dt);
    void Draw(App& app, const World& world);

private:
    void Pick(App& app, World& world, int index);
    Rectangle CardRect(const App& app, int index) const;

    std::vector<UpgradeId> offers_;
    bool active_ = false;
    bool cache_ = false;
    int selected_ = 1;
    int picked_ = -1;
    float time_ = 0.0f;
    float pickTime_ = 0.0f;
    float rerollFlash_ = 0.0f;
};

} // namespace gb
