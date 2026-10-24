#pragma once

#include "game/Entities.h"
#include "game/Ships.h"
#include "game/Upgrades.h"

#include <raylib.h>

#include <span>

namespace gb {

class NeonRenderer;

namespace shapes {

// Outline of a ship in local space (nose along +x, roughly unit radius).
std::span<const Vector2> ShipOutline(ShipId ship);

void DrawShip(NeonRenderer& neon, ShipId ship, Vector2 position, float angle, float scale, Color color,
              float alpha = 1.0f, float thickness = 3.0f);

// Draws an enemy body (no telegraph / elite decorations).
void DrawEnemyBody(NeonRenderer& neon, const Enemy& enemy, Color color, float time);

// Small glyph for an upgrade (cards, HUD build list).
void DrawUpgradeIcon(NeonRenderer& neon, UpgradeId id, Vector2 center, float size, Color color, float time);

// Generic enemy preview used by menus (codex/attract screens).
void DrawEnemyPreview(NeonRenderer& neon, EnemyKind kind, Vector2 center, float scale, float time);

} // namespace shapes
} // namespace gb
