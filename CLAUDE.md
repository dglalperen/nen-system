# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

Requires `raylib` installed (e.g. `brew install raylib` on macOS).

```bash
# Configure + build (debug with sanitizers)
cmake --preset dev
cmake --build --preset dev

# Run
./build/dev/nen_world

# Release build
cmake --preset release
cmake --build --preset release

# Build with clang-tidy enabled
cmake --build --preset dev-lint
```

## Tests

Tests are a single binary built with CTest. No external test framework — uses a simple `Expect()` helper in `tests/test_training.cpp`.

```bash
# Build only the test target
cmake --build --preset dev --target nen_tests

# Run tests via CTest
cd build/dev && ctest --output-on-failure
```

## Formatting & Linting

```bash
# Format a file
clang-format -i src/nen/affinity.cpp

# Run clang-tidy manually (or use the dev-lint preset)
clang-tidy src/nen/affinity.cpp
```

Style: LLVM base, 4-space indent, 100-column limit (`.clang-format`). Includes sorted case-sensitively.

clang-tidy checks: `bugprone-*`, `performance-*`, `readability-*`, `modernize-*` (minus trailing return types and magic numbers). Headers filtered to `include/`, `src/`, `tests/`.

## Architecture

Three CMake targets with strict layering:

- **`nen_core`** (`include/nen/`, `src/nen/`) — pure domain logic, no renderer dependency. Contains:
  - `types`: `nen::Type` enum (6 types on a ring), `Character` struct
  - `affinity`: efficiency % by ring distance (100/80/60/40)
  - `combat`: damage modifiers, aura consumption
  - `quiz`: personality quiz → type determination + water divination reveal text
  - `hatsu`: deterministic name/potency generation, per-type ability descriptions
  - `training`: starter training plan ordered by ring adjacency

- **`nen_game`** (`include/game/`, `src/game/`) — links `nen_core` + `raylib`. Contains:
  - `game`: main loop, screen state machine (`MainMenu → NameEntry/LoadCharacter → Quiz → Reveal → World`), 2D+3D rendering, animation state machine (`Idle/Move/Charge/CastBase/CastHatsu`), queued action system with cast-release timing
  - `attack_system`: animated attack lifecycle
  - `persistence`: character save/load

- **`nen_world`** — thin executable (`src/main.cpp`) calling `game::Run()`.

## Key Domain Rules

- Nen types sit on a ring: `Enhancer → Transmuter → Emitter → Conjurer → Manipulator → Specialist`. Affinity scales by ring distance.
- Combat damage uses the same ring distance as affinity.
- Hatsu name and potency are deterministic from character name (same input → same output).
- Manipulator type applies control effects (manipulated + vulnerable states) on attack.
- Aura is a resource consumed by attacks and recharged by holding a key.

## Planned Architecture (not yet built)

See `docs/game-design/hatsu-runtime-architecture.md` for the phased `HatsuSpec → HatsuRuntime` pipeline using Strategy/Composite/Decorator/Factory/Observer patterns. Currently at Phase 1 (animation state machine + queued cast timing).
