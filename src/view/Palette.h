#pragma once

#include <raylib.h>

// Shared colour language. Player-side things are warm white/yellow, pickups
// are green/blue/violet, danger is red/magenta/orange.
namespace gb::palette {

inline constexpr Color kPlayerBullet{255, 236, 150, 255};
inline constexpr Color kPlayerBulletCrit{255, 170, 90, 255};
inline constexpr Color kXpSmall{80, 255, 160, 255};
inline constexpr Color kXpMedium{70, 190, 255, 255};
inline constexpr Color kXpLarge{210, 110, 255, 255};
inline constexpr Color kHealth{255, 90, 130, 255};
inline constexpr Color kCache{255, 200, 70, 255};
inline constexpr Color kMagnet{110, 180, 255, 255};
inline constexpr Color kElite{255, 205, 70, 255};
inline constexpr Color kShield{120, 200, 255, 255};
inline constexpr Color kDanger{255, 70, 90, 255};

inline constexpr Color kUiText{225, 236, 255, 255};
inline constexpr Color kUiDim{135, 152, 196, 255};
inline constexpr Color kUiFaint{80, 95, 140, 255};
inline constexpr Color kUiAccent{90, 225, 255, 255};
inline constexpr Color kUiPanel{8, 12, 28, 225};
inline constexpr Color kUiPanelLight{18, 26, 54, 235};
inline constexpr Color kUiBorder{60, 95, 170, 255};
inline constexpr Color kUiGood{110, 255, 160, 255};
inline constexpr Color kUiWarn{255, 200, 80, 255};

} // namespace gb::palette
