#include "game/game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include <raylib.h>

#include "game/attack_system.hpp"
#include "game/persistence.hpp"
#include "nen/affinity.hpp"
#include "nen/combat.hpp"
#include "nen/quiz.hpp"
#include "nen/training.hpp"
#include "nen/types.hpp"

namespace game {

namespace {

constexpr int kWidth = 1100;
constexpr int kHeight = 640;
constexpr int kDummyMaxHealth = 220;
constexpr int kAttackAuraCost = 16;
constexpr int kAttackBasePower = 28;

enum class Screen {
    MainMenu,
    NameEntry,
    LoadCharacter,
    Quiz,
    Reveal,
    World,
};

struct TrainingZone {
    nen::Type type;
    Rectangle area;
    Color color;
};

struct AppState {
    Screen screen = Screen::MainMenu;
    int menuSelection = 0;
    int loadSelection = 0;

    std::filesystem::path saveDir = DefaultSaveDirectory();
    std::vector<StoredCharacter> storedCharacters;

    std::string draftName;
    nen::QuizScores quizScores{};
    std::size_t quizQuestionIndex = 0;
    float revealTimer = 0.0F;

    nen::Character player{
        .name = "",
        .naturalType = nen::Type::Enhancer,
        .auraPool = 120,
    };
    bool hasCharacter = false;
    Vector2 playerPosition{520.0F, 520.0F};
    Vector2 playerSize{20.0F, 20.0F};
    nen::Type selectedAttackType = nen::Type::Enhancer;

    std::vector<AttackEffect> activeAttacks;
    float attackCooldown = 0.0F;
    Vector2 dummyCenter{845.0F, 445.0F};
    float dummyRadius = 38.0F;
    int dummyHealth = kDummyMaxHealth;
    int activeZoneIndex = -1;
    std::string statusMessage = "Select New Character or Load Character.";
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
        {nen::Type::Enhancer, {30.0F, 130.0F, 180.0F, 130.0F}, TypeColor(nen::Type::Enhancer)},
        {nen::Type::Transmuter, {230.0F, 130.0F, 180.0F, 130.0F}, TypeColor(nen::Type::Transmuter)},
        {nen::Type::Emitter, {430.0F, 130.0F, 180.0F, 130.0F}, TypeColor(nen::Type::Emitter)},
        {nen::Type::Conjurer, {30.0F, 280.0F, 180.0F, 130.0F}, TypeColor(nen::Type::Conjurer)},
        {nen::Type::Manipulator,
         {230.0F, 280.0F, 180.0F, 130.0F},
         TypeColor(nen::Type::Manipulator)},
        {nen::Type::Specialist, {430.0F, 280.0F, 180.0F, 130.0F}, TypeColor(nen::Type::Specialist)},
    }};
}

void HandleNameInput(std::string *text) {
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 125 && text->size() < 20) {
            text->push_back(static_cast<char>(key));
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !text->empty()) {
        text->pop_back();
    }
}

void RefreshStoredCharacters(AppState *app) {
    std::string error;
    app->storedCharacters = ListCharacters(app->saveDir, &error);
    if (!error.empty()) {
        app->statusMessage = error;
    }
    if (app->loadSelection >= static_cast<int>(app->storedCharacters.size())) {
        app->loadSelection = std::max(0, static_cast<int>(app->storedCharacters.size()) - 1);
    }
}

void SaveCurrentCharacter(AppState *app) {
    if (!app->hasCharacter) {
        return;
    }
    std::string error;
    if (!SaveCharacter(app->player, app->saveDir, &error)) {
        app->statusMessage = error;
    }
}

void StartWorld(AppState *app, const nen::Character &character) {
    app->screen = Screen::World;
    app->player = character;
    app->hasCharacter = true;
    app->selectedAttackType = character.naturalType;
    app->playerPosition = {560.0F, 520.0F};
    app->dummyHealth = kDummyMaxHealth;
    app->activeZoneIndex = -1;
    app->activeAttacks.clear();
    app->attackCooldown = 0.0F;
    app->statusMessage =
        "Move with WASD. E trains aura in a zone. SPACE casts animated Nen attack.";
}

void UpdateMainMenu(AppState *app) {
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        app->menuSelection = (app->menuSelection + 1) % 2;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        app->menuSelection = (app->menuSelection + 1) % 2;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (app->menuSelection == 0) {
            app->screen = Screen::NameEntry;
            app->draftName.clear();
            app->statusMessage = "Type your character name, then press ENTER.";
        } else {
            app->screen = Screen::LoadCharacter;
            RefreshStoredCharacters(app);
            if (app->storedCharacters.empty()) {
                app->statusMessage = "No saved character found. Create one first.";
            } else {
                app->statusMessage = "Select a saved character and press ENTER.";
            }
        }
    }
}

