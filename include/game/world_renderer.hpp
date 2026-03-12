#pragma once

#include <raylib.h>

#include "game/app_state.hpp"
#include "nen/types.hpp"

namespace game {

Color   TypeColor   (nen::Type type);
Vector3 ArenaToWorld(Vector2 arenaPos, float y = 0.0F);
Vector2 WorldToArena(Vector3 worldPos);

void DrawArena3D        (const AppState &app);
void DrawAttackEffects3D(const AppState &app);
void DrawElasticTether3D(const AppState &app);
void DrawEnemy3D        (const AppState &app);
void DrawPlayer3D       (const AppState &app);
void DrawEnSphere       (const AppState &app);

// Calls BeginMode3D, all 3D draw functions, DrawParticleSystem, EndMode3D
void DrawWorld3D(const AppState &app);

} // namespace game
