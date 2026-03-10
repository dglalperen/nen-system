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
#include "nen/combat.hpp"
#include "nen/hatsu.hpp"
#include "nen/quiz.hpp"
#include "nen/types.hpp"

namespace game {

namespace {

constexpr int kWidth = 1560;
constexpr int kHeight = 920;
constexpr int kAuraMax = 2400;
constexpr int kEnemyMaxHealth = 430;

constexpr int kBaseAttackAuraCost = 16;
constexpr int kHatsuAuraCost = 52;
constexpr int kBaseAttackPower = 25;
constexpr int kHatsuAttackPower = 44;
constexpr float kWorldScale = 0.035F;

const Rectangle kArenaBounds{40.0F, 110.0F, 1020.0F, 730.0F};

enum class Screen {
    MainMenu,
    NameEntry,
    LoadCharacter,
    Quiz,
    Reveal,
    World,
};

struct EnemyState {
    Vector2 position{810.0F, 450.0F};
    Vector2 velocity{180.0F, 120.0F};
    float radius = 42.0F;
    int maxHealth = kEnemyMaxHealth;
    int health = kEnemyMaxHealth;
    float manipulatedTimer = 0.0F;
    float vulnerabilityTimer = 0.0F;
    float hitFlashTimer = 0.0F;
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
    Vector2 playerPosition{kArenaBounds.x + 110.0F, kArenaBounds.y + kArenaBounds.height - 130.0F};
    Vector2 playerSize{36.0F, 52.0F};
    bool chargingAura = false;
    float chargeEffectTimer = 0.0F;
    nen::Type selectedBaseType = nen::Type::Enhancer;

    float baseAttackCooldown = 0.0F;
    float hatsuCooldown = 0.0F;
    std::vector<AttackEffect> activeAttacks;
    EnemyState enemy;
    Model playerModel{};
    bool hasPlayerModel = false;
    float playerModelScale = 1.0F;
    float playerModelYOffset = 0.0F;
    float playerModelYawOffset = 180.0F;
    std::string playerModelPath;
    std::string playerModelStatus = "Model not loaded";
    bool use3DView = true;
    Camera3D camera{
        .position = {0.0F, 34.0F, 30.0F},
        .target = {0.0F, 0.0F, 0.0F},
        .up = {0.0F, 1.0F, 0.0F},
        .fovy = 45.0F,
        .projection = CAMERA_PERSPECTIVE,
    };

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

Vector3 ArenaToWorld(Vector2 arenaPosition, float y = 0.0F) {
    const float cx = kArenaBounds.x + kArenaBounds.width * 0.5F;
    const float cy = kArenaBounds.y + kArenaBounds.height * 0.5F;
    return {(arenaPosition.x - cx) * kWorldScale, y, (arenaPosition.y - cy) * kWorldScale};
}

Vector2 WorldToArena(Vector3 worldPosition) {
    const float cx = kArenaBounds.x + kArenaBounds.width * 0.5F;
    const float cy = kArenaBounds.y + kArenaBounds.height * 0.5F;
    return {worldPosition.x / kWorldScale + cx, worldPosition.z / kWorldScale + cy};
}

bool MouseToArenaPoint(const Camera3D &camera, Vector2 *outArenaPoint) {
    if (outArenaPoint == nullptr) {
        return false;
    }

    const Ray ray = GetMouseRay(GetMousePosition(), camera);
    if (std::abs(ray.direction.y) < 0.0001F) {
        return false;
    }
    const float t = -ray.position.y / ray.direction.y;
    if (t < 0.0F) {
        return false;
    }

    const Vector3 hit{
        ray.position.x + ray.direction.x * t,
        0.0F,
        ray.position.z + ray.direction.z * t,
    };
    *outArenaPoint = WorldToArena(hit);
    outArenaPoint->x =
        std::clamp(outArenaPoint->x, kArenaBounds.x, kArenaBounds.x + kArenaBounds.width);
    outArenaPoint->y =
        std::clamp(outArenaPoint->y, kArenaBounds.y, kArenaBounds.y + kArenaBounds.height);
    return true;
}

bool IsInside(const Rectangle &rect, Vector2 point) { return CheckCollisionPointRec(point, rect); }

bool IsButtonTriggered(const Rectangle &rect) {
    return IsInside(rect, GetMousePosition()) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void DrawButton(const Rectangle &rect, const std::string &label, bool selected) {
    const bool hovered = IsInside(rect, GetMousePosition());
    const Color fill = selected || hovered ? Color{57, 87, 143, 255} : Color{32, 50, 82, 255};
    DrawRectangleRounded(rect, 0.2F, 8, fill);
    DrawRectangleRoundedLinesEx(rect, 0.2F, 8, 2.0F,
                                selected || hovered ? WHITE : Fade(WHITE, 0.5F));
    DrawText(label.c_str(), static_cast<int>(rect.x + 18.0F),
             static_cast<int>(rect.y + rect.height * 0.28F), 28, RAYWHITE);
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

std::string ResolveKilluaModelPath() {
    const std::filesystem::path relative =
        std::filesystem::path("assets") / "models" / "killua.glb";
    const std::array<std::filesystem::path, 6> candidates = {
        relative,
        std::filesystem::path("..") / relative,
        std::filesystem::path("../..") / relative,
        std::filesystem::path("../../..") / relative,
        std::filesystem::path(GetWorkingDirectory()) / relative,
        std::filesystem::path(GetApplicationDirectory()) / relative,
    };

    for (const auto &candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return std::filesystem::weakly_canonical(candidate, ec).string();
        }
    }
    return {};
}

void TryLoadPlayerModel(AppState *app) {
    if (app == nullptr) {
        return;
    }

    const std::string modelPath = ResolveKilluaModelPath();
    app->playerModelPath = modelPath;
    if (modelPath.empty()) {
        app->playerModelStatus = "Killua model not found (checked assets/models/killua.glb paths)";
        app->statusMessage = "Killua model not found. Using fallback character geometry.";
        return;
    }

    Model model = LoadModel(modelPath.c_str());
    if (!IsModelValid(model)) {
        app->playerModelStatus = "Killua model file found but failed to load";
        app->statusMessage = "Found killua.glb but loading failed. Using fallback.";
        return;
    }

    app->playerModel = model;
    app->hasPlayerModel = true;
    app->playerModelStatus = "Killua model loaded";

    const BoundingBox bounds = GetModelBoundingBox(model);
    const float height = bounds.max.y - bounds.min.y;
    if (height > 0.001F) {
        app->playerModelScale = std::clamp(1.75F / height, 0.02F, 8.0F);
        app->playerModelYOffset = -bounds.min.y * app->playerModelScale;
    } else {
        app->playerModelScale = 1.0F;
        app->playerModelYOffset = 0.0F;
    }
}

void UnloadPlayerModel(AppState *app) {
    if (app == nullptr || !app->hasPlayerModel) {
        return;
    }
    UnloadModel(app->playerModel);
    app->hasPlayerModel = false;
    app->playerModelStatus = "Model unloaded";
}

void StartWorld(AppState *app, const nen::Character &character) {
    app->screen = Screen::World;
    app->player = character;
    app->hasCharacter = true;
    app->playerPosition = {kArenaBounds.x + 110.0F, kArenaBounds.y + kArenaBounds.height - 130.0F};
    app->chargingAura = false;
    app->chargeEffectTimer = 0.0F;
    app->selectedBaseType = character.naturalType;
    app->baseAttackCooldown = 0.0F;
    app->hatsuCooldown = 0.0F;
    app->activeAttacks.clear();
    app->enemy = EnemyState{};
    app->use3DView = true;
    app->camera.position = {0.0F, 34.0F, 30.0F};
    app->camera.target = {0.0F, 0.0F, 0.0F};
    app->camera.up = {0.0F, 1.0F, 0.0F};
    app->camera.fovy = 45.0F;
    app->camera.projection = CAMERA_PERSPECTIVE;
    app->statusMessage =
        "LMB/SPACE base attack, RMB/Q hatsu, hold R to recover aura. TAB toggles 2D/3D.";
}

void EnsureCharacterHasHatsu(nen::Character *character) {
    if (character == nullptr) {
        return;
    }
    if (character->hatsuName.empty() || character->hatsuName == "Unnamed Hatsu") {
        character->hatsuName = nen::GenerateHatsuName(character->name, character->naturalType);
    }
    if (character->hatsuPotency < 80 || character->hatsuPotency > 150) {
        character->hatsuPotency = nen::GenerateHatsuPotency(character->name);
    }
}

void UpdateMainMenu(AppState *app, bool *running) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        *running = false;
        return;
    }

