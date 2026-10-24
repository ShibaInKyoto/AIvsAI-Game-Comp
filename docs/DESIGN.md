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
- `src/ui`       — widgets, upgrade draft, settings, codex and records panels
- `src/scenes`   — boot, title, hangar, game, results
- `src/platform` — OS-specific bits (XInput rumble)
- `tests`        — doctest unit tests (simulation runs headless)
- `tools`        — balance simulator, audio dump

## Status (living section)

Implemented:
- Full loop: boot → title (attract demo) → hangar → run → results; pause/settings.
- 6 ships, 39 upgrades + 7 evolutions, 14 enemies, 5 bosses, 5 sector modifiers.
- Campaign / Endless / Boss Rush / Daily, 3 difficulties, credits + firmware + 16 challenges.
- Codex of hostiles and upgrades, decoded as they are met or drafted.
- Sector events (ambush, crossfire, data storm) every ~40 s of waves.
- Results show the run's loadout and what destroyed the ship; Records keep
  the last ten runs.
- Procedural SFX + adaptive music; XInput rumble on Windows.
- Render scale (auto caps 4K), F3 perf overlay, auto-pause on focus loss.
- CI: MSVC + GCC with -Werror, clang-format check, unit/sim tests, ASan +
  UBSan job, smoke tests (Linux Xvfb, Windows Mesa llvmpipe), dependency
  check, rolling nightly release.
- Audio: soft peak limiter on the final mix.

Balance targets (tools/BalanceSim, bot skill = reaction speed):
- Standard @ skill 0.45 ≈ 85 % wins, losses split between the Warden and
  the Architect (difficulty should rise through the run, not peak at boss 1);
  Relaxed @ 0.2 ≈ 95 %; Overclocked @ 0.6 ≈ 55 %, punishing for fragile ships.
- Boss Rush @ Standard 0.45 ≈ 70 % (a gauntlet, slightly harder than the
  campaign); credits are halved so it is not a farming shortcut.
- A run's first sector event is always a Data Storm (teaches events safely).
- Endless loops compound (enemy hp x4, spawn rate x1.4, guardian hp x3 per
  loop) and add corruption (destroyed hostiles may fire back). Overcharge
  stacks additively so builds cannot outgrow loops forever. The bot dodges
  far better than people, so endless pacing is tuned by design, not by sim.

Testing helpers:
- `--scene title|game|bossrush|draft|evolution|pause|hangar|firmware|results|records|codex`
- `--sector N --boss`, `--autotest S`, `--screenshot P --screenshot-at S`, `--perf`
- xdotool scripts under Xvfb drive real key presses through the menus.

Remaining ideas / TODO:
- Tag the v1.0.0 release at the end (push a `[release]` commit or a tag).
- Optional: more evolutions.
