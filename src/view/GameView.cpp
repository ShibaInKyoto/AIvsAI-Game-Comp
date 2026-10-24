#include "view/GameView.h"

#include "core/App.h"

namespace gb {

namespace {

constexpr float kViewHeight = 1020.0f;
constexpr float kGridSpacing = 42.0f;

Color DampColor(Color current, Color target, float dt) {
    const float t = 1.0f - std::exp2(-dt / 0.6f);
    return LerpColor(current, target, t);
}

} // namespace

GameView::GameView(App& app, const RunConfig& config)
    : app_(app), world_(std::make_unique<World>(config)),
      effects_(particles_, grid_, camera_, app.Audio(), app.Haptics(), app.Prefs()) {
    grid_.Build(World::kArena, kGridSpacing);
    camera_.SetViewHeight(kViewHeight);
    camera_.SetBounds(World::kArena, 140.0f);
    camera_.SnapTo(world_->GetPlayer().position);
    gridColor_ = world_->GetDirector().Info().gridColor;
    motif_ = world_->GetDirector().Sector() % Director::kCampaignSectors;
    accent_ = world_->GetDirector().Info().accentColor;
}

void GameView::Update(float dt, const PlayerInput& input, bool simulate) {
    const double start = GetTime();
    time_ += dt;
    const float simDt = dt * effects_.TimeScale();
    if (simulate) {
        world_->Update(simDt, input);
        effects_.Process(*world_, world_->Events());
        world_->Events().clear();
    }
    effects_.Update(*world_, simulate ? dt : 0.0f);
    renderer_.Update(*world_, simulate ? simDt : 0.0f);
    grid_.Update(simulate ? simDt : dt * 0.25f);
    particles_.Update(simulate ? simDt : 0.0f, World::kArena);

    const Director& director = world_->GetDirector();
    gridColor_ = DampColor(gridColor_, director.Info().gridColor, dt);
    accent_ = DampColor(accent_, director.Info().accentColor, dt);
    // The backdrop motif fades out, swaps to the new sector's, and fades in.
    const int motif = director.Sector() % Director::kCampaignSectors;
    if (motif != motif_) {
        motifAlpha_ -= dt * 1.5f;
        if (motifAlpha_ <= 0.0f) {
            motifAlpha_ = 0.0f;
            motif_ = motif;
        }
    } else {
        motifAlpha_ = std::min(1.0f, motifAlpha_ + dt * 0.6f);
    }

    const Player& p = world_->GetPlayer();
    const Vector2 lookAhead = FromAngle(p.aimAngle, 70.0f) + p.velocity * 0.12f;
    camera_.SetShakeScale(app_.Prefs().screenShake);
    const Vector2 scene = app_.Gfx().SceneSize();
    camera_.Update(dt, p.position, lookAhead, scene.x, scene.y);
    updateMs_ = static_cast<float>((GetTime() - start) * 1000.0);
}

Camera2D GameView::CurrentCamera() const {
    const Vector2 scene = app_.Gfx().SceneSize();
    return camera_.Get(scene.x, scene.y);
}

Vector2 GameView::ScreenToWorld(Vector2 screen) const {
    return GetScreenToWorld2D(app_.Gfx().ScreenToScene(screen), CurrentCamera());
}

Vector2 GameView::WorldToUi(Vector2 world) const {
    const Vector2 scene = GetWorldToScreen2D(world, CurrentCamera());
    return app_.Gfx().ScreenToUi(app_.Gfx().SceneToScreen(scene));
}

void GameView::DrawWorld(Vector2 reticle, bool showReticle) {
    const double start = GetTime();
    const Vector2 scene = app_.Gfx().SceneSize();
    const float w = scene.x;
    const float h = scene.y;
    BeginMode2D(CurrentCamera());
    WorldDrawContext ctx;
    ctx.visible = camera_.VisibleWorld(w, h);
    ctx.cameraCenter = camera_.Position();
    ctx.time = time_;
    ctx.gridMinor = WithAlpha(gridColor_, 0.075f);
    ctx.gridMajor = WithAlpha(gridColor_, 0.16f);
    ctx.arenaColor = accent_;
    ctx.reticle = reticle;
    ctx.showReticle = showReticle;
    ctx.motif = motif_;
    ctx.motifAlpha = motifAlpha_;
    ctx.warp = effects_.Warp();
    renderer_.Draw(app_.Gfx().Neon(), *world_, grid_, particles_, ctx);
    EndMode2D();
    drawMs_ = static_cast<float>((GetTime() - start) * 1000.0);
}

} // namespace gb