    const Rectangle newButton{86.0F, 262.0F, 420.0F, 74.0F};
    const Rectangle loadButton{86.0F, 354.0F, 420.0F, 74.0F};

    if (IsInside(newButton, GetMousePosition())) {
        app->menuSelection = 0;
    } else if (IsInside(loadButton, GetMousePosition())) {
        app->menuSelection = 1;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        app->menuSelection = std::max(0, app->menuSelection - 1);
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        app->menuSelection = std::min(1, app->menuSelection + 1);
    }

    const bool chooseNew =
        IsButtonTriggered(newButton) || (app->menuSelection == 0 && IsKeyPressed(KEY_ENTER));
    const bool chooseLoad =
        IsButtonTriggered(loadButton) || (app->menuSelection == 1 && IsKeyPressed(KEY_ENTER));

    if (chooseNew) {
        app->screen = Screen::NameEntry;
        app->draftName.clear();
        app->statusMessage = "Enter a name, then continue.";
        return;
    }
    if (chooseLoad) {
        app->screen = Screen::LoadCharacter;
        RefreshStoredCharacters(app);
        app->statusMessage =
            app->storedCharacters.empty() ? "No saves found yet." : "Choose a saved character.";
    }
}

void UpdateNameEntry(AppState *app) {
    HandleNameInput(&app->draftName);

    const Rectangle continueButton{86.0F, 346.0F, 300.0F, 62.0F};
    const Rectangle backButton{402.0F, 346.0F, 184.0F, 62.0F};

    if (IsKeyPressed(KEY_ESCAPE) || IsButtonTriggered(backButton)) {
        app->screen = Screen::MainMenu;
        return;
    }

    const bool continuePressed = IsButtonTriggered(continueButton) || IsKeyPressed(KEY_ENTER);
    if (!continuePressed) {
        return;
    }
    if (app->draftName.empty()) {
        app->statusMessage = "Name cannot be empty.";
        return;
    }

    app->quizScores.fill(0);
    app->quizQuestionIndex = 0;
    app->screen = Screen::Quiz;
    app->statusMessage = "Answer with 1/2/3 or click.";
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

    int clickedIndex = -1;
    int y = 210;
    for (std::size_t i = 0; i < app->storedCharacters.size(); ++i) {
        const Rectangle row{80.0F, static_cast<float>(y), 640.0F, 40.0F};
        if (IsInside(row, GetMousePosition())) {
            app->loadSelection = static_cast<int>(i);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                clickedIndex = static_cast<int>(i);
            }
        }
        y += 48;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        clickedIndex = app->loadSelection;
    }

    if (clickedIndex >= 0) {
        EnsureCharacterHasHatsu(
            &app->storedCharacters[static_cast<std::size_t>(clickedIndex)].character);
        StartWorld(app, app->storedCharacters[static_cast<std::size_t>(clickedIndex)].character);
        app->statusMessage = "Loaded " + app->player.name + ".";
    }
}

void UpdateQuiz(AppState *app) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        app->screen = Screen::MainMenu;
        return;
    }

    const auto &questions = nen::PersonalityQuestions();
    const auto &question = questions[app->quizQuestionIndex];

    std::array<Rectangle, 3> optionRects{{
        {86.0F, 312.0F, 920.0F, 62.0F},
        {86.0F, 392.0F, 920.0F, 62.0F},
        {86.0F, 472.0F, 920.0F, 62.0F},
    }};

    int answerIndex = -1;
    if (IsKeyPressed(KEY_ONE)) {
        answerIndex = 0;
    } else if (IsKeyPressed(KEY_TWO)) {
        answerIndex = 1;
    } else if (IsKeyPressed(KEY_THREE)) {
        answerIndex = 2;
    } else {
        for (int i = 0; i < 3; ++i) {
            if (IsButtonTriggered(optionRects[static_cast<std::size_t>(i)])) {
                answerIndex = i;
                break;
            }
        }
    }

    if (answerIndex < 0) {
        return;
    }

    nen::ApplyQuizAnswer(&app->quizScores, question.options[static_cast<std::size_t>(answerIndex)]);
    app->quizQuestionIndex += 1;
    if (app->quizQuestionIndex < questions.size()) {
        return;
    }

    app->player = nen::Character{
        .name = app->draftName,
        .naturalType = nen::DetermineNenType(app->quizScores),
        .auraPool = 180,
    };
    EnsureCharacterHasHatsu(&app->player);
    app->hasCharacter = true;
    app->revealTimer = 0.0F;
    app->screen = Screen::Reveal;
    app->statusMessage = "Divination complete. Continue to world.";
}

