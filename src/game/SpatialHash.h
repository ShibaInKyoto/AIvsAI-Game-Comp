#pragma once

#include "core/MathUtil.h"

#include <raylib.h>

#include <algorithm>
#include <vector>

namespace gb {

// Uniform grid over the arena, rebuilt every simulation step with a counting
// sort (no per-step allocations once warmed up). Items are bucketed by centre;
// queries are padded by the largest inserted radius so big bodies are found.
class SpatialHash {
public:
    void Configure(Rectangle bounds, float cellSize);

    // Rebuilds from `count` items; `getter(i, position, radius)` returns false
    // to skip an item.
    template <typename Getter>
    void Build(int count, Getter&& getter);

    // Calls fn(index) for every item whose cell overlaps the query circle.
    // Callers still do their own precise distance test.
    template <typename Fn>
    void Query(Vector2 center, float radius, Fn&& fn) const;

    int CellCount() const { return cols_ * rows_; }

private:
    int CellIndex(Vector2 p) const {
        const int cx = std::clamp(static_cast<int>((p.x - bounds_.x) * inverseCell_), 0, cols_ - 1);
        const int cy = std::clamp(static_cast<int>((p.y - bounds_.y) * inverseCell_), 0, rows_ - 1);
        return cy * cols_ + cx;
    }

    Rectangle bounds_{};
    float inverseCell_ = 1.0f / 96.0f;
    int cols_ = 1;
    int rows_ = 1;
    float maxRadius_ = 0.0f;
    std::vector<int> cellStart_;
    std::vector<int> entries_;
    std::vector<int> itemCell_;
    std::vector<int> fill_;
};

template <typename Getter>
void SpatialHash::Build(int count, Getter&& getter) {
    const int cells = cols_ * rows_;
    cellStart_.assign(static_cast<std::size_t>(cells) + 1, 0);
    itemCell_.assign(static_cast<std::size_t>(count), -1);
    maxRadius_ = 0.0f;

    for (int i = 0; i < count; ++i) {
        Vector2 position{};
        float radius = 0.0f;
        if (!getter(i, position, radius)) {
            continue;
        }
        const int cell = CellIndex(position);
        itemCell_[static_cast<std::size_t>(i)] = cell;
        cellStart_[static_cast<std::size_t>(cell) + 1]++;
        maxRadius_ = std::max(maxRadius_, radius);
    }
    for (int c = 0; c < cells; ++c) {
        cellStart_[static_cast<std::size_t>(c) + 1] += cellStart_[static_cast<std::size_t>(c)];
    }
    entries_.assign(static_cast<std::size_t>(cellStart_.back()), 0);
    fill_.assign(cellStart_.begin(), cellStart_.end() - 1);
    for (int i = 0; i < count; ++i) {
        const int cell = itemCell_[static_cast<std::size_t>(i)];
        if (cell >= 0) {
            entries_[static_cast<std::size_t>(fill_[static_cast<std::size_t>(cell)]++)] = i;
        }
    }
}

template <typename Fn>
void SpatialHash::Query(Vector2 center, float radius, Fn&& fn) const {
    if (entries_.empty()) {
        return;
    }
    const float r = radius + maxRadius_;
    const int x0 = std::clamp(static_cast<int>((center.x - r - bounds_.x) * inverseCell_), 0, cols_ - 1);
    const int y0 = std::clamp(static_cast<int>((center.y - r - bounds_.y) * inverseCell_), 0, rows_ - 1);
    const int x1 = std::clamp(static_cast<int>((center.x + r - bounds_.x) * inverseCell_), 0, cols_ - 1);
    const int y1 = std::clamp(static_cast<int>((center.y + r - bounds_.y) * inverseCell_), 0, rows_ - 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const int cell = y * cols_ + x;
            const int end = cellStart_[static_cast<std::size_t>(cell) + 1];
            for (int k = cellStart_[static_cast<std::size_t>(cell)]; k < end; ++k) {
                fn(entries_[static_cast<std::size_t>(k)]);
            }
        }
    }
}

} // namespace gb
