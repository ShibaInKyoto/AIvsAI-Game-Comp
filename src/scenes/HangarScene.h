#pragma once

#include "core/Scene.h"
#include "game/World.h"
#include "ui/Widgets.h"
#include "view/MenuBackdrop.h"

namespace gb {

// Pre-run setup: pick a ship (or unlock one), difficulty and mode, and spend
// credits on permanent firmware upgrades.
class HangarScene final : public Scene {
public:
    // openFirmware: start on the firmware shop instead of the launch list.
    HangarScene(App& app, bool openFirmware);

    void OnEnter() override;
    void Update(float dt) override;
    void DrawWorld() override;
    void DrawUi() override;

private:
    void RefreshMenu();
    void Launch();
    void UpdateFirmware();
    void DrawShipDetails(Rectangle area);
    void DrawFirmware();

    MenuBackdrop backdrop_;
    ui::Menu menu_;
    ui::Menu firmwareMenu_;
    bool firmwareOpen_ = false;
    ShipId ship_ = ShipId::Vector;
    ShipId previewShip_ = ShipId::Vector;
    Difficulty difficulty_ = Difficulty::Standard;
    GameMode mode_ = GameMode::Campaign;
    float time_ = 0.0f;
    float denyFlash_ = 0.0f;
    float purchaseFlash_ = 0.0f;
};

} // namespace gb