void UpdateReveal(AppState *app, float dt) {
    app->revealTimer += dt;
    const Rectangle continueButton{86.0F, 716.0F, 340.0F, 66.0F};
    if (app->revealTimer < 1.1F) {
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsButtonTriggered(continueButton)) {
        StartWorld(app, app->player);
        SaveCurrentCharacter(app);
        RefreshStoredCharacters(app);
    }
}

Vector2 EnemyCenter(const AppState &app) { return app.enemy.position; }

void MoveEnemy(AppState *app, float dt) {
    EnemyState &enemy = app->enemy;
    enemy.hitFlashTimer = std::max(0.0F, enemy.hitFlashTimer - dt);
    enemy.manipulatedTimer = std::max(0.0F, enemy.manipulatedTimer - dt);
    enemy.vulnerabilityTimer = std::max(0.0F, enemy.vulnerabilityTimer - dt);

    if (enemy.manipulatedTimer > 0.0F) {
        const Vector2 toPlayer{app->playerPosition.x - enemy.position.x,
                               app->playerPosition.y - enemy.position.y};
        const float distance = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
        Vector2 dir = {0.0F, 0.0F};
        if (distance > 0.001F) {
            dir = {toPlayer.x / distance, toPlayer.y / distance};
        }
        enemy.velocity.x *= 0.92F;
        enemy.velocity.y *= 0.92F;
        enemy.position.x += dir.x * 105.0F * dt;
        enemy.position.y += dir.y * 105.0F * dt;
    } else {
        enemy.position.x += enemy.velocity.x * dt;
        enemy.position.y += enemy.velocity.y * dt;
    }

    const float left = kArenaBounds.x + enemy.radius;
    const float right = kArenaBounds.x + kArenaBounds.width - enemy.radius;
    const float top = kArenaBounds.y + enemy.radius;
    const float bottom = kArenaBounds.y + kArenaBounds.height - enemy.radius;

    if (enemy.position.x < left) {
        enemy.position.x = left;
        enemy.velocity.x = std::abs(enemy.velocity.x);
    }
    if (enemy.position.x > right) {
        enemy.position.x = right;
        enemy.velocity.x = -std::abs(enemy.velocity.x);
    }
    if (enemy.position.y < top) {
        enemy.position.y = top;
        enemy.velocity.y = std::abs(enemy.velocity.y);
    }
    if (enemy.position.y > bottom) {
        enemy.position.y = bottom;
        enemy.velocity.y = -std::abs(enemy.velocity.y);
    }
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

Vector2 Normalize2D(Vector2 vector) {
    const float len = std::sqrt(vector.x * vector.x + vector.y * vector.y);
    if (len <= 0.0001F) {
        return {1.0F, 0.0F};
    }
    return {vector.x / len, vector.y / len};
}

void UpdateCombatCamera(AppState *app, float dt) {
    if (!app->use3DView) {
        return;
    }

    const Vector2 playerCenter{
        app->playerPosition.x + app->playerSize.x * 0.5F,
        app->playerPosition.y + app->playerSize.y * 0.5F,
    };
    const Vector2 focus2D{
        (playerCenter.x + app->enemy.position.x) * 0.5F,
        (playerCenter.y + app->enemy.position.y) * 0.5F,
    };
    const Vector3 focus = ArenaToWorld(focus2D, 0.7F);
    const Vector3 desiredPos{focus.x + 9.5F, 13.5F, focus.z + 14.5F};

    const float follow = std::clamp(dt * 4.0F, 0.01F, 0.25F);
    app->camera.target.x = Lerp(app->camera.target.x, focus.x, follow);
    app->camera.target.y = Lerp(app->camera.target.y, focus.y, follow);
    app->camera.target.z = Lerp(app->camera.target.z, focus.z, follow);

    app->camera.position.x = Lerp(app->camera.position.x, desiredPos.x, follow);
    app->camera.position.y = Lerp(app->camera.position.y, desiredPos.y, follow);
    app->camera.position.z = Lerp(app->camera.position.z, desiredPos.z, follow);
}

void ClampPlayerToArena(AppState *app) {
    const float left = kArenaBounds.x + 8.0F;
    const float right = kArenaBounds.x + kArenaBounds.width - app->playerSize.x - 8.0F;
    const float top = kArenaBounds.y + 8.0F;
    const float bottom = kArenaBounds.y + kArenaBounds.height - app->playerSize.y - 8.0F;
    app->playerPosition.x = std::clamp(app->playerPosition.x, left, right);
    app->playerPosition.y = std::clamp(app->playerPosition.y, top, bottom);
}

void UpdatePlayerMovement(AppState *app, float dt) {
    if (app->chargingAura) {
        return;
    }

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

    const float speed = 320.0F;
    app->playerPosition.x += movement.x * speed * dt;
    app->playerPosition.y += movement.y * speed * dt;
    ClampPlayerToArena(app);
}

void RechargeAura(AppState *app, float dt) {
    const bool holdRecharge = IsKeyDown(KEY_R) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    app->chargingAura = holdRecharge;
    if (!holdRecharge) {
        return;
    }

    app->chargeEffectTimer += dt;
    const float typeBonus = app->player.naturalType == nen::Type::Enhancer ? 8.0F : 0.0F;
    const int gain = static_cast<int>(std::floor((39.0F + typeBonus) * dt));
    app->player.auraPool = std::min(kAuraMax, app->player.auraPool + std::max(1, gain));
    app->statusMessage = "Channeling aura... hold R to recover.";
}

void UpdateBaseTypeSelection(AppState *app) {
    if (IsKeyPressed(KEY_ONE)) {
        app->selectedBaseType = nen::Type::Enhancer;
    } else if (IsKeyPressed(KEY_TWO)) {
        app->selectedBaseType = nen::Type::Transmuter;
    } else if (IsKeyPressed(KEY_THREE)) {
        app->selectedBaseType = nen::Type::Emitter;
    } else if (IsKeyPressed(KEY_FOUR)) {
        app->selectedBaseType = nen::Type::Conjurer;
    } else if (IsKeyPressed(KEY_FIVE)) {
        app->selectedBaseType = nen::Type::Manipulator;
    } else if (IsKeyPressed(KEY_SIX)) {
        app->selectedBaseType = nen::Type::Specialist;
    }
}

void CastBaseAttack(AppState *app, Vector2 target) {
    if (app->baseAttackCooldown > 0.0F) {
        app->statusMessage = "Base attack recharging.";
        return;
    }
    if (!nen::TryConsumeAura(&app->player, kBaseAttackAuraCost)) {
        app->statusMessage = "Not enough aura for base attack.";
        return;
    }

    const Vector2 origin{app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.38F};
    const int damage =
        nen::ComputeAttackDamage(app->player.naturalType, app->selectedBaseType, kBaseAttackPower);
    SpawnBaseAttack(&app->activeAttacks, app->selectedBaseType, origin, target, damage);
    app->baseAttackCooldown = 0.24F;
    app->statusMessage =
        "Base aura attack cast: " + std::string(nen::ToString(app->selectedBaseType)) + ".";
}

void CastHatsu(AppState *app, Vector2 target) {
    if (app->hatsuCooldown > 0.0F) {
        app->statusMessage = "Hatsu still cooling down.";
        return;
    }
    if (!nen::TryConsumeAura(&app->player, kHatsuAuraCost)) {
        app->statusMessage = "Not enough aura for hatsu.";
        return;
    }

    const Vector2 origin{app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.3F};
    const int potency = std::max(70, app->player.hatsuPotency);
    const int damage = static_cast<int>(
        std::round(nen::ComputeAttackDamage(app->player.naturalType, app->player.naturalType,
                                            kHatsuAttackPower) *
                   (static_cast<float>(potency) / 100.0F)));

    SpawnHatsuAttack(&app->activeAttacks, app->player.naturalType, origin, target, damage);
    app->hatsuCooldown = 2.8F;
    app->statusMessage = "Hatsu activated: " + app->player.hatsuName;
}

void ApplyAttackOutcome(AppState *app, const AttackOutcome &outcome) {
    if (outcome.damage <= 0 && outcome.manipulationSeconds <= 0.0F &&
        outcome.vulnerabilitySeconds <= 0.0F) {
        return;
    }

    if (outcome.manipulationSeconds > 0.0F) {
        app->enemy.manipulatedTimer =
            std::max(app->enemy.manipulatedTimer, outcome.manipulationSeconds);
        app->statusMessage = "Manipulator control effect applied.";
    }
    if (outcome.vulnerabilitySeconds > 0.0F) {
        app->enemy.vulnerabilityTimer =
            std::max(app->enemy.vulnerabilityTimer, outcome.vulnerabilitySeconds);
    }

    int damage = outcome.damage;
    if (app->enemy.vulnerabilityTimer > 0.0F) {
        damage = static_cast<int>(std::round(static_cast<float>(damage) * 1.35F));
    }
    damage = std::max(0, damage);
    if (damage <= 0) {
        return;
    }

    app->enemy.health = std::max(0, app->enemy.health - damage);
    app->enemy.hitFlashTimer = 0.12F;
    app->statusMessage = "Hit for " + std::to_string(damage) + " damage.";

    if (app->enemy.health > 0) {
        SaveCurrentCharacter(app);
        return;
    }

    app->enemy.health = app->enemy.maxHealth;
    app->enemy.position = {kArenaBounds.x + kArenaBounds.width - 160.0F,
                           kArenaBounds.y + 160.0F + static_cast<float>(GetRandomValue(0, 220))};
    app->enemy.velocity = {static_cast<float>(GetRandomValue(-220, -140)),
                           static_cast<float>(GetRandomValue(120, 210))};
    app->player.auraPool = std::min(kAuraMax, app->player.auraPool + 68);
    app->statusMessage = "Enemy overwhelmed. +68 aura.";
    SaveCurrentCharacter(app);
}

void UpdateWorld(AppState *app) {
    const float dt = GetFrameTime();

    if (IsKeyPressed(KEY_ESCAPE)) {
        SaveCurrentCharacter(app);
        app->screen = Screen::MainMenu;
        app->statusMessage = "Returned to main menu.";
        return;
    }

    app->baseAttackCooldown = std::max(0.0F, app->baseAttackCooldown - dt);
    app->hatsuCooldown = std::max(0.0F, app->hatsuCooldown - dt);
    if (IsKeyPressed(KEY_TAB)) {
        app->use3DView = !app->use3DView;
        app->statusMessage = app->use3DView ? "3D view enabled." : "2D view enabled.";
    }

    UpdateBaseTypeSelection(app);

    RechargeAura(app, dt);
    UpdatePlayerMovement(app, dt);
    MoveEnemy(app, dt);
    UpdateCombatCamera(app, dt);

    Vector2 target = EnemyCenter(*app);
    if (app->use3DView) {
        Vector2 arenaPoint;
        if (MouseToArenaPoint(app->camera, &arenaPoint)) {
            target = arenaPoint;
        }
    } else {
        const Vector2 cursor = GetMousePosition();
        target = IsInside(kArenaBounds, cursor) ? cursor : EnemyCenter(*app);
    }

    const bool baseCast = IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool hatsuCast = IsKeyPressed(KEY_Q) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (baseCast && !app->chargingAura) {
        CastBaseAttack(app, target);
    }
    if (hatsuCast && !app->chargingAura) {
        CastHatsu(app, target);
    }

    const AttackOutcome outcome =
        UpdateAttackEffects(&app->activeAttacks, dt, EnemyCenter(*app), app->enemy.radius);
    ApplyAttackOutcome(app, outcome);

    if (IsKeyPressed(KEY_F5)) {
        SaveCurrentCharacter(app);
        RefreshStoredCharacters(app);
        app->statusMessage = "Character saved.";
    }
}

void DrawMainMenu(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {11, 18, 34, 255});
    DrawText("Nen World", 86, 70, 64, RAYWHITE);
    DrawText("Create or load a hunter profile", 86, 148, 30, LIGHTGRAY);

    const Rectangle newButton{86.0F, 262.0F, 420.0F, 74.0F};
    const Rectangle loadButton{86.0F, 354.0F, 420.0F, 74.0F};
    DrawButton(newButton, "New Character", app.menuSelection == 0);
    DrawButton(loadButton, "Load Character", app.menuSelection == 1);

    DrawText("Keyboard or mouse both supported", 86, 456, 22, Fade(WHITE, 0.82F));
    DrawText("ESC to quit", 86, 492, 20, Fade(WHITE, 0.7F));
}