void UpdateNameEntry(AppState *app) {
    HandleNameInput(&app->draftName);

    if (IsKeyPressed(KEY_ESCAPE)) {
        app->screen = Screen::MainMenu;
        return;
    }

    if (!IsKeyPressed(KEY_ENTER)) {
        return;
    }
    if (app->draftName.empty()) {
        app->statusMessage = "Name cannot be empty.";
        return;
    }

    app->quizScores.fill(0);
    app->quizQuestionIndex = 0;
    app->screen = Screen::Quiz;
    app->statusMessage = "Answer the personality quiz with keys 1, 2, or 3.";
}

void UpdateLoadCharacter(AppState *app) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        app->screen = Screen::MainMenu;
        return;
    }

    if (app->storedCharacters.empty()) {
        return;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        app->loadSelection =
            (app->loadSelection + static_cast<int>(app->storedCharacters.size()) - 1) %
            static_cast<int>(app->storedCharacters.size());
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        app->loadSelection =
            (app->loadSelection + 1) % static_cast<int>(app->storedCharacters.size());
    }

    if (IsKeyPressed(KEY_ENTER)) {
        StartWorld(app, app->storedCharacters[app->loadSelection].character);
        app->statusMessage = "Loaded " + app->player.name + ".";
    }
}

void UpdateQuiz(AppState *app) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        app->screen = Screen::MainMenu;
        return;
    }

    const auto &questions = nen::PersonalityQuestions();
    int answerIndex = -1;

    if (IsKeyPressed(KEY_ONE)) {
        answerIndex = 0;
    } else if (IsKeyPressed(KEY_TWO)) {
        answerIndex = 1;
    } else if (IsKeyPressed(KEY_THREE)) {
        answerIndex = 2;
    }

    if (answerIndex < 0) {
        return;
    }

    nen::ApplyQuizAnswer(&app->quizScores, questions[app->quizQuestionIndex].options[answerIndex]);
    app->quizQuestionIndex += 1;

    if (app->quizQuestionIndex < questions.size()) {
        return;
    }

    app->player = nen::Character{
        .name = app->draftName,
        .naturalType = nen::DetermineNenType(app->quizScores),
        .auraPool = 120,
    };
    app->hasCharacter = true;
    app->selectedAttackType = app->player.naturalType;
    app->revealTimer = 0.0F;
    app->screen = Screen::Reveal;
    app->statusMessage = "Water divination complete. Press ENTER to enter the world.";
}

void UpdateReveal(AppState *app, float dt) {
    app->revealTimer += dt;
    if (app->revealTimer >= 1.5F && IsKeyPressed(KEY_ENTER)) {
        StartWorld(app, app->player);
        SaveCurrentCharacter(app);
        RefreshStoredCharacters(app);
    }
}

void UpdateAttackTypeHotkeys(AppState *app) {
    if (IsKeyPressed(KEY_ONE)) {
        app->selectedAttackType = nen::Type::Enhancer;
    } else if (IsKeyPressed(KEY_TWO)) {
        app->selectedAttackType = nen::Type::Transmuter;
    } else if (IsKeyPressed(KEY_THREE)) {
        app->selectedAttackType = nen::Type::Emitter;
    } else if (IsKeyPressed(KEY_FOUR)) {
        app->selectedAttackType = nen::Type::Conjurer;
    } else if (IsKeyPressed(KEY_FIVE)) {
        app->selectedAttackType = nen::Type::Manipulator;
    } else if (IsKeyPressed(KEY_SIX)) {
        app->selectedAttackType = nen::Type::Specialist;
    }
}

void UpdateMovement(AppState *app, float dt) {
    constexpr float kSpeed = 240.0F;
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

    app->playerPosition.x = std::clamp(app->playerPosition.x + movement.x * kSpeed * dt, 0.0F,
                                       static_cast<float>(kWidth) - app->playerSize.x);
    app->playerPosition.y = std::clamp(app->playerPosition.y + movement.y * kSpeed * dt, 95.0F,
                                       static_cast<float>(kHeight) - app->playerSize.y - 45.0F);
}

