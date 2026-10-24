#pragma once

#include "render/PostFx.h"

namespace gb {

class App;

// A self-contained screen (title, game, settings...). The App owns exactly one
// active scene and cross-fades between them.
class Scene {
public:
    explicit Scene(App& app) : app_(app) {}
    virtual ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    virtual void OnEnter() {}
    virtual void Update(float dt) = 0;
    // Glowing world layer, rendered into the HDR buffer and bloomed.
    virtual void DrawWorld() {}
    // Crisp overlay in virtual UI coordinates (1080 units tall).
    virtual void DrawUi() {}
    // Lets a scene tune post-processing (flashes, aberration...).
    virtual void ConfigurePostFx(PostFxParams& /*params*/) const {}

protected:
    App& app_;
};

} // namespace gb