void DrawNameEntry(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});
    DrawText("Create New Hunter", 86, 70, 56, RAYWHITE);
    DrawText("Enter character name", 86, 150, 30, LIGHTGRAY);

    DrawRectangleRounded({86.0F, 234.0F, 500.0F, 68.0F}, 0.2F, 8, {24, 38, 64, 255});
    DrawRectangleRoundedLinesEx({86.0F, 234.0F, 500.0F, 68.0F}, 0.2F, 8, 2.0F, WHITE);
    DrawText(app.draftName.c_str(), 106, 254, 34, RAYWHITE);

    DrawButton({86.0F, 346.0F, 300.0F, 62.0F}, "Continue", false);
    DrawButton({402.0F, 346.0F, 184.0F, 62.0F}, "Back", false);
}

void DrawLoadCharacter(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});
    DrawText("Load Character", 80, 70, 56, RAYWHITE);

    if (app.storedCharacters.empty()) {
        DrawText("No saved characters found.", 80, 210, 30, LIGHTGRAY);
        DrawText("Press ESC to return.", 80, 252, 24, LIGHTGRAY);
        return;
    }

    int y = 210;
    for (std::size_t i = 0; i < app.storedCharacters.size(); ++i) {
        const bool selected = static_cast<int>(i) == app.loadSelection;
        const Rectangle row{80.0F, static_cast<float>(y), 640.0F, 40.0F};
        DrawRectangleRounded(row, 0.2F, 6,
                             selected ? Color{48, 74, 124, 255} : Color{27, 43, 72, 255});
        DrawRectangleRoundedLinesEx(row, 0.2F, 6, 2.0F, selected ? WHITE : Fade(WHITE, 0.45F));

        const auto &ch = app.storedCharacters[i].character;
        const std::string line = ch.name + " | " + std::string(nen::ToString(ch.naturalType)) +
                                 " | aura " + std::to_string(ch.auraPool);
        DrawText(line.c_str(), 96, y + 8, 24, RAYWHITE);
        y += 48;
    }
}