void UpdateWorld(AppState *app, float dt, const std::array<TrainingZone, 6> &zones) {
    UpdateAttackTypeHotkeys(app);
    UpdateMovement(app, dt);
    app->attackCooldown = std::max(0.0F, app->attackCooldown - dt);

    const Rectangle playerRect{app->playerPosition.x, app->playerPosition.y, app->playerSize.x,
                               app->playerSize.y};
    app->activeZoneIndex = -1;
    for (std::size_t i = 0; i < zones.size(); ++i) {
        if (CheckCollisionRecs(playerRect, zones[i].area)) {
            app->activeZoneIndex = static_cast<int>(i);
            break;
        }
    }

    if (app->activeZoneIndex >= 0 && IsKeyPressed(KEY_E)) {
        const nen::Type zoneType = zones[static_cast<std::size_t>(app->activeZoneIndex)].type;
        const int efficiency = nen::EfficiencyPercent(app->player.naturalType, zoneType);
        const int gainedAura = std::max(2, efficiency / 12);
        app->player.auraPool = std::min(2200, app->player.auraPool + gainedAura);
        app->statusMessage = "Training in " + std::string(nen::ToString(zoneType)) +
                             " zone gave +" + std::to_string(gainedAura) +
                             " aura (E uses type efficiency).";
        SaveCurrentCharacter(app);
    }

    const int frameDamage =
        UpdateAttackEffects(&app->activeAttacks, dt, app->dummyCenter, app->dummyRadius);
    if (frameDamage > 0) {
        app->dummyHealth = std::max(0, app->dummyHealth - frameDamage);
        app->statusMessage = std::string(nen::ToString(app->selectedAttackType)) +
                             " aura attack connected for " + std::to_string(frameDamage) +
                             " damage.";

        if (app->dummyHealth == 0) {
            app->dummyHealth = kDummyMaxHealth;
            app->player.auraPool = std::min(2200, app->player.auraPool + 24);
            app->statusMessage += " Dummy broken. +24 aura reward.";
        }
        SaveCurrentCharacter(app);
    }

    if (IsKeyPressed(KEY_SPACE)) {
        if (app->attackCooldown > 0.0F) {
            app->statusMessage = "Attack is recharging...";
            return;
        }

        if (!nen::TryConsumeAura(&app->player, kAttackAuraCost)) {
            app->statusMessage =
                "Not enough aura for attack. Need " + std::to_string(kAttackAuraCost) + ".";
            return;
        }

        const Vector2 origin{app->playerPosition.x + app->playerSize.x * 0.5F,
                             app->playerPosition.y + app->playerSize.y * 0.5F};
        const int damage = nen::ComputeAttackDamage(app->player.naturalType,
                                                    app->selectedAttackType, kAttackBasePower);
        SpawnAttackEffect(&app->activeAttacks, app->selectedAttackType, origin, app->dummyCenter,
                          damage);
        app->attackCooldown = 0.22F;
        app->statusMessage = std::string("Cast ") +
                             std::string(nen::ToString(app->selectedAttackType)) +
                             " aura attack. Cost -" + std::to_string(kAttackAuraCost) + " aura.";
        SaveCurrentCharacter(app);
    }

    if (IsKeyPressed(KEY_F5)) {
        SaveCurrentCharacter(app);
        RefreshStoredCharacters(app);
        app->statusMessage = "Character saved.";
    }
}

void DrawMainMenu(const AppState &app) {
    DrawText("Nen World Prototype", 64, 60, 46, RAYWHITE);
    DrawText("Choose an option", 64, 120, 24, LIGHTGRAY);

    const Color firstColor = app.menuSelection == 0 ? WHITE : Fade(WHITE, 0.7F);
    const Color secondColor = app.menuSelection == 1 ? WHITE : Fade(WHITE, 0.7F);
    DrawText("New Character", 96, 210, 34, firstColor);
    DrawText("Load Existing Character", 96, 270, 34, secondColor);

    DrawText("Use UP/DOWN + ENTER", 96, 340, 20, LIGHTGRAY);
}

void DrawNameEntry(const AppState &app) {
    DrawText("Create New Character", 64, 60, 46, RAYWHITE);
    DrawText("Type your hunter name and press ENTER", 64, 130, 24, LIGHTGRAY);
    DrawRectangle(64, 210, 500, 54, {19, 29, 48, 255});
    DrawRectangleLines(64, 210, 500, 54, WHITE);
    DrawText(app.draftName.c_str(), 80, 225, 28, RAYWHITE);
    DrawText("ESC: back", 64, 290, 18, LIGHTGRAY);
}

