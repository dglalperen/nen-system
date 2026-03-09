# Nen World C++ Game Prototype

This repository is a learning-first C++ project for building a Nen sandbox game with strong architecture and code quality.

## Stack

- C++20
- CMake + Ninja
- raylib (2D runtime)
- CTest (unit tests)
- clang-format + clang-tidy

## Prerequisites (macOS)

```bash
xcode-select --install
brew install cmake ninja raylib
```

Verify:

```bash
clang++ --version
cmake --version
ninja --version
```

## Build and Run

```bash
cmake --preset dev
cmake --build --preset dev
./build/dev/nen_world
```

Controls:

- `WASD` or arrow keys: move
- `E`: train in active zone
- `1-6`: switch natural Nen type
- `ESC`: quit

## Tests

```bash
cmake --build --preset tests
ctest --preset dev
```

## Quality Commands

Format:

```bash
rg --files include src tests | rg '\.(hpp|cpp)$' | xargs xcrun clang-format -i
```

Tidy:

```bash
brew install llvm
cmake --preset dev-lint
cmake --build --preset dev-lint
```

## Build Profiles

- `dev`: Debug + sanitizers
- `release`: optimized, sanitizers off
- `dev-no-sanitizers`: debug without ASan/UBSan
- `dev-lint`: debug + sanitizers + clang-tidy during build

## Project Structure

- `include/nen`, `src/nen`: Nen domain logic
- `include/game`, `src/game`: raylib runtime/game loop
- `src/main.cpp`: entry point
- `tests`: domain tests
- `docs/architecture.md`: architecture overview
- `docs/adr`: architecture decisions
- `docs/game-design`: gameplay design notes

## Current Complexity Level

This is currently a Tier 1 prototype:

- interactive 2D world
- Nen affinity mechanics
- training loop
- basic HUD

Next milestone is Tier 2:

- composable hatsu system (`Strategy`, `Decorator`, `Factory`)
- data-driven ability definitions