void DrawQuiz(const AppState &app) {
    const auto &questions = nen::PersonalityQuestions();
    const auto &question = questions[app.quizQuestionIndex];
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});

    DrawText("Nen Personality Assessment", 86, 66, 52, RAYWHITE);
    DrawText(TextFormat("Question %d / %d", static_cast<int>(app.quizQuestionIndex + 1),
                        static_cast<int>(questions.size())),
             86, 132, 28, LIGHTGRAY);

    DrawRectangleRounded({86.0F, 200.0F, 960.0F, 82.0F}, 0.15F, 6, {24, 38, 64, 255});
    DrawText(question.prompt.data(), 110, 230, 30, RAYWHITE);

    std::array<Rectangle, 3> optionRects{{
        {86.0F, 312.0F, 920.0F, 62.0F},
        {86.0F, 392.0F, 920.0F, 62.0F},
        {86.0F, 472.0F, 920.0F, 62.0F},
    }};

    for (int i = 0; i < 3; ++i) {
        DrawButton(optionRects[static_cast<std::size_t>(i)],
                   std::to_string(i + 1) + ". " +
                       std::string(question.options[static_cast<std::size_t>(i)].text),
                   false);
    }
}

void DrawReveal(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {20, 29, 50, 255}, {12, 19, 36, 255});
    DrawText("Water Divination", 86, 66, 54, RAYWHITE);
    DrawText(TextFormat("%s is a %s!", app.player.name.c_str(),
                        nen::ToString(app.player.naturalType).data()),
             86, 160, 48, WHITE);
    DrawText(nen::WaterDivinationEffect(app.player.naturalType).data(), 86, 232, 30, LIGHTGRAY);
    DrawText(TextFormat("Unique Hatsu: %s", app.player.hatsuName.c_str()), 86, 286, 30, RAYWHITE);
    DrawText(
        TextFormat("Special Ability: %s", nen::HatsuAbilityName(app.player.naturalType).data()), 86,
        330, 28, LIGHTGRAY);
    DrawText(nen::HatsuAbilityDescription(app.player.naturalType).data(), 86, 366, 24, LIGHTGRAY);

    const Rectangle glass{980.0F, 166.0F, 280.0F, 430.0F};
    DrawRectangleRounded(glass, 0.06F, 8, Fade({51, 80, 132, 255}, 0.44F));
    DrawRectangleRoundedLinesEx(glass, 0.06F, 8, 4.0F, WHITE);

    float waterLevel = 190.0F;
    Color waterBase = {73, 136, 236, 255};
    Color glow = TypeColor(app.player.naturalType);

    switch (app.player.naturalType) {
    case nen::Type::Enhancer:
        waterLevel = 190.0F + std::min(185.0F, app.revealTimer * 78.0F);
        break;
    case nen::Type::Transmuter:
        waterBase = {98, 216, 255, 255};
        break;
    case nen::Type::Emitter:
        waterBase = {150, 134, 252, 255};
        break;
    case nen::Type::Conjurer:
        waterBase = {111, 149, 238, 255};
        break;
    case nen::Type::Manipulator:
        waterBase = {101, 170, 228, 255};
        break;
    case nen::Type::Specialist:
        waterBase = {166, 104, 234, 255};
        break;
    }

    const Rectangle water{
        glass.x + 16.0F,
        glass.y + glass.height - waterLevel - 16.0F,
        glass.width - 32.0F,
        waterLevel,
    };
    DrawRectangleRounded(water, 0.08F, 8, waterBase);
    DrawRectangleRounded(water, 0.08F, 8, Fade(glow, 0.2F));

    for (int i = 0; i < 26; ++i) {
        const float t = static_cast<float>(i) / 25.0F;
        const float x = water.x + t * water.width;
        const float y = water.y + std::sin(t * 9.0F + app.revealTimer * 5.0F) * 5.5F;
        DrawCircle(static_cast<int>(x), static_cast<int>(y), 2.0F, Fade(WHITE, 0.85F));
    }

    DrawRectangle(
        static_cast<int>(glass.x + glass.width * 0.5F + std::sin(app.revealTimer * 2.6F) * 36.0F),
        static_cast<int>(glass.y + 106.0F), 36, 12, {198, 230, 169, 255});

    DrawButton({86.0F, 716.0F, 340.0F, 66.0F}, "Continue", false);
}

