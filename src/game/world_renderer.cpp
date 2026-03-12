#include "game/world_renderer.hpp"

#include <cmath>

#include <raylib.h>

#include "game/app_state.hpp"
#include "game/game_constants.hpp"
#include "game/particle_system.hpp"
#include "nen/nen_system.hpp"
#include "nen/types.hpp"

namespace game {

namespace {

Vector2 Normalize2D(Vector2 vector) {
    const float len = std::sqrt(vector.x * vector.x + vector.y * vector.y);
    if (len <= 0.0001F) {
        return {1.0F, 0.0F};
    }
    return {vector.x / len, vector.y / len};
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

} // namespace

Color TypeColor(nen::Type type) {
    switch (type) {
    case nen::Type::Enhancer:
        return {95, 196, 102, 255};
    case nen::Type::Transmuter:
        return {69, 184, 229, 255};
    case nen::Type::Emitter:
        return {245, 111, 76, 255};
    case nen::Type::Conjurer:
        return {152, 129, 255, 255};
    case nen::Type::Manipulator:
        return {255, 180, 70, 255};
    case nen::Type::Specialist:
        return {255, 90, 169, 255};
    }
    return RAYWHITE;
}

Vector3 ArenaToWorld(Vector2 arenaPosition, float y) {
    const float cx = kArenaBounds.x + kArenaBounds.width * 0.5F;
    const float cy = kArenaBounds.y + kArenaBounds.height * 0.5F;
    return {(arenaPosition.x - cx) * kWorldScale, y, (arenaPosition.y - cy) * kWorldScale};
}

Vector2 WorldToArena(Vector3 worldPosition) {
    const float cx = kArenaBounds.x + kArenaBounds.width * 0.5F;
    const float cy = kArenaBounds.y + kArenaBounds.height * 0.5F;
    return {worldPosition.x / kWorldScale + cx, worldPosition.z / kWorldScale + cy};
}

void DrawArena3D(const AppState & /*app*/) {
    // Large ground plane — dark stone
    DrawPlane({0.0F, 0.0F, 0.0F}, {200.0F, 200.0F}, {22, 30, 46, 255});

    // Arena boundary — glowing rectangle on the floor
    const Vector3 arenaCenter = ArenaToWorld(
        {kArenaBounds.x + kArenaBounds.width * 0.5F, kArenaBounds.y + kArenaBounds.height * 0.5F});
    const float hw     = kArenaBounds.width  * kWorldScale * 0.5F;
    const float hd     = kArenaBounds.height * kWorldScale * 0.5F;
    const float yFloor = 0.02F;
    const Color borderGlow = {72, 132, 255, 200};
    // Four edges of the arena boundary
    DrawLine3D({arenaCenter.x - hw, yFloor, arenaCenter.z - hd},
               {arenaCenter.x + hw, yFloor, arenaCenter.z - hd}, borderGlow);
    DrawLine3D({arenaCenter.x + hw, yFloor, arenaCenter.z - hd},
               {arenaCenter.x + hw, yFloor, arenaCenter.z + hd}, borderGlow);
    DrawLine3D({arenaCenter.x + hw, yFloor, arenaCenter.z + hd},
               {arenaCenter.x - hw, yFloor, arenaCenter.z + hd}, borderGlow);
    DrawLine3D({arenaCenter.x - hw, yFloor, arenaCenter.z + hd},
               {arenaCenter.x - hw, yFloor, arenaCenter.z - hd}, borderGlow);

    // Subtle floor grid inside the arena
    for (int i = 0; i <= 10; ++i) {
        const float t = static_cast<float>(i) / 10.0F;
        const float z = arenaCenter.z - hd + t * hd * 2.0F;
        DrawLine3D({arenaCenter.x - hw + 0.1F, yFloor, z},
                   {arenaCenter.x + hw - 0.1F, yFloor, z}, Fade(WHITE, 0.06F));
    }
    for (int i = 0; i <= 12; ++i) {
        const float t = static_cast<float>(i) / 12.0F;
        const float x = arenaCenter.x - hw + t * hw * 2.0F;
        DrawLine3D({x, yFloor, arenaCenter.z - hd + 0.1F},
                   {x, yFloor, arenaCenter.z + hd - 0.1F}, Fade(WHITE, 0.05F));
    }
}

void DrawAttackEffects3D(const AppState &app) {
    for (const auto &effect : app.activeAttacks) {
        const Vector3 pos    = ArenaToWorld(effect.position, 0.55F + effect.radius * 0.004F);
        const float   radius = std::max(0.08F, effect.radius * kWorldScale * 0.65F);
        Color color = TypeColor(effect.type);
        color.a = 220;

        switch (effect.type) {
        case nen::Type::Enhancer:
            DrawSphere(pos, radius * 1.15F, color);
            DrawCircle3D(pos, radius * 1.9F, {1.0F, 0.0F, 0.0F}, 90.0F, Fade(color, 0.65F));
            break;
        case nen::Type::Transmuter: {
            const Vector2 dir2D = Normalize2D(effect.velocity);
            const Vector3 tail =
                ArenaToWorld({effect.position.x - dir2D.x * 28.0F,
                              effect.position.y - dir2D.y * 28.0F},
                             pos.y - 0.04F);
            const Vector3 sideA{pos.x + std::sin(effect.phase * 5.2F) * 0.22F, pos.y + 0.18F,
                                pos.z + std::cos(effect.phase * 6.0F) * 0.17F};
            const Vector3 sideB{pos.x - std::sin(effect.phase * 5.2F) * 0.22F, pos.y - 0.15F,
                                pos.z - std::cos(effect.phase * 6.0F) * 0.17F};
            DrawLine3D(tail, pos, Fade(color, 0.95F));
            DrawLine3D(sideA, pos, WHITE);
            DrawLine3D(pos, sideB, WHITE);
            DrawSphere(pos, radius * 0.75F, color);
            DrawCircle3D(pos, radius * 1.7F, {0.0F, 0.0F, 1.0F}, 0.0F, Fade(WHITE, 0.52F));
            break;
        }
        case nen::Type::Emitter: {
            const Vector2 dir2D = Normalize2D(effect.velocity);
            const Vector3 start = ArenaToWorld(
                {effect.position.x - dir2D.x * 28.0F,
                 effect.position.y - dir2D.y * 28.0F}, pos.y);
            DrawCylinderEx(start, pos, radius * 0.45F, radius * 0.68F, 10, Fade(color, 0.95F));
            DrawSphere(pos, radius * 0.58F, {255, 244, 224, 255});
            DrawCircle3D(pos, radius * 1.2F, {1.0F, 0.0F, 0.0F}, 90.0F, Fade(color, 0.4F));
            break;
        }
        case nen::Type::Conjurer: {
            const float spin = effect.phase * 240.0F;
            DrawCubeV(pos, {radius * 1.3F, radius * 0.45F, radius * 3.2F}, Fade(color, 0.9F));
            DrawCubeWiresV(pos, {radius * 1.4F, radius * 0.48F, radius * 3.35F}, WHITE);
            const Vector3 shardA{pos.x + std::sin(effect.phase * 4.2F) * radius * 1.9F,
                                 pos.y + 0.12F,
                                 pos.z + std::cos(effect.phase * 4.2F) * radius * 1.9F};
            const Vector3 shardB{pos.x - std::sin(effect.phase * 4.2F) * radius * 1.9F,
                                 pos.y - 0.12F,
                                 pos.z - std::cos(effect.phase * 4.2F) * radius * 1.9F};
            DrawCubeV(shardA, {radius * 0.7F, radius * 0.28F, radius * 1.65F}, Fade(color, 0.8F));
            DrawCubeV(shardB, {radius * 0.7F, radius * 0.28F, radius * 1.65F}, Fade(color, 0.8F));
            DrawCircle3D(pos, radius * 1.1F, {0.0F, 1.0F, 0.0F}, spin, Fade(WHITE, 0.35F));
            break;
        }
        case nen::Type::Manipulator: {
            DrawSphere(pos, radius, color);
            const float   orbit = radius * 2.2F;
            const Vector3 orb1{pos.x + std::cos(effect.phase * 6.0F) * orbit, pos.y + 0.08F,
                               pos.z + std::sin(effect.phase * 6.0F) * orbit};
            const Vector3 orb2{pos.x + std::cos(effect.phase * 6.0F + PI) * orbit, pos.y - 0.08F,
                               pos.z + std::sin(effect.phase * 6.0F + PI) * orbit};
            DrawSphere(orb1, radius * 0.35F, Fade(color, 0.9F));
            DrawSphere(orb2, radius * 0.35F, Fade(color, 0.75F));
            break;
        }
        case nen::Type::Specialist:
            DrawSphere(pos, radius * 1.2F, color);
            DrawLine3D({pos.x - radius * 1.6F, pos.y, pos.z},
                       {pos.x + radius * 1.6F, pos.y, pos.z}, Fade(WHITE, 0.9F));
            DrawLine3D({pos.x, pos.y - radius * 1.6F, pos.z},
                       {pos.x, pos.y + radius * 1.6F, pos.z}, Fade(WHITE, 0.9F));
            DrawLine3D({pos.x, pos.y, pos.z - radius * 1.6F},
                       {pos.x, pos.y, pos.z + radius * 1.6F}, Fade(WHITE, 0.9F));
            break;
        }
    }
}

void DrawElasticTether3D(const AppState &app) {
    if (app.enemy.elasticTimer <= 0.0F) {
        return;
    }

    const Vector2 player2{
        app.playerPosition.x + app.playerSize.x * 0.5F,
        app.playerPosition.y + app.playerSize.y * 0.42F,
    };
    const Vector3 start = ArenaToWorld(player2, 1.1F);
    const Vector3 end   = ArenaToWorld(app.enemy.position, 1.08F);

    constexpr int segmentCount = 12;
    const float   phase        = static_cast<float>(GetTime()) * 12.0F;
    for (int i = 0; i < segmentCount; ++i) {
        const float t0    = static_cast<float>(i)     / static_cast<float>(segmentCount);
        const float t1    = static_cast<float>(i + 1) / static_cast<float>(segmentCount);
        const float wave0 = std::sin(t0 * 18.0F + phase) * 0.09F;
        const float wave1 = std::sin(t1 * 18.0F + phase) * 0.09F;
        const Vector3 p0{
            Lerp(start.x, end.x, t0),
            Lerp(start.y, end.y, t0) + wave0,
            Lerp(start.z, end.z, t0),
        };
        const Vector3 p1{
            Lerp(start.x, end.x, t1),
            Lerp(start.y, end.y, t1) + wave1,
            Lerp(start.z, end.z, t1),
        };
        DrawLine3D(p0, p1, {133, 228, 255, 235});
    }
}

void DrawEnemy3D(const AppState &app) {
    const EnemyState &enemy = app.enemy;
    const Vector3 center    = ArenaToWorld(enemy.position, 0.95F);
    const float   radius    = enemy.radius * kWorldScale * 0.45F;
    const Color coreColor   =
        enemy.hitFlashTimer > 0.0F ? Color{255, 236, 178, 255} : Color{216, 105, 102, 255};

    DrawSphere(center, radius, {70, 37, 37, 255});
    DrawSphere({center.x, center.y + radius * 0.25F, center.z}, radius * 0.72F, coreColor);
    DrawSphereWires(center, radius + 0.03F, 10, 10, Fade(WHITE, 0.75F));

    if (enemy.manipulatedTimer > 0.0F) {
        DrawCircle3D({center.x, center.y + 0.02F, center.z}, radius + 0.18F,
                     {1.0F, 0.0F, 0.0F}, 90.0F, {255, 207, 108, 255});
    }
    if (enemy.vulnerabilityTimer > 0.0F) {
        DrawCircle3D({center.x, center.y + 0.05F, center.z}, radius + 0.24F,
                     {1.0F, 0.0F, 0.0F}, 90.0F, {255, 120, 205, 255});
    }
    if (enemy.elasticTimer > 0.0F) {
        DrawCircle3D({center.x, center.y + 0.03F, center.z}, radius + 0.3F,
                     {1.0F, 0.0F, 0.0F}, 90.0F, {133, 228, 255, 255});
    }
}

void DrawPlayer3D(const AppState &app) {
    const Vector2 p2 = {app.playerPosition.x + app.playerSize.x * 0.5F,
                        app.playerPosition.y + app.playerSize.y * 0.5F};
    const Vector2 facing2        = Normalize2D(app.facingDirection);
    const Vector2 strideOffset2D = {facing2.x * app.modelStrideOffset,
                                    facing2.y * app.modelStrideOffset};
    const Vector2 render2D       = {p2.x + strideOffset2D.x, p2.y + strideOffset2D.y};
    const Vector3 anchor         = ArenaToWorld(render2D, 0.0F + app.modelBobOffset);
    const Color   aura           = TypeColor(app.player.naturalType);
    const float   animatedScale  = app.playerModelScale * (1.0F + app.modelScalePulse);

    if (app.hasPlayerModel) {
        const Vector2 facing = facing2;
        const float yawDegrees =
            std::atan2(facing.x, facing.y) * RAD2DEG + app.playerModelYawOffset +
            app.modelLeanDegrees + app.modelCastLeanDegrees;
        const float yaw = yawDegrees * DEG2RAD;
        const Vector3 pivot{
            -app.playerModelPivot.x * animatedScale,
            -app.playerModelPivot.y * animatedScale,
            -app.playerModelPivot.z * animatedScale,
        };
        const Vector3 rotatedPivot{
            pivot.x * std::cos(yaw) - pivot.z * std::sin(yaw),
            pivot.y,
            pivot.x * std::sin(yaw) + pivot.z * std::cos(yaw),
        };
        const Vector3 drawPos{anchor.x + rotatedPivot.x, anchor.y + rotatedPivot.y,
                              anchor.z + rotatedPivot.z};
        const Color modelTint = app.chargingAura ? Color{240, 240, 248, 255} : WHITE;
        if (app.chargingAura) {
            DrawModelWiresEx(app.playerModel, drawPos, {0.0F, 1.0F, 0.0F}, yawDegrees,
                             {animatedScale, animatedScale, animatedScale}, Fade(WHITE, 0.26F));
        }
        DrawModelEx(app.playerModel, drawPos, {0.0F, 1.0F, 0.0F}, yawDegrees,
                    {animatedScale, animatedScale, animatedScale}, modelTint);
    } else {
        DrawCube({anchor.x, anchor.y + 0.22F, anchor.z}, 0.38F, 0.46F, 0.24F, {48, 74, 123, 255});
        DrawCube({anchor.x - 0.24F, anchor.y + 0.2F, anchor.z}, 0.1F, 0.34F, 0.1F,
                 {230, 220, 201, 255});
        DrawCube({anchor.x + 0.24F, anchor.y + 0.2F, anchor.z}, 0.1F, 0.34F, 0.1F,
                 {230, 220, 201, 255});
        DrawCube({anchor.x - 0.1F, anchor.y - 0.18F, anchor.z}, 0.1F, 0.34F, 0.1F,
                 {220, 211, 196, 255});
        DrawCube({anchor.x + 0.1F, anchor.y - 0.18F, anchor.z}, 0.1F, 0.34F, 0.1F,
                 {220, 211, 196, 255});
        DrawSphere({anchor.x, anchor.y + 0.57F, anchor.z}, 0.13F, {236, 226, 205, 255});
    }

    const float auraRadius = app.hasPlayerModel ? app.playerModelAuraRadius : 0.48F;
    DrawCircle3D({anchor.x, anchor.y + 0.22F, anchor.z}, auraRadius, {1.0F, 0.0F, 0.0F}, 90.0F,
                 Fade(aura, 0.8F));
    if (app.chargingAura) {
        const float t     = app.chargeEffectTimer;
        const float pulse = auraRadius + 0.18F + std::sin(t * 5.2F) * 0.07F;
        DrawSphereWires({anchor.x, anchor.y + 0.56F, anchor.z}, pulse, 10, 10, Fade(WHITE, 0.72F));
        DrawCircle3D({anchor.x, anchor.y + 0.22F, anchor.z}, pulse,
                     {1.0F, 0.0F, 0.0F}, 90.0F, WHITE);
        DrawCircle3D({anchor.x, anchor.y + 0.92F, anchor.z}, pulse * 0.88F,
                     {1.0F, 0.0F, 0.0F}, 90.0F, Fade(WHITE, 0.88F));
    }
}

void DrawEnSphere(const AppState &app) {
    if (app.cachedNenStats.detectionRadius <= 0.0F) {
        return;
    }
    const Vector2 p2 = {app.playerPosition.x + app.playerSize.x * 0.5F,
                        app.playerPosition.y + app.playerSize.y * 0.5F};
    const Vector3 center = ArenaToWorld(p2, 0.0F);
    const float   r      = app.cachedNenStats.detectionRadius;
    DrawCircle3D(center, r, {1.0F, 0.0F, 0.0F}, 90.0F, Fade({133, 228, 255, 255}, 0.18F));
    DrawCircle3D(center, r, {0.0F, 0.0F, 1.0F},  0.0F, Fade({133, 228, 255, 255}, 0.12F));
    DrawSphereWires(center, r, 6, 6, Fade({133, 228, 255, 255}, 0.08F));
}

void DrawWorld3D(const AppState &app) {
    BeginMode3D(app.camera);
    DrawArena3D(app);
    DrawAttackEffects3D(app);
    DrawElasticTether3D(app);
    DrawEnemy3D(app);
    DrawPlayer3D(app);
    DrawParticleSystem(app.particleSystem, app.camera);
    DrawEnSphere(app);
    EndMode3D();
}

} // namespace game
