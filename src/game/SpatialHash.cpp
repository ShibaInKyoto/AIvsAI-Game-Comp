#include "game/SpatialHash.h"

namespace gb {

void SpatialHash::Configure(Rectangle bounds, float cellSize) {
    // Pad the grid so entities slightly outside the arena still hash sensibly.
    const float pad = cellSize * 2.0f;
    bounds_ = {bounds.x - pad, bounds.y - pad, bounds.width + pad * 2.0f, bounds.height + pad * 2.0f};
    inverseCell_ = 1.0f / cellSize;
    cols_ = std::max(1, static_cast<int>(std::ceil(bounds_.width * inverseCell_)));
    rows_ = std::max(1, static_cast<int>(std::ceil(bounds_.height * inverseCell_)));
}

} // namespace gb