void DrawPlayer(const AppState &app) {
    const Vector2 base{app.playerPosition.x, app.playerPosition.y};
    const Color auraColor = TypeColor(app.player.naturalType);

    if (app.chargingAura) {
        const float t = app.chargeEffectTimer;
        DrawCircleLines(base.x + 18.0F, base.y + 24.0F, 30.0F + std::sin(t * 5.5F) * 5.0F, WHITE);
        DrawCircleLines(base.x + 18.0F, base.y + 24.0F, 42.0F + std::sin(t * 4.2F) * 5.0F,
                        Fade(WHITE, 0.8F));
    }

    DrawCircle(base.x + 18.0F, base.y + 10.0F, 9.0F, {236, 226, 205, 255});
    DrawRectangleRounded({base.x + 8.0F, base.y + 20.0F, 20.0F, 24.0F}, 0.3F, 6,
                         {49, 75, 124, 255});
    DrawLineEx({base.x + 8.0F, base.y + 28.0F}, {base.x - 1.0F, base.y + 40.0F}, 4.0F,
               {228, 219, 204, 255});
    DrawLineEx({base.x + 28.0F, base.y + 28.0F}, {base.x + 37.0F, base.y + 40.0F}, 4.0F,
               {228, 219, 204, 255});
    DrawRectangle(base.x + 9.0F, base.y + 44.0F, 7.0F, 10.0F, {218, 210, 194, 255});
    DrawRectangle(base.x + 20.0F, base.y + 44.0F, 7.0F, 10.0F, {218, 210, 194, 255});
    DrawCircleLines(base.x + 18.0F, base.y + 26.0F, 25.0F, Fade(auraColor, 0.65F));
}

void DrawEnemy(const AppState &app) {
    const EnemyState &enemy = app.enemy;
    const Color coreColor =
        enemy.hitFlashTimer > 0.0F ? Color{255, 236, 178, 255} : Color{216, 105, 102, 255};
    DrawCircleV(enemy.position, enemy.radius, {70, 37, 37, 255});
    DrawCircleV(enemy.position, enemy.radius * 0.75F, coreColor);
    DrawCircleLinesV(enemy.position, enemy.radius + 2.0F, Fade(WHITE, 0.85F));

    if (enemy.manipulatedTimer > 0.0F) {
        DrawCircleLines(enemy.position.x, enemy.position.y, enemy.radius + 10.0F,
                        {255, 207, 108, 255});
        DrawLineBezier({enemy.position.x - enemy.radius - 24.0F, enemy.position.y - 30.0F},
                       {enemy.position.x + enemy.radius + 24.0F, enemy.position.y + 30.0F}, 2.6F,
                       {255, 207, 108, 255});
    }
    if (enemy.vulnerabilityTimer > 0.0F) {
        DrawCircleLines(enemy.position.x, enemy.position.y, enemy.radius + 16.0F,
                        {255, 120, 205, 255});
    }
}

void DrawArena3D(const AppState &app) {
    const Vector3 center = ArenaToWorld(
        {kArenaBounds.x + kArenaBounds.width * 0.5F, kArenaBounds.y + kArenaBounds.height * 0.5F});
    const float width = kArenaBounds.width * kWorldScale;
    const float depth = kArenaBounds.height * kWorldScale;
    DrawPlane(center, {width, depth}, {26, 39, 66, 255});

    const float wallH = 1.2F;
    const float wallT = 0.2F;
    DrawCube({center.x, wallH * 0.5F, center.z - depth * 0.5F}, width, wallH, wallT,
             {32, 49, 81, 255});
    DrawCube({center.x, wallH * 0.5F, center.z + depth * 0.5F}, width, wallH, wallT,
             {32, 49, 81, 255});
    DrawCube({center.x - width * 0.5F, wallH * 0.5F, center.z}, wallT, wallH, depth,
             {32, 49, 81, 255});
    DrawCube({center.x + width * 0.5F, wallH * 0.5F, center.z}, wallT, wallH, depth,
             {32, 49, 81, 255});
    DrawCubeWires({center.x, wallH * 0.5F, center.z}, width, wallH, depth, Fade(WHITE, 0.35F));

    for (int i = 0; i < 11; ++i) {
        const float t = static_cast<float>(i) / 10.0F;
        const float z = center.z - depth * 0.5F + t * depth;
        DrawLine3D({center.x - width * 0.5F + 0.2F, 0.02F, z},
                   {center.x + width * 0.5F - 0.2F, 0.02F, z}, Fade(WHITE, 0.1F));
    }
    for (int i = 0; i < 13; ++i) {
        const float t = static_cast<float>(i) / 12.0F;
        const float x = center.x - width * 0.5F + t * width;
        DrawLine3D({x, 0.02F, center.z - depth * 0.5F + 0.2F},
                   {x, 0.02F, center.z + depth * 0.5F - 0.2F}, Fade(WHITE, 0.08F));
    }

    if (app.chargingAura) {
        const float pulse = 0.18F + std::sin(app.chargeEffectTimer * 5.0F) * 0.05F;
        DrawSphereWires({center.x, 0.2F, center.z}, pulse, 8, 8, Fade(WHITE, 0.4F));
    }
}