void DrawLoadCharacter(const AppState &app) {
    DrawText("Load Existing Character", 64, 60, 46, RAYWHITE);
    DrawText("Choose a save and press ENTER", 64, 120, 24, LIGHTGRAY);

    if (app.storedCharacters.empty()) {
        DrawText("No characters saved yet.", 64, 210, 28, LIGHTGRAY);
        DrawText("Press ESC to return.", 64, 250, 20, LIGHTGRAY);
        return;
    }

    int y = 190;
    for (std::size_t i = 0; i < app.storedCharacters.size(); ++i) {
        const bool selected = static_cast<int>(i) == app.loadSelection;
        const Color color = selected ? WHITE : Fade(WHITE, 0.7F);
        const auto &character = app.storedCharacters[i].character;
        const std::string line = character.name + " | " +
                                 std::string(nen::ToString(character.naturalType)) + " | aura " +
                                 std::to_string(character.auraPool);
        DrawText(line.c_str(), 90, y, 24, color);
        y += 36;
    }
}

void DrawQuiz(const AppState &app) {
    const auto &questions = nen::PersonalityQuestions();
    const auto &question = questions[app.quizQuestionIndex];

    DrawText("Nen Personality Assessment", 64, 52, 42, RAYWHITE);
    DrawText(TextFormat("Question %d / %d", static_cast<int>(app.quizQuestionIndex + 1),
                        static_cast<int>(questions.size())),
             64, 108, 24, LIGHTGRAY);

    DrawRectangle(64, 152, 960, 86, {25, 35, 57, 255});
    DrawText(question.prompt.data(), 84, 186, 28, RAYWHITE);

    DrawText("1)", 84, 280, 30, WHITE);
    DrawText(question.options[0].text.data(), 132, 282, 24, LIGHTGRAY);
    DrawText("2)", 84, 332, 30, WHITE);
    DrawText(question.options[1].text.data(), 132, 334, 24, LIGHTGRAY);
    DrawText("3)", 84, 384, 30, WHITE);
    DrawText(question.options[2].text.data(), 132, 386, 24, LIGHTGRAY);
}

