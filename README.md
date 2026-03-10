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

- Main menu: create new character or load existing save
- New character flow: enter name -> answer personality quiz -> water divination reveal
- Keyboard + mouse navigation for menu and quiz options
- `WASD` or arrow keys: move in world
- `SPACE` or left mouse button: cast base Nen attack
- `Q` or right mouse button: cast your character's unique hatsu
- Hold `R`: recharge aura with channeling animation
- `F5`: manual save
- `ESC`: back to menu (or quit from main menu)

Saved characters are stored in `~/.nen_world/characters`.

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

- interactive 2D world with state-driven UI flow
- character create/load persistence
- personality-based Nen type assignment + water divination reveal
- unique hatsu identity per character (name + potency)
- animated type-specific base attacks + hatsu attacks
- manipulation/vulnerability combat effects (Manipulator-type control fantasy)
- aura economy loop (spend aura in combat, recover by channeling)

Next milestone is Tier 2:

- composable hatsu system (`Strategy`, `Decorator`, `Factory`)
- data-driven ability definitions
