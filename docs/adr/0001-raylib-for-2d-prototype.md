# ADR 0001: Use Raylib for the First Playable Prototype

## Status

Accepted (2026-03-09)

## Context

The project needs a real-time game loop quickly while keeping C++ learning friction low.
The user is early in both C++ and game development, so setup complexity must be controlled.

## Decision

Use `raylib` for the first playable 2D prototype.

## Consequences

### Positive

- Fast path from domain logic to interactive window.
- Small API surface and easy build integration on macOS.
- Keeps focus on architecture and game systems, not engine internals.

### Negative

- Limited built-in editor/tooling compared to full engines.
- If moving to 3D-heavy gameplay later, some runtime code may be replaced.

## Follow-up

Re-evaluate after the first vertical slice (movement, abilities, combat loop, save/load).
