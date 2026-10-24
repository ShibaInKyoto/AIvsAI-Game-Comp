#pragma once

#include "game/Types.h"
#include "game/Upgrades.h"

#include <raylib.h>

#include <vector>

namespace gb {

class App;

// Compendium of hostiles and upgrades, plus a short how-to-play guide.
// Hostile and upgrade entries decode once they have been met or drafted in
// any run; the rest stay redacted.
class CodexPanel {
public:
    void Open(App& app);
    bool Active() const { return active_; }
    // Returns true when the panel closed this frame.
    bool Update(App& app, float dt);
    void Draw(App& app);

private:
    enum class Tab {
        Hostiles,
        Upgrades,
        Guide
    };
    static constexpr int kTabCount = 3;

    struct Layout {
        Rectangle panel;
        Rectangle detail;
        Vector2 gridOrigin;
        float tile;
        float gap;
        int columns;
    };

    Layout ComputeLayout(const App& app) const;
    Rectangle TabRect(const Layout& layout, Tab tab) const;
    Rectangle TileRect(const Layout& layout, int index) const;
    int EntryCount() const;
    bool Discovered(const App& app, int index) const;
    void SwitchTab(App& app, Tab tab);

    void DrawTile(App& app, const Layout& layout, int index);
    void DrawEnemyDetail(App& app, const Layout& layout, EnemyKind kind, bool known);
    void DrawUpgradeDetail(App& app, const Layout& layout, UpgradeId id, bool known);
    void DrawGuideDetail(App& app, const Layout& layout, int index);

    std::vector<EnemyKind> enemies_;
    std::vector<UpgradeId> upgrades_;
    Tab tab_ = Tab::Hostiles;
    int selected_ = 0;
    float time_ = 0.0f;
    float selectPulse_ = 0.0f;
    bool active_ = false;
};

} // namespace gb