void DrawReveal(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});
    DrawText("Water Divination", 64, 54, 46, RAYWHITE);
    DrawText("Your aura has been measured.", 64, 116, 24, LIGHTGRAY);

    DrawText(TextFormat("%s is a %s!", app.player.name.c_str(),
                        nen::ToString(app.player.naturalType).data()),
             64, 168, 40, WHITE);
    DrawText(nen::WaterDivinationEffect(app.player.naturalType).data(), 64, 226, 24, LIGHTGRAY);

    const Rectangle glass{750.0F, 158.0F, 220.0F, 312.0F};
    DrawRectangleRounded(glass, 0.04F, 6, Fade({46, 68, 112, 255}, 0.4F));
    DrawRectangleRoundedLinesEx(glass, 0.04F, 6, 4.0F, WHITE);

    float waterLevel = 132.0F;
    Color waterBase = {73, 136, 236, 255};
    Color glowColor = {128, 179, 255, 255};
    float leafOffset = std::sin(app.revealTimer * 2.5F) * 28.0F;

    switch (app.player.naturalType) {
    case nen::Type::Enhancer:
        waterLevel = 132.0F + std::min(130.0F, app.revealTimer * 72.0F);
        glowColor = {166, 236, 158, 255};
        break;
    case nen::Type::Transmuter:
        waterBase = {98, 216, 255, 255};
        glowColor = {188, 255, 250, 255};
        break;
    case nen::Type::Emitter:
        waterBase = {144, 124, 255, 255};
        glowColor = {218, 149, 255, 255};
        break;
    case nen::Type::Conjurer:
        waterBase = {95, 138, 236, 255};
        glowColor = {198, 170, 255, 255};
        break;
    case nen::Type::Manipulator:
        waterBase = {99, 170, 228, 255};
        glowColor = {232, 229, 171, 255};
        break;
    case nen::Type::Specialist:
        waterBase = {171, 105, 234, 255};
        glowColor = {255, 154, 221, 255};
        break;
    }

    const Rectangle waterArea{
        glass.x + 14.0F,
        glass.y + glass.height - waterLevel - 14.0F,
        glass.width - 28.0F,
        waterLevel,
    };
    DrawRectangleRounded(waterArea, 0.05F, 5, waterBase);
    DrawRectangleRounded(waterArea, 0.05F, 5, Fade(glowColor, 0.16F));

    const int waveSteps = 16;
    for (int i = 0; i < waveSteps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(waveSteps - 1);
        const float x = waterArea.x + t * waterArea.width;
        const float y = waterArea.y + std::sin(t * 8.5F + app.revealTimer * 5.0F) * 5.0F;
        DrawCircle(static_cast<int>(x), static_cast<int>(y), 2.2F, Fade(WHITE, 0.9F));
    }

    DrawRectangle(static_cast<int>(glass.x + 96.0F + leafOffset), static_cast<int>(glass.y + 92.0F),
                  34, 12, {198, 230, 169, 255});

    switch (app.player.naturalType) {
    case nen::Type::Enhancer:
        DrawRectangle(static_cast<int>(waterArea.x + 4.0F), static_cast<int>(waterArea.y - 16.0F),
                      static_cast<int>(waterArea.width - 8.0F), 14, Fade(glowColor, 0.6F));
        break;
    case nen::Type::Transmuter:
        DrawLineEx({waterArea.x + 24.0F, waterArea.y + 30.0F},
                   {waterArea.x + 98.0F, waterArea.y + 66.0F}, 3.0F, glowColor);
        DrawLineEx({waterArea.x + 98.0F, waterArea.y + 66.0F},
                   {waterArea.x + 62.0F, waterArea.y + 90.0F}, 3.0F, glowColor);
        break;
    case nen::Type::Emitter:
        DrawRing({glass.x + glass.width * 0.5F, waterArea.y + 54.0F}, 16.0F, 32.0F, 0.0F, 360.0F,
                 42, Fade(glowColor, 0.75F));
        break;
    case nen::Type::Conjurer:
        for (int i = 0; i < 5; ++i) {
            const float px = waterArea.x + 26.0F + static_cast<float>(i) * 32.0F;
            const float py = waterArea.y + 38.0F + std::sin(app.revealTimer * 2.0F + i) * 8.0F;
            DrawPoly({px, py}, 4, 6.0F, 45.0F, Fade(glowColor, 0.85F));
        }
        break;
    case nen::Type::Manipulator:
        DrawLineBezier({waterArea.x + 34.0F, waterArea.y + 30.0F},
                       {waterArea.x + waterArea.width - 40.0F, waterArea.y + 64.0F}, 2.5F,
                       Fade(glowColor, 0.85F));
        break;
    case nen::Type::Specialist:
        DrawPoly({glass.x + glass.width * 0.5F, waterArea.y + 52.0F}, 7, 24.0F,
                 app.revealTimer * 72.0F, Fade(glowColor, 0.8F));
        DrawCircle(glass.x + glass.width * 0.5F, waterArea.y + 52.0F, 10.0F, Fade(WHITE, 0.7F));
        break;
    }

    DrawText("Press ENTER to continue", 64, 520, 24, LIGHTGRAY);
}

