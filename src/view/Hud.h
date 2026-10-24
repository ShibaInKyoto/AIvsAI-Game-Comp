#pragma once

#include <raylib.h>

#include <deque>
#include <string>

namespace gb {

class App;
class GameView;

// In-game heads-up display: integrity, score/combo, sector progress or boss
// health, experience, nova gauge, module loadout, banners and off-screen
// threat indicators. Drawn in virtual UI coordinates.
class Hud {
public:
    void Update(const GameView& view, float dt);
    void Draw(App& app, const GameView& view);
    // Queues a notification banner (challenges, unlocks).
    void Toast(std::string title, std::string detail, Color color);

private:
    void DrawVitals(App& app, const GameView& view) const;
    void DrawScore(App& app, const GameView& view) const;
    void DrawProgress(App& app, const GameView& view) const;
    void DrawExperience(App& app, const GameView& view) const;
    void DrawNova(App& app, const GameView& view) const;
    void DrawModules(App& app, const GameView& view) const;
    void DrawBanners(App& app, const GameView& view) const;
    void DrawIndicators(App& app, const GameView& view) const;
    void DrawFloatingText(App& app, const GameView& view) const;
    void DrawToasts(App& app) const;

    struct ToastMessage {
        std::string title;
        std::string detail;
        Color color;
        float time = 0.0f;
    };
    std::deque<ToastMessage> toasts_;

    float displayedScore_ = 0.0f;
    float xpFill_ = 0.0f;
    float bossFill_ = 1.0f;
    float bossLag_ = 1.0f;
    float hpFlash_ = 0.0f;
    int lastHp_ = -1;
    int lastLevel_ = 1;
    float levelFlash_ = 0.0f;
    int lastMultiplier_ = 1;
    float multiplierPop_ = 0.0f;
    float time_ = 0.0f;
};

} // namespace gb
