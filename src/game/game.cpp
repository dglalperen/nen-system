#include "game/game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include <raylib.h>

#include "nen/affinity.hpp"
#include "nen/training.hpp"
#include "nen/types.hpp"

namespace game {

namespace {

struct TrainingZone {
    nen::Type type;
    Rectangle area;
    Color color;
};

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

std::array<TrainingZone, 6> BuildZones() {
    return {{
        {nen::Type::Enhancer, {30.0F, 120.0F, 170.0F, 130.0F}, TypeColor(nen::Type::Enhancer)},
        {nen::Type::Transmuter, {220.0F, 120.0F, 170.0F, 130.0F}, TypeColor(nen::Type::Transmuter)},
        {nen::Type::Emitter, {410.0F, 120.0F, 170.0F, 130.0F}, TypeColor(nen::Type::Emitter)},
        {nen::Type::Conjurer, {30.0F, 270.0F, 170.0F, 130.0F}, TypeColor(nen::Type::Conjurer)},
        {nen::Type::Manipulator, {220.0F, 270.0F, 170.0F, 130.0F}, TypeColor(nen::Type::Manipulator)},
        {nen::Type::Specialist, {410.0F, 270.0F, 170.0F, 130.0F}, TypeColor(nen::Type::Specialist)},
    }};
}

void ApplyTypeHotkeys(nen::Character* player) {
    if (IsKeyPressed(KEY_ONE)) {
        player->naturalType = nen::Type::Enhancer;
    } else if (IsKeyPressed(KEY_TWO)) {
        player->naturalType = nen::Type::Transmuter;
    } else if (IsKeyPressed(KEY_THREE)) {
        player->naturalType = nen::Type::Emitter;
    } else if (IsKeyPressed(KEY_FOUR)) {
        player->naturalType = nen::Type::Conjurer;
    } else if (IsKeyPressed(KEY_FIVE)) {
        player->naturalType = nen::Type::Manipulator;
    } else if (IsKeyPressed(KEY_SIX)) {
        player->naturalType = nen::Type::Specialist;
    }
}

}  // namespace

int Run() {
    constexpr int kWidth = 960;
    constexpr int kHeight = 540;

    InitWindow(kWidth, kHeight, "Nen World - 2D Prototype");
    SetTargetFPS(60);

    nen::Character player{
        .name = "alpi",
        .naturalType = nen::Type::Enhancer,
        .auraPool = 100,
    };
    Vector2 playerPosition{470.0F, 455.0F};
    const Vector2 playerSize{20.0F, 20.0F};
    const float movementSpeed = 230.0F;

    const std::array<TrainingZone, 6> zones = BuildZones();
    std::string statusMessage = "Move with WASD or arrow keys.";

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        ApplyTypeHotkeys(&player);

        Vector2 movement{0.0F, 0.0F};
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
            movement.y -= 1.0F;
        }
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
            movement.y += 1.0F;
        }
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
            movement.x -= 1.0F;
        }
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
            movement.x += 1.0F;
        }

        if (movement.x != 0.0F || movement.y != 0.0F) {
            const float length = std::sqrt(movement.x * movement.x + movement.y * movement.y);
            movement.x /= length;
            movement.y /= length;
        }

        playerPosition.x = std::clamp(playerPosition.x + movement.x * movementSpeed * dt, 0.0F,
                                      static_cast<float>(kWidth) - playerSize.x);
        playerPosition.y = std::clamp(playerPosition.y + movement.y * movementSpeed * dt, 95.0F,
                                      static_cast<float>(kHeight) - playerSize.y);

        const Rectangle playerRect{playerPosition.x, playerPosition.y, playerSize.x, playerSize.y};
        const TrainingZone* activeZone = nullptr;
        for (const auto& zone : zones) {
            if (CheckCollisionRecs(playerRect, zone.area)) {
                activeZone = &zone;
                break;
            }
        }

        if (activeZone != nullptr && IsKeyPressed(KEY_E)) {
            const int efficiency = nen::EfficiencyPercent(player.naturalType, activeZone->type);
            const int gainedAura = std::max(1, efficiency / 20);
            player.auraPool = std::min(1500, player.auraPool + gainedAura);
            statusMessage = "Trained in " + std::string(nen::ToString(activeZone->type)) + " zone +" +
                            std::to_string(gainedAura) + " aura.";
        }

        const nen::TrainingPlan starterPlan = nen::BuildStarterPlan(player.naturalType);

        BeginDrawing();
        ClearBackground({16, 23, 36, 255});

        DrawRectangle(0, 0, kWidth, 80, {24, 34, 53, 255});
        DrawText("Nen World Prototype (Phase 1)", 24, 20, 26, RAYWHITE);
        DrawText("1-6: switch natural type | E: train in a zone | ESC: quit", 24, 50, 16,
                 LIGHTGRAY);

        for (const auto& zone : zones) {
            const bool isActive = activeZone != nullptr && activeZone->type == zone.type;
            const Color fill = isActive ? zone.color : Fade(zone.color, 0.65F);
            DrawRectangleRec(zone.area, fill);
            DrawRectangleLinesEx(zone.area, 2.0F, isActive ? WHITE : Fade(WHITE, 0.45F));
            DrawText(nen::ToString(zone.type).data(), static_cast<int>(zone.area.x) + 12,
                     static_cast<int>(zone.area.y) + 12, 20, {10, 15, 24, 255});

            const int efficiency = nen::EfficiencyPercent(player.naturalType, zone.type);
            DrawText(TextFormat("%d%%", efficiency), static_cast<int>(zone.area.x) + 12,
                     static_cast<int>(zone.area.y) + 88, 26, {10, 15, 24, 255});
        }

        DrawRectangleRec(playerRect, {243, 242, 228, 255});

        DrawText(TextFormat("Natural Type: %s", nen::ToString(player.naturalType).data()), 620, 140,
                 20, RAYWHITE);
        DrawText(TextFormat("Aura Pool: %d", player.auraPool), 620, 172, 20, RAYWHITE);
        DrawText("Starter Plan", 620, 225, 20, RAYWHITE);

        DrawText(TextFormat("1) %s", nen::ToString(starterPlan.focusOrder[0].type).data()), 620, 255,
                 18, LIGHTGRAY);
        DrawText(TextFormat("2) %s", nen::ToString(starterPlan.focusOrder[1].type).data()), 620, 281,
                 18, LIGHTGRAY);
        DrawText(TextFormat("3) %s", nen::ToString(starterPlan.focusOrder[2].type).data()), 620, 307,
                 18, LIGHTGRAY);

        DrawRectangle(0, 500, kWidth, 40, {24, 34, 53, 255});
        DrawText(statusMessage.c_str(), 20, 511, 16, {218, 223, 235, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

}  // namespace game
