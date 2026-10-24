#pragma once

#include <raylib.h>

namespace gb::shaders {

// Loads a shader from embedded assets. Either path may be nullptr to use
// raylib's default stage. Returns an invalid shader (id 0) on failure.
Shader Load(const char* vertexAsset, const char* fragmentAsset);

} // namespace gb::shaders
