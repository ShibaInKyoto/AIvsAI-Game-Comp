# Gridbreaker — Design Document

## Pitch
Neon vector twin-stick roguelite. You are a rogue process breaking out of a
corrupted mainframe, sector by sector. Every kill feeds data shards; every
level-up lets you draft one of three upgrades. Five sectors, five bosses,
endless mode after that. Short runs (15–20 min), lots of build variety.

## Pillars
1. **Juice first** — every action has light, sound, shake and grid ripple.
2. **Readable chaos** — enemy spawns are telegraphed, enemy bullets are bright
   and distinct from player bullets, hits always feel fair.
3. **Build expression** — upgrades stack and combine into recognisably
   different play styles (bullet storm, orbit tank, drone swarm, lightning...).
4. **Instant replay** — death screen to new run in two button presses.

## Controls
| Action  | Keyboard/Mouse            | Gamepad            |
|---------|---------------------------|--------------------|
| Move    | WASD / arrows             | Left stick         |
| Aim     | Mouse                     | Right stick        |
| Fire    | Left mouse (or auto-fire) | Right trigger      |
| Dash    | Space / Shift / Right mouse | Left trigger / A |
| Nova    | Q / E / F / Middle mouse  | Bumpers / B        |
| Pause   | Esc / P                   | Start              |

## Run structure
- 5 sectors. Each sector: timed wave phase (director spends a growing threat
  budget), then a boss. Clearing a boss lets you pick a sector modifier for
  the next sector.
- XP shards → level-ups → pick 1 of 3 upgrades (rarity tiers). Rerolls.
- Elites drop a "cache" that grants an extra upgrade pick.
- Nova (special) charges from kills: screen-clearing shockwave.
- Score with combo multiplier; multiplier decays and resets when hit.

## Meta progression
- Credits earned per run (score / progress based).
- Unlock ships (different starting weapon/passive).
- Permanent "firmware" upgrades with a few ranks each.
- Difficulty: Relaxed / Standard / Overclocked; plus Daily run (date seed).

## Tech
- C++20, raylib 5.5, CMake, everything embedded in one exe.
- HDR scene buffer → 6-level bloom chain → tone-mapped composite.
- Neon primitives: one procedural atlas, additive batched quads.
- Simulation (`src/game`) is independent of rendering: it emits events that
  the presentation layer turns into particles, sound, shake and grid forces.
- Audio is fully procedural: sfxr-style effect synthesis at startup and a
  real-time adaptive music sequencer on the audio thread.

## Code map
- `src/core`   — app loop, scenes, input, settings, storage, math, RNG
- `src/render` — neon batcher, post FX, warp grid, particles, camera, text
- `src/audio`  — synthesiser, sound bank, music engine
- `src/game`   — simulation: world, player, enemies, bosses, upgrades, director
- `src/scenes` — title, hangar, game, settings, results
- `src/ui`     — widgets and HUD
- `tests`      — doctest unit tests (simulation runs headless)
