# Architecture Overview

## Goal

Build a maintainable Nen sandbox game with clear boundaries between domain logic and runtime rendering.

## Layers

- `nen_core`:
  - Pure game-domain logic with minimal dependencies.
  - Contains Nen types, affinity calculations, and training rules.
  - Should stay reusable even if renderer/engine changes.
- `nen_game`:
  - Runtime gameplay loop and presentation bridge.
  - Uses `raylib` for input, windowing, drawing, and frame loop.
  - Calls into `nen_core` for authoritative rules.
- `nen_world`:
  - Small executable entry point.

## Current Module Map

- `include/nen`, `src/nen`: Nen domain
- `include/game`, `src/game`: 2D runtime
- `tests/`: fast unit tests for domain behaviors

## Quality Gates

- Compiler warnings: enabled (`-Wall -Wextra -Wpedantic`)
- Sanitizers in Debug: Address + Undefined Behavior
- Formatting: `.clang-format`
- Static analysis: `.clang-tidy`
- Unit tests: CTest target `nen_tests`

## Complexity Tiers

- Tier 1 (current target): 2D movement + zone training + Nen affinity mechanics
- Tier 2: Ability system with strategy/decorator patterns and data-driven abilities
- Tier 3: large-scale content simulation and advanced AI
