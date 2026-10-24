#include "render/ShaderLibrary.h"

#include "core/EmbeddedAssets.h"

#include <rlgl.h>

namespace gb::shaders {

Shader Load(const char* vertexAsset, const char* fragmentAsset) {
    const char* vs = vertexAsset != nullptr ? assets::FindText(vertexAsset) : nullptr;
    const char* fs = fragmentAsset != nullptr ? assets::FindText(fragmentAsset) : nullptr;
    if ((vertexAsset != nullptr && vs == nullptr) || (fragmentAsset != nullptr && fs == nullptr)) {
        TraceLog(LOG_ERROR, "SHADER: embedded asset missing (%s, %s)", vertexAsset ? vertexAsset : "default",
                 fragmentAsset ? fragmentAsset : "default");
        return Shader{};
    }
    Shader shader = LoadShaderFromMemory(vs, fs);
    // raylib falls back to its default shader on compile failure; treat that as an error.
    if (shader.id == 0 || shader.id == rlGetShaderIdDefault()) {
        TraceLog(LOG_ERROR, "SHADER: failed to compile %s", fragmentAsset ? fragmentAsset : vertexAsset);
        return Shader{};
    }
    return shader;
}

} // namespace gb::shaders