void DrawWorld(const AppState &app, const std::array<TrainingZone, 6> &zones) {
    const Rectangle playerRect{app.playerPosition.x, app.playerPosition.y, app.playerSize.x,
                               app.playerSize.y};
    const nen::TrainingPlan starterPlan = nen::BuildStarterPlan(app.player.naturalType);

    DrawRectangle(0, 0, kWidth, 88, {24, 34, 53, 255});
    DrawText("Nen World - Phase 2 Prototype", 24, 18, 34, RAYWHITE);
    DrawText("E: train in zone | 1-6: choose attack type | SPACE: cast animated aura attack | F5: "
             "save | ESC: quit",
             24, 56, 18, LIGHTGRAY);

    for (std::size_t i = 0; i < zones.size(); ++i) {
        const auto &zone = zones[i];
        const bool active = app.activeZoneIndex == static_cast<int>(i);
        const Color fill = active ? zone.color : Fade(zone.color, 0.66F);

        DrawRectangleRec(zone.area, fill);
        DrawRectangleLinesEx(zone.area, 2.0F, active ? WHITE : Fade(WHITE, 0.45F));
        DrawText(zone.type == app.selectedAttackType ? "SELECTED ATTACK" : "",
                 static_cast<int>(zone.area.x) + 10, static_cast<int>(zone.area.y) - 22, 14, WHITE);
        DrawText(nen::ToString(zone.type).data(), static_cast<int>(zone.area.x) + 12,
                 static_cast<int>(zone.area.y) + 12, 24, {10, 15, 24, 255});
        DrawText(TextFormat("%d%%", nen::EfficiencyPercent(app.player.naturalType, zone.type)),
                 static_cast<int>(zone.area.x) + 12, static_cast<int>(zone.area.y) + 90, 34,
                 {10, 15, 24, 255});
    }

    DrawAttackEffects(app.activeAttacks);
    DrawRectangleRec(playerRect, {243, 242, 228, 255});
    DrawCircleV(app.dummyCenter, app.dummyRadius, {124, 68, 68, 255});
    DrawCircleLinesV(app.dummyCenter, app.dummyRadius + 2.0F, Fade(WHITE, 0.8F));
    DrawCircleV(app.dummyCenter, app.dummyRadius * 0.62F, {186, 92, 92, 255});

    DrawText(TextFormat("Name: %s", app.player.name.c_str()), 650, 150, 24, RAYWHITE);
    DrawText(TextFormat("Natural Type: %s", nen::ToString(app.player.naturalType).data()), 650, 184,
             24, RAYWHITE);
    DrawText(TextFormat("Aura Pool: %d", app.player.auraPool), 650, 218, 24, RAYWHITE);
    DrawText(TextFormat("Attack Type: %s", nen::ToString(app.selectedAttackType).data()), 650, 252,
             24, RAYWHITE);
    DrawText(TextFormat("Attack Modifier: x%.2f",
                        static_cast<float>(
                            nen::DamageModifier(app.player.naturalType, app.selectedAttackType))),
             650, 286, 24, LIGHTGRAY);
    DrawText(TextFormat("Cooldown: %.2fs", app.attackCooldown), 650, 314, 20, LIGHTGRAY);

    DrawText("Starter Plan", 650, 344, 24, RAYWHITE);
    DrawText(TextFormat("1) %s", nen::ToString(starterPlan.focusOrder[0].type).data()), 650, 376,
             20, LIGHTGRAY);
    DrawText(TextFormat("2) %s", nen::ToString(starterPlan.focusOrder[1].type).data()), 650, 404,
             20, LIGHTGRAY);
    DrawText(TextFormat("3) %s", nen::ToString(starterPlan.focusOrder[2].type).data()), 650, 432,
             20, LIGHTGRAY);

    DrawRectangle(720, 470, 290, 110, {23, 32, 49, 255});
    DrawRectangleLines(720, 470, 290, 110, Fade(WHITE, 0.7F));
    DrawText("Training Dummy", 744, 488, 22, RAYWHITE);
    DrawRectangle(744, 522, 240, 18, {53, 68, 94, 255});
    const int hpBarWidth = static_cast<int>((240.0F * static_cast<float>(app.dummyHealth)) /
                                            static_cast<float>(kDummyMaxHealth));
    DrawRectangle(744, 522, hpBarWidth, 18, {224, 83, 83, 255});
    DrawText(TextFormat("%d / %d HP", app.dummyHealth, kDummyMaxHealth), 744, 548, 18, LIGHTGRAY);
}

} // namespace

int Run() {
    InitWindow(kWidth, kHeight, "Nen World - 2D Prototype");
    SetTargetFPS(60);

    const std::array<TrainingZone, 6> zones = BuildZones();
    AppState app;
    RefreshStoredCharacters(&app);

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();

        switch (app.screen) {
        case Screen::MainMenu:
            UpdateMainMenu(&app);
            break;
        case Screen::NameEntry:
            UpdateNameEntry(&app);
            break;
        case Screen::LoadCharacter:
            UpdateLoadCharacter(&app);
            break;
        case Screen::Quiz:
            UpdateQuiz(&app);
            break;
        case Screen::Reveal:
            UpdateReveal(&app, dt);
            break;
        case Screen::World:
            UpdateWorld(&app, dt, zones);
            break;
        }

        BeginDrawing();
        ClearBackground({16, 23, 36, 255});

        switch (app.screen) {
        case Screen::MainMenu:
            DrawMainMenu(app);
            break;
        case Screen::NameEntry:
            DrawNameEntry(app);
            break;
        case Screen::LoadCharacter:
            DrawLoadCharacter(app);
            break;
        case Screen::Quiz:
            DrawQuiz(app);
            break;
        case Screen::Reveal:
            DrawReveal(app);
            break;
        case Screen::World:
            DrawWorld(app, zones);
            break;
        }

        DrawRectangle(0, kHeight - 42, kWidth, 42, {24, 34, 53, 255});
        DrawText(app.statusMessage.c_str(), 20, kHeight - 29, 20, {218, 223, 235, 255});

        EndDrawing();
    }

    SaveCurrentCharacter(&app);
    CloseWindow();
    return 0;
}

} // namespace game