void DrawAttackEffects3D(const AppState &app) {
    for (const auto &effect : app.activeAttacks) {
        const Vector3 pos = ArenaToWorld(effect.position, 0.55F + effect.radius * 0.004F);
        const float radius = std::max(0.08F, effect.radius * kWorldScale * 0.65F);
        Color color = TypeColor(effect.type);
        color.a = 220;

        switch (effect.type) {
        case nen::Type::Enhancer:
            DrawSphere(pos, radius * 1.15F, color);
            DrawCircle3D(pos, radius * 1.9F, {1.0F, 0.0F, 0.0F}, 90.0F, Fade(color, 0.65F));
            break;
        case nen::Type::Transmuter: {
            const Vector2 dir2D = Normalize2D(effect.velocity);
            const Vector3 tail = ArenaToWorld(
                {effect.position.x - dir2D.x * 24.0F, effect.position.y - dir2D.y * 24.0F},
                pos.y - 0.08F);
            DrawLine3D(tail, pos, color);
            DrawSphere(pos, radius * 0.92F, color);
            const Vector3 boltA{pos.x + std::sin(effect.phase * 5.0F) * 0.22F, pos.y + 0.25F,
                                pos.z};
            const Vector3 boltB{pos.x - std::sin(effect.phase * 5.0F) * 0.22F, pos.y - 0.18F,
                                pos.z};
            DrawLine3D(boltA, boltB, Fade(color, 0.95F));
            break;
        }
        case nen::Type::Emitter: {
            const Vector2 dir2D = Normalize2D(effect.velocity);
            const Vector3 start = ArenaToWorld(
                {effect.position.x - dir2D.x * 28.0F, effect.position.y - dir2D.y * 28.0F}, pos.y);
            DrawCylinderEx(start, pos, radius * 0.55F, radius * 0.9F, 10, Fade(color, 0.9F));
            DrawSphere(pos, radius * 0.82F, {255, 240, 220, 255});
            break;
        }
        case nen::Type::Conjurer:
            DrawCubeV(pos, {radius * 1.8F, radius * 1.8F, radius * 1.8F}, Fade(color, 0.9F));
            DrawCubeWiresV(pos, {radius * 2.0F, radius * 2.0F, radius * 2.0F}, WHITE);
            break;
        case nen::Type::Manipulator: {
            DrawSphere(pos, radius, color);
            const float orbit = radius * 2.2F;
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
            DrawLine3D({pos.x - radius * 1.6F, pos.y, pos.z}, {pos.x + radius * 1.6F, pos.y, pos.z},
                       Fade(WHITE, 0.9F));
            DrawLine3D({pos.x, pos.y - radius * 1.6F, pos.z}, {pos.x, pos.y + radius * 1.6F, pos.z},
                       Fade(WHITE, 0.9F));
            DrawLine3D({pos.x, pos.y, pos.z - radius * 1.6F}, {pos.x, pos.y, pos.z + radius * 1.6F},
                       Fade(WHITE, 0.9F));
            break;
        }
    }
}

void DrawEnemy3D(const AppState &app) {
    const EnemyState &enemy = app.enemy;
    const Vector3 center = ArenaToWorld(enemy.position, 0.95F);
    const float radius = enemy.radius * kWorldScale;
    const Color coreColor =
        enemy.hitFlashTimer > 0.0F ? Color{255, 236, 178, 255} : Color{216, 105, 102, 255};

    DrawSphere(center, radius, {70, 37, 37, 255});
    DrawSphere({center.x, center.y + radius * 0.25F, center.z}, radius * 0.72F, coreColor);
    DrawSphereWires(center, radius + 0.03F, 10, 10, Fade(WHITE, 0.75F));

    if (enemy.manipulatedTimer > 0.0F) {
        DrawCircle3D({center.x, center.y + 0.02F, center.z}, radius + 0.18F, {1.0F, 0.0F, 0.0F},
                     90.0F, {255, 207, 108, 255});
    }
    if (enemy.vulnerabilityTimer > 0.0F) {
        DrawCircle3D({center.x, center.y + 0.05F, center.z}, radius + 0.24F, {1.0F, 0.0F, 0.0F},
                     90.0F, {255, 120, 205, 255});
    }
}

void DrawPlayer3D(const AppState &app) {
    const Vector2 p2 = {app.playerPosition.x + app.playerSize.x * 0.5F,
                        app.playerPosition.y + app.playerSize.y * 0.5F};
    const Vector3 pos = ArenaToWorld(p2, 0.75F);
    const Color aura = TypeColor(app.player.naturalType);

    if (app.hasPlayerModel) {
        const Vector2 toEnemy{app.enemy.position.x - p2.x, app.enemy.position.y - p2.y};
        const float yawDegrees =
            std::atan2(toEnemy.x, toEnemy.y) * RAD2DEG + app.playerModelYawOffset;
        const Vector3 drawPos = ArenaToWorld(p2, app.playerModelYOffset);
        DrawModelEx(app.playerModel, drawPos, {0.0F, 1.0F, 0.0F}, yawDegrees,
                    {app.playerModelScale, app.playerModelScale, app.playerModelScale}, WHITE);
    } else {
        DrawCube({pos.x, pos.y + 0.22F, pos.z}, 0.38F, 0.46F, 0.24F, {48, 74, 123, 255});
        DrawCube({pos.x - 0.24F, pos.y + 0.2F, pos.z}, 0.1F, 0.34F, 0.1F, {230, 220, 201, 255});
        DrawCube({pos.x + 0.24F, pos.y + 0.2F, pos.z}, 0.1F, 0.34F, 0.1F, {230, 220, 201, 255});
        DrawCube({pos.x - 0.1F, pos.y - 0.18F, pos.z}, 0.1F, 0.34F, 0.1F, {220, 211, 196, 255});
        DrawCube({pos.x + 0.1F, pos.y - 0.18F, pos.z}, 0.1F, 0.34F, 0.1F, {220, 211, 196, 255});
        DrawSphere({pos.x, pos.y + 0.57F, pos.z}, 0.13F, {236, 226, 205, 255});
    }

    DrawCircle3D({pos.x, pos.y + 0.22F, pos.z}, 0.4F, {1.0F, 0.0F, 0.0F}, 90.0F, Fade(aura, 0.8F));
    if (app.chargingAura) {
        const float pulse = 0.52F + std::sin(app.chargeEffectTimer * 4.8F) * 0.06F;
        DrawCircle3D({pos.x, pos.y + 0.22F, pos.z}, pulse, {1.0F, 0.0F, 0.0F}, 90.0F, WHITE);
    }
}

void DrawWorld(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {15, 24, 41, 255}, {8, 14, 27, 255});

    if (app.use3DView) {
        BeginMode3D(app.camera);
        DrawArena3D(app);
        DrawAttackEffects3D(app);
        DrawEnemy3D(app);
        DrawPlayer3D(app);
        EndMode3D();
        DrawText("3D Mode (TAB to toggle 2D)", static_cast<int>(kArenaBounds.x),
                 static_cast<int>(kArenaBounds.y - 34.0F), 24, Fade(WHITE, 0.9F));
    } else {
        DrawRectangleRounded(kArenaBounds, 0.02F, 6, {18, 30, 50, 255});
        DrawRectangleRoundedLinesEx(kArenaBounds, 0.02F, 6, 3.0F, Fade(WHITE, 0.65F));

        for (int x = 0; x < 20; ++x) {
            const float px = kArenaBounds.x + 14.0F + static_cast<float>(x) * 50.0F;
            DrawLine(px, kArenaBounds.y + 10.0F, px, kArenaBounds.y + kArenaBounds.height - 10.0F,
                     Fade(WHITE, 0.06F));
        }
        for (int y = 0; y < 14; ++y) {
            const float py = kArenaBounds.y + 14.0F + static_cast<float>(y) * 50.0F;
            DrawLine(kArenaBounds.x + 10.0F, py, kArenaBounds.x + kArenaBounds.width - 10.0F, py,
                     Fade(WHITE, 0.06F));
        }

        DrawAttackEffects(app.activeAttacks);
        DrawEnemy(app);
        DrawPlayer(app);
        DrawText("2D Mode (TAB for 3D)", static_cast<int>(kArenaBounds.x),
                 static_cast<int>(kArenaBounds.y - 34.0F), 24, Fade(WHITE, 0.9F));
    }

    const int panelX = 1100;
    DrawRectangleRounded({static_cast<float>(panelX), 110.0F, 420.0F, 730.0F}, 0.03F, 6,
                         {22, 34, 56, 255});
    DrawRectangleRoundedLinesEx({static_cast<float>(panelX), 110.0F, 420.0F, 730.0F}, 0.03F, 6,
                                2.0F, Fade(WHITE, 0.6F));

    DrawText(TextFormat("Name: %s", app.player.name.c_str()), panelX + 20, 138, 28, RAYWHITE);
    DrawText(TextFormat("Type: %s", nen::ToString(app.player.naturalType).data()), panelX + 20, 174,
             28, RAYWHITE);
    DrawText(TextFormat("Aura: %d / %d", app.player.auraPool, kAuraMax), panelX + 20, 210, 28,
             RAYWHITE);
    DrawText(TextFormat("Hatsu: %s", app.player.hatsuName.c_str()), panelX + 20, 248, 24,
             LIGHTGRAY);
    DrawText(TextFormat("Hatsu Skill: %s", nen::HatsuAbilityName(app.player.naturalType).data()),
             panelX + 20, 278, 24, LIGHTGRAY);
    DrawText(TextFormat("Potency: %d", app.player.hatsuPotency), panelX + 20, 308, 24, LIGHTGRAY);
    DrawText(TextFormat("Base Type: %s", nen::ToString(app.selectedBaseType).data()), panelX + 20,
             338, 24, LIGHTGRAY);
    DrawText(TextFormat("Model: %s", app.hasPlayerModel ? "Loaded" : "Fallback"), panelX + 20, 368,
             24, app.hasPlayerModel ? Color{150, 234, 170, 255} : Color{250, 190, 128, 255});
    if (!app.hasPlayerModel) {
        DrawText(app.playerModelStatus.c_str(), panelX + 20, 392, 18, Fade(WHITE, 0.72F));
    }

    const int statY = app.hasPlayerModel ? 402 : 422;
    DrawText(TextFormat("Base CD: %.2fs", app.baseAttackCooldown), panelX + 20, statY, 24,
             LIGHTGRAY);
    DrawText(TextFormat("Hatsu CD: %.2fs", app.hatsuCooldown), panelX + 20, statY + 30, 24,
             LIGHTGRAY);
    DrawText(TextFormat("Enemy HP: %d / %d", app.enemy.health, app.enemy.maxHealth), panelX + 20,
             statY + 60, 24, LIGHTGRAY);

    if (app.enemy.manipulatedTimer > 0.0F) {
        DrawText(TextFormat("Manipulated: %.2fs", app.enemy.manipulatedTimer), panelX + 20,
                 statY + 92, 24, {255, 210, 120, 255});
    }
    if (app.enemy.vulnerabilityTimer > 0.0F) {
        DrawText(TextFormat("Vulnerable: %.2fs", app.enemy.vulnerabilityTimer), panelX + 20,
                 statY + 122, 24, {255, 121, 210, 255});
    }

    DrawText("Hatsu Description", panelX + 20, 590, 28, RAYWHITE);
    DrawText(nen::HatsuAbilityDescription(app.player.naturalType).data(), panelX + 20, 626, 20,
             LIGHTGRAY);

    DrawText("Controls", panelX + 20, 668, 28, RAYWHITE);
    DrawText("Move: WASD / Arrows", panelX + 20, 706, 22, LIGHTGRAY);
    DrawText("Base Attack: LMB or SPACE", panelX + 20, 734, 22, LIGHTGRAY);
    DrawText("Select Base Type: 1..6", panelX + 20, 762, 22, LIGHTGRAY);
    DrawText("Hatsu: RMB or Q | Recharge: Hold R", panelX + 20, 790, 22, LIGHTGRAY);
    DrawText("TAB 2D/3D | Save: F5 | Back: ESC", panelX + 20, 818, 22, LIGHTGRAY);
}

} // namespace

int Run() {
    InitWindow(kWidth, kHeight, "Nen World - 2D Prototype");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    AppState app;
    TryLoadPlayerModel(&app);
    RefreshStoredCharacters(&app);

    bool running = true;
    while (running && !WindowShouldClose()) {
        const float dt = GetFrameTime();
        (void)dt;

        switch (app.screen) {
        case Screen::MainMenu:
            UpdateMainMenu(&app, &running);
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
            UpdateWorld(&app);
            break;
        }

        BeginDrawing();
        ClearBackground({14, 21, 36, 255});

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
            DrawWorld(app);
            break;
        }

        DrawRectangle(0, kHeight - 44, kWidth, 44, {23, 35, 58, 255});
        DrawText(app.statusMessage.c_str(), 22, kHeight - 30, 22, {218, 223, 235, 255});

        EndDrawing();
    }

    SaveCurrentCharacter(&app);
    UnloadPlayerModel(&app);
    CloseWindow();
    return 0;
}

} // namespace game
