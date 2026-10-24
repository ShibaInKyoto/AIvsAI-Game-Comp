#pragma once

#include "core/Scene.h"
#include "view/MenuBackdrop.h"

namespace gb {

// Short, skippable start-up sequence: a terminal "breach" log types itself
// out, then the logo flashes in and hands over to the title screen.
class BootScene final : public Scene {
public:
    explicit BootScene(App& app);

    void OnEnter() override;
    void Update(float dt) override;
    void DrawWorld() override;
    void DrawUi() override;
    void ConfigurePostFx(PostFxParams& params) const override;

private:
    void Finish();

    MenuBackdrop backdrop_;
    float time_ = 0.0f;
    bool flashed_ = false;
    bool finished_ = false;
};

} // namespace gb
