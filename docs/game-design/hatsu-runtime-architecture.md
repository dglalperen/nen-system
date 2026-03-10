# Hatsu Runtime Architecture (Phase Plan)

## Goals

- Let each player create a unique hatsu without hardcoding every ability.
- Keep combat deterministic and testable.
- Separate visual animation from combat simulation.
- Support incremental complexity: templates first, full custom graph later.

## Bounded Contexts

1. `Avatar/Animation`
- Character model loading, animation clips, state machine.
- States: `Idle`, `Move`, `Charge`, `CastBase`, `CastHatsu`, `Hit`.
- Emits gameplay events (cast release frame, hit-react frame).

2. `Combat Simulation`
- Aura economy, cooldowns, attack lifecycle, collision/outcome.
- Deterministic step update (`dt`-driven, no rendering dependencies).
- Accepts commands (`QueueBase`, `QueueHatsu`) and produces outcomes.

3. `Hatsu Authoring`
- Player-defined hatsu specification.
- Validation + balancing (cost budgets + vows/restrictions).
- Compiles spec into runtime-executable effect graph.

4. `Persistence`
- Stores character profile, hatsu specs, and versioned schema.
- Handles migration between spec versions.

## Core Data Model (future)

- `HatsuSpec`
  - `identity`: id, name, owner
  - `nen_profile`: primary/secondary types, affinity constraints
  - `activation`: trigger + cast time + cooldown + aura cost
  - `vows`: constraints that increase power budget
  - `effects`: graph nodes (projectile, zone, tether, buff/debuff, summon)
  - `scaling`: formulas by potency, affinity, and vows

- `HatsuRuntime`
  - Compiled executable structure from `HatsuSpec`.
  - Immutable during battle for deterministic behavior.

## Execution Pipeline

1. `QueueCastCommand` from input/UI.
2. `AnimationStateMachine` starts cast animation.
3. Animation notify/event reaches release frame.
4. `Combat` executes compiled hatsu runtime.
5. Effects update and emit `AttackOutcome`.
6. `UI/VFX` renders events and current state.

## Design Patterns

- `Strategy`: effect executors by effect type.
- `Composite`: hatsu as a graph of effect nodes.
- `Decorator`: vows/restrictions modifying budget/output.
- `Factory`: compile validated `HatsuSpec` into runtime nodes.
- `State`: avatar animation and action queue.
- `Observer/EventBus`: animation notifies and combat events.

## Balancing Model

- Each hatsu gets a power budget from:
  - base potency
  - affinity fit
  - vows/risk multipliers
- Effects consume budget; invalid specs are rejected at authoring time.
- Runtime never re-balances; it only executes validated specs.

## Incremental Delivery

1. Phase 1 (current): model animation state machine + queued cast release timing.
2. Phase 2: data-driven hatsu templates (`json`) with parameters.
3. Phase 3: constrained hatsu builder UI (preset nodes + vows).
4. Phase 4: full custom graph + save/version migration.

## Testing Strategy

- Unit tests: affinity math, budget validation, effect execution.
- Integration tests: queued action -> release frame -> outcome.
- Golden tests: hatsu specs compiled into stable runtime snapshots.
