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
- `src/core`     — app loop, scene base, input, settings, storage, math, RNG, embedded assets
- `src/render`   — neon batcher, HDR post FX, warp grid, particles, camera, text
- `src/audio`    — sound-effect synthesiser, real-time music engine, audio system
- `src/game`     — simulation: world, enemies, bosses, modules, upgrades, ships,
                   director, challenges, profile, bot
- `src/view`     — presentation of the simulation: renderer, effects, HUD, shapes, backdrop
- `src/ui`       — widgets, upgrade draft, settings panel, codex
- `src/scenes`   — boot, title, hangar, game, results
- `src/platform` — OS-specific bits (XInput rumble)
- `tests`        — doctest unit tests (simulation runs headless)
- `tools`        — balance simulator, audio dump

## Status (living section)

Implemented:
- Full loop: boot → title (attract demo) → hangar → run → results; pause/settings.
- 6 ships, 39 upgrades + 7 evolutions, 14 enemies, 5 bosses, 5 sector modifiers.
- Campaign / Endless / Daily, 3 difficulties, credits + firmware + 16 challenges.
- Codex of hostiles and upgrades, decoded as they are met or drafted.
- Procedural SFX + adaptive music; XInput rumble on Windows.
- Render scale (auto caps 4K), F3 perf overlay, auto-pause on focus loss.
- CI: MSVC + GCC with -Werror, unit/sim tests, smoke tests (Linux Xvfb,
  Windows Mesa llvmpipe), dependency check, rolling nightly release.

Balance targets (tools/BalanceSim, bot skill = reaction speed):
- Standard @ skill 0.45 ≈ 50–60 % wins; Relaxed @ 0.2 ≈ 90 %;
  Overclocked @ 0.6 ≈ 30–50 %, punishing for weak-start ships.

Testing helpers:
- `--scene title|game|draft|evolution|pause|hangar|firmware|results|records|codex`
- `--sector N --boss`, `--autotest S`, `--screenshot P --screenshot-at S`, `--perf`
- xdotool scripts under Xvfb drive real key presses through the menus.

Remaining ideas / TODO:
- Final README screenshot refresh; tag v1.0.0 release at the end.
- Optional: more boss attack variety, extra sector events.
