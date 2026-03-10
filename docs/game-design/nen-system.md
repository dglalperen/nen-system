# Nen System Notes (Prototype)

## Scope

Prototype a simplified Nen training sandbox that supports experimentation and future extension.

## Current Rules

- Character has:
  - `naturalType`
  - `auraPool`
  - `hatsuName`
  - `hatsuPotency`
- New characters are assigned a natural type using a personality quiz.
- Reveal uses a water divination scene tied to the resulting type.
- Affinity efficiency by ring distance:
  - same: 100%
  - adjacent: 80%
  - two steps away: 60%
  - opposite: 40%
- Aura is recovered by channeling (hold recharge action).
- Base attacks can be cast as any Nen type (player-selected).
- Combat uses a type modifier:
  - same type attacks are strongest
  - farther types are progressively weaker
- Each type has a specific base attack behavior and visual identity.
- Each character has a deterministic unique hatsu name and potency.
- Hatsu has an explicit ability name and description in UI.
- Manipulator attacks can apply control effects (manipulated and vulnerable states).

## Why This Simplification

Canonical Nen is rich and nuanced. The prototype uses a compact model to:

- keep balancing understandable
- allow rapid iteration and testing
- create a stable base for later hatsu systems

## Planned Extension

- Ability interfaces (`INenAbility`)
- Ability constraints/vows (`Decorator`)
- Ability creation from external data (`Factory`)
