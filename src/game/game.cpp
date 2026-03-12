#include "game/game.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <filesystem>
#include <string>
#include <vector>

#include <raylib.h>

#include "game/app_state.hpp"
#include "game/attack_system.hpp"
#include "game/game_constants.hpp"
#include "game/particle_system.hpp"
#include "game/persistence.hpp"
#include "game/ui_renderer.hpp"
#include "game/world_renderer.hpp"
#include "nen/combat.hpp"
#include "nen/hatsu.hpp"
#include "nen/hatsu_spec.hpp"
#include "nen/log.hpp"
#include "nen/nen_system.hpp"
#include "nen/quiz.hpp"
#include "nen/types.hpp"

namespace game {

namespace {

// ── Private game-logic constants ──────────────────────────────────────────────

constexpr int   kBaseAttackAuraCost    = 16;
constexpr int   kHatsuAuraCost         = 52;
constexpr int   kBaseAttackPower       = 25;
constexpr int   kHatsuAttackPower      = 44;
constexpr float kPlayerModelTargetHeight = 1.18F;

// ── Helpers ───────────────────────────────────────────────────────────────────

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

Vector2 Normalize2D(Vector2 vector) {
    const float len = std::sqrt(vector.x * vector.x + vector.y * vector.y);
    if (len <= 0.0001F) {
        return {1.0F, 0.0F};
    }
    return {vector.x / len, vector.y / len};
}

Vector2 EnemyCenter(const AppState &app) { return app.enemy.position; }

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

// ── Input helpers ─────────────────────────────────────────────────────────────

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

// ── Persistence helpers ───────────────────────────────────────────────────────

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
        NEN_ERROR("save failed: %s", error.c_str());
    } else {
        NEN_DEBUG("character saved  name=%s  aura=%.0f",
                  app->player.name.c_str(), app->player.auraPool.current);
    }
}

// ── Model loading ─────────────────────────────────────────────────────────────

std::vector<std::filesystem::path> CandidateModelFiles() {
    return {
        std::filesystem::path("assets") / "models" / "hisoka_hxh.glb",
        std::filesystem::path("assets") / "models" / "hisoka.glb",
        std::filesystem::path("assets") / "models" / "killua.glb",
    };
}

std::string ResolveCharacterModelPath() {
    const auto modelFiles = CandidateModelFiles();
    for (const auto &relative : modelFiles) {
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
    }
    return {};
}

BoundingBox MergeBounds(const BoundingBox &a, const BoundingBox &b) {
    return {
        {std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z)},
        {std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z)},
    };
}

bool IsUsableMeshBounds(const BoundingBox &bounds, int vertexCount) {
    if (vertexCount < 3) {
        return false;
    }
    const float w = bounds.max.x - bounds.min.x;
    const float h = bounds.max.y - bounds.min.y;
    const float d = bounds.max.z - bounds.min.z;
    if (w <= 0.00001F || h <= 0.00001F || d <= 0.00001F) {
        return false;
    }
    if (!std::isfinite(w) || !std::isfinite(h) || !std::isfinite(d)) {
        return false;
    }
    const float longest = std::max({w, h, d});
    return longest < 1'000'000.0F;
}

std::string ToLowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

bool ContainsAnyTerm(std::string_view text, std::initializer_list<std::string_view> terms) {
    for (std::string_view term : terms) {
        if (text.find(term) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int FindAnimationIndex(const AppState &app, std::initializer_list<std::string_view> terms) {
    for (int i = 0; i < app.playerAnimationCount; ++i) {
        const std::string name = ToLowerAscii(app.playerAnimations[i].name);
        if (ContainsAnyTerm(name, terms)) {
            return i;
        }
    }
    return -1;
}

void ConfigureAnimationSet(AppState *app) {
    if (app == nullptr || app->playerAnimations == nullptr || app->playerAnimationCount <= 0) {
        return;
    }
    app->idleAnimationIndex   = FindAnimationIndex(*app, {"idle", "stand", "breath"});
    app->moveAnimationIndex   = FindAnimationIndex(*app, {"walk", "run", "move", "jog"});
    app->chargeAnimationIndex = FindAnimationIndex(*app, {"charge", "focus", "channel", "cast"});
    app->baseCastAnimationIndex =
        FindAnimationIndex(*app, {"attack", "punch", "kick", "slash", "shoot", "fire"});
    app->hatsuCastAnimationIndex =
        FindAnimationIndex(*app, {"skill", "special", "spell", "ultimate", "cast"});

    if (app->idleAnimationIndex < 0)      { app->idleAnimationIndex   = 0; }
    if (app->moveAnimationIndex < 0)      { app->moveAnimationIndex   = app->idleAnimationIndex; }
    if (app->chargeAnimationIndex < 0)    { app->chargeAnimationIndex = app->idleAnimationIndex; }
    if (app->baseCastAnimationIndex < 0)  { app->baseCastAnimationIndex  = app->moveAnimationIndex; }
    if (app->hatsuCastAnimationIndex < 0) { app->hatsuCastAnimationIndex = app->baseCastAnimationIndex; }
}

int AnimationIndexForState(const AppState &app, AnimState state) {
    switch (state) {
    case AnimState::Idle:      return app.idleAnimationIndex;
    case AnimState::Move:      return app.moveAnimationIndex;
    case AnimState::Charge:    return app.chargeAnimationIndex;
    case AnimState::CastBase:  return app.baseCastAnimationIndex;
    case AnimState::CastHatsu: return app.hatsuCastAnimationIndex;
    }
    return app.idleAnimationIndex;
}

void SetAnimationState(AppState *app, AnimState state) {
    if (app == nullptr || app->animationState == state) {
        return;
    }
    app->animationState       = state;
    app->activeAnimationFrame = 0.0F;
    app->activeAnimationIndex = -1;
}

void ResolveModelBounds(const Model &model, BoundingBox *outBounds, int *outSelectedMeshes) {
    if (outBounds == nullptr || outSelectedMeshes == nullptr) {
        return;
    }
    std::vector<std::pair<BoundingBox, int>> candidates;
    candidates.reserve(static_cast<std::size_t>(std::max(0, model.meshCount)));
    int maxVertexCount = 0;
    for (int i = 0; i < model.meshCount; ++i) {
        const BoundingBox meshBounds  = GetMeshBoundingBox(model.meshes[i]);
        const int         vertexCount = model.meshes[i].vertexCount;
        if (!IsUsableMeshBounds(meshBounds, vertexCount)) {
            continue;
        }
        candidates.push_back({meshBounds, vertexCount});
        maxVertexCount = std::max(maxVertexCount, vertexCount);
    }

    if (candidates.empty()) {
        *outBounds        = GetModelBoundingBox(model);
        *outSelectedMeshes = 0;
        return;
    }

    const int threshold     = std::max(6, maxVertexCount / 10);
    bool      hasMergedBounds = false;
    BoundingBox mergedBounds{};
    int         selectedMeshes = 0;
    for (const auto &[bounds, vertexCount] : candidates) {
        if (vertexCount < threshold) {
            continue;
        }
        if (!hasMergedBounds) {
            mergedBounds    = bounds;
            hasMergedBounds = true;
        } else {
            mergedBounds = MergeBounds(mergedBounds, bounds);
        }
        selectedMeshes += 1;
    }

    if (!hasMergedBounds) {
        for (const auto &[bounds, vertexCount] : candidates) {
            (void)vertexCount;
            if (!hasMergedBounds) {
                mergedBounds    = bounds;
                hasMergedBounds = true;
            } else {
                mergedBounds = MergeBounds(mergedBounds, bounds);
            }
            selectedMeshes += 1;
        }
    }

    *outBounds        = hasMergedBounds ? mergedBounds : GetModelBoundingBox(model);
    *outSelectedMeshes = selectedMeshes;
}

void TryLoadPlayerModel(AppState *app) {
    if (app == nullptr) {
        return;
    }
    const std::string modelPath = ResolveCharacterModelPath();
    app->playerModelPath = modelPath;
    if (modelPath.empty()) {
        app->playerModelStatus =
            "No model found (tried hisoka_hxh.glb, hisoka.glb, killua.glb)";
        app->statusMessage = "Character model not found. Using fallback geometry.";
        return;
    }

    Model model = LoadModel(modelPath.c_str());
    if (!IsModelValid(model)) {
        app->playerModelStatus = "Model file found but failed to load";
        app->statusMessage     = "Model file found but loading failed. Using fallback.";
        return;
    }

    bool hasRenderableTexture = false;
    int  whiteMaterialCount   = 0;
    for (int i = 0; i < model.materialCount; ++i) {
        const Texture2D tex = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
        if (tex.id > 0 && tex.width > 1 && tex.height > 1) {
            hasRenderableTexture = true;
        }
        const Color c = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color;
        if (c.r >= 245 && c.g >= 245 && c.b >= 245) {
            whiteMaterialCount += 1;
        }
    }

    const bool mostlyWhiteMaterials =
        model.materialCount > 0 && whiteMaterialCount >= model.materialCount - 1;
    const bool forcePalette = !hasRenderableTexture || mostlyWhiteMaterials;
    if (forcePalette) {
        static constexpr std::array<Color, 8> kMaterialPalette = {
            Color{219, 213, 201, 255}, Color{54, 64, 98, 255},  Color{79, 96, 132, 255},
            Color{147, 102, 87, 255},  Color{234, 228, 205, 255}, Color{118, 72, 82, 255},
            Color{211, 186, 153, 255}, Color{92, 111, 163, 255},
        };
        for (int i = 0; i < model.materialCount; ++i) {
            Texture2D tex = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
            if (tex.width <= 1 || tex.height <= 1) {
                model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{};
            }
            const Color palette =
                kMaterialPalette[static_cast<std::size_t>(i) % kMaterialPalette.size()];
            model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = palette;
            model.materials[i].maps[MATERIAL_MAP_EMISSION].color = Color{
                static_cast<unsigned char>(palette.r / 2),
                static_cast<unsigned char>(palette.g / 2),
                static_cast<unsigned char>(palette.b / 2), 255};
        }
    }

    BoundingBox chosenBounds{};
    int         selectedMeshes = 0;
    ResolveModelBounds(model, &chosenBounds, &selectedMeshes);
    const float height  = chosenBounds.max.y - chosenBounds.min.y;
    const float width   = chosenBounds.max.x - chosenBounds.min.x;
    const float depth   = chosenBounds.max.z - chosenBounds.min.z;
    const float centerX = (chosenBounds.max.x + chosenBounds.min.x) * 0.5F;
    const float centerZ = (chosenBounds.max.z + chosenBounds.min.z) * 0.5F;

    app->playerModel            = model;
    app->hasPlayerModel         = true;
    app->playerModelHasTexture  = hasRenderableTexture;
    app->playerModelStatus =
        "Model loaded: " + std::filesystem::path(modelPath).filename().string();
    if (forcePalette) {
        app->playerModelStatus += " | palette shading";
    }

    if (height > 0.001F) {
        app->playerModelScale =
            std::clamp(kPlayerModelTargetHeight / height, 0.03F, 4.0F);
        app->playerModelPivot       = {centerX, chosenBounds.min.y, centerZ};
        app->playerModelAuraRadius  =
            std::max(width, depth) * app->playerModelScale * 0.65F + 0.26F;
    } else {
        app->playerModelScale      = 0.65F;
        app->playerModelPivot      = {0.0F, 0.0F, 0.0F};
        app->playerModelAuraRadius = 0.62F;
    }

    int              animationCount = 0;
    ModelAnimation  *animations     = LoadModelAnimations(modelPath.c_str(), &animationCount);
    if (animations != nullptr && animationCount > 0) {
        app->playerAnimations     = animations;
        app->playerAnimationCount = animationCount;
        ConfigureAnimationSet(app);
    }

    app->playerModelStatus += " | meshes " + std::to_string(selectedMeshes) + "/" +
                              std::to_string(model.meshCount);
    app->playerModelStatus += " | anims " + std::to_string(app->playerAnimationCount);
    if (app->playerAnimationCount == 0) {
        app->statusMessage =
            "Model has no embedded animation clips. Procedural fallback animation enabled.";
    }
}

void UnloadPlayerModel(AppState *app) {
    if (app == nullptr) {
        return;
    }
    if (app->playerAnimations != nullptr) {
        UnloadModelAnimations(app->playerAnimations, app->playerAnimationCount);
        app->playerAnimations     = nullptr;
        app->playerAnimationCount = 0;
    }
    if (!app->hasPlayerModel) {
        app->playerModelStatus = "Model unloaded";
        return;
    }
    UnloadModel(app->playerModel);
    app->hasPlayerModel         = false;
    app->playerModelHasTexture  = false;
    app->idleAnimationIndex     = -1;
    app->moveAnimationIndex     = -1;
    app->chargeAnimationIndex   = -1;
    app->baseCastAnimationIndex = -1;
    app->hatsuCastAnimationIndex = -1;
    app->activeAnimationIndex   = -1;
    app->activeAnimationFrame   = 0.0F;
    app->playerModelStatus      = "Model unloaded";
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

void StartWorld(AppState *app, const nen::Character &character) {
    NEN_INFO("entering world  name=%s  type=%s  aura=%.0f  hatsu=%s  potency=%d",
             character.name.c_str(),
             nen::ToString(character.naturalType).data(),
             character.auraPool.current,
             character.hatsuName.c_str(),
             character.hatsuPotency);
    app->screen            = Screen::World;
    app->player            = character;
    app->hasCharacter      = true;
    app->playerPosition    = {kArenaBounds.x + 110.0F,
                              kArenaBounds.y + kArenaBounds.height - 130.0F};
    app->facingDirection   = {0.0F, -1.0F};
    app->chargingAura      = false;
    app->chargeEffectTimer = 0.0F;
    app->playerMoveSpeed   = 0.0F;
    app->proceduralAnimTime = 0.0F;
    app->modelBobOffset    = 0.0F;
    app->modelStrideOffset = 0.0F;
    app->modelLeanDegrees  = 0.0F;
    app->modelCastLeanDegrees = 0.0F;
    app->modelScalePulse   = 0.0F;
    app->animationState    = AnimState::Idle;
    app->activeAnimationIndex = -1;
    app->activeAnimationFrame = 0.0F;
    app->queuedAction      = QueuedAction{};
    app->selectedBaseType  = character.naturalType;
    app->baseAttackCooldown = 0.0F;
    app->hatsuCooldown     = 0.0F;
    app->activeAttacks.clear();
    app->enemy             = EnemyState{};
    app->cameraOrbitAngle  = 0.88F;
    app->cameraDistance    = 15.0F;
    app->cameraHeight      = 9.0F;
    app->camera.position   = {0.0F, 13.0F, 16.0F};
    app->camera.target     = {0.0F, 0.0F, 0.0F};
    app->camera.up         = {0.0F, 1.0F, 0.0F};
    app->camera.fovy       = 52.0F;
    app->camera.projection = CAMERA_PERSPECTIVE;
    app->cachedNenStats    = nen::ComputeNenStats(app->player);
    app->cachedHatsuSpec   = nen::BuildProceduralHatsuSpec(
        app->player.name, app->player.naturalType, app->player.hatsuPotency);
    app->statusMessage =
        "LMB/SPACE attack, RMB/Q hatsu, hold R recharge, E Ren, Z Zetsu, G Gyo, N En, K Ko.";

    InitParticleSystem(&app->particleSystem);
    const Vector3 playerWorldPos = ArenaToWorld(app->playerPosition);
    app->nenAuraEmitter = SpawnEmitter(
        &app->particleSystem,
        MakeAuraTenConfig(TypeColor(character.naturalType)),
        playerWorldPos);
    app->koEmitter = kNullEmitter;
}

// ── Screen update functions ───────────────────────────────────────────────────

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

    const Rectangle continueButton{86.0F, 390.0F, 300.0F, 62.0F};
    const Rectangle backButton{402.0F, 390.0F, 184.0F, 62.0F};

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
    app->screen            = Screen::Quiz;
    app->statusMessage     = "Answer with 1/2/3 or click.";
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
    int y            = 210;
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
        auto &loaded =
            app->storedCharacters[static_cast<std::size_t>(clickedIndex)].character;
        EnsureCharacterHasHatsu(&loaded);
        NEN_INFO("character loaded from save  name=%s  type=%s",
                 loaded.name.c_str(), nen::ToString(loaded.naturalType).data());
        StartWorld(app, loaded);
        app->statusMessage = "Loaded " + app->player.name + ".";
    }
}

void UpdateQuiz(AppState *app) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        app->screen = Screen::MainMenu;
        return;
    }

    const auto &questions = nen::PersonalityQuestions();
    const auto &question  = questions[app->quizQuestionIndex];

    std::array<Rectangle, 3> optionRects{{
        {86.0F, 258.0F, 960.0F, 72.0F},
        {86.0F, 340.0F, 960.0F, 72.0F},
        {86.0F, 422.0F, 960.0F, 72.0F},
    }};

    int answerIndex = -1;
    if (IsKeyPressed(KEY_ONE))        { answerIndex = 0; }
    else if (IsKeyPressed(KEY_TWO))   { answerIndex = 1; }
    else if (IsKeyPressed(KEY_THREE)) { answerIndex = 2; }
    else {
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

    nen::ApplyQuizAnswer(&app->quizScores,
                         question.options[static_cast<std::size_t>(answerIndex)]);
    app->quizQuestionIndex += 1;
    if (app->quizQuestionIndex < questions.size()) {
        return;
    }

    app->player = nen::Character{
        .name         = app->draftName,
        .naturalType  = nen::DetermineNenType(app->quizScores),
        .auraPool     = nen::AuraPool{.current = 180.0F},
        .hatsuPotency = nen::GenerateHatsuPotency(app->draftName),
    };
    app->hasCharacter  = true;
    app->revealTimer   = 0.0F;
    app->screen        = Screen::Reveal;
    app->statusMessage = "Divination complete. Define your hatsu.";
}

void UpdateReveal(AppState *app, float dt) {
    app->revealTimer += dt;
    const Rectangle continueButton{86.0F, 716.0F, 340.0F, 66.0F};
    if (app->revealTimer < 1.1F) {
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsButtonTriggered(continueButton)) {
        app->draftHatsuName.clear();
        app->selectedHatsuCategoryIndex = 0;
        app->selectedVowMask            = 0;
        app->screen                     = Screen::HatsuCreation;
        app->statusMessage              = "Name your hatsu and choose its nature.";
    }
}

void UpdateHatsuCreation(AppState *app) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        app->screen      = Screen::Reveal;
        app->revealTimer = 1.5F;
        return;
    }

    HandleNameInput(&app->draftHatsuName);

    if (IsKeyPressed(KEY_ONE))   { app->selectedHatsuCategoryIndex = 0; }
    if (IsKeyPressed(KEY_TWO))   { app->selectedHatsuCategoryIndex = 1; }
    if (IsKeyPressed(KEY_THREE)) { app->selectedHatsuCategoryIndex = 2; }
    if (IsKeyPressed(KEY_FOUR))  { app->selectedHatsuCategoryIndex = 3; }
    if (IsKeyPressed(KEY_FIVE))  { app->selectedHatsuCategoryIndex = 4; }
    if (IsKeyPressed(KEY_SIX))   { app->selectedHatsuCategoryIndex = 5; }

    constexpr float catY      = 330.0F;
    constexpr float catH      = 64.0F;
    constexpr float catW      = 166.0F;
    constexpr float catGap    = 10.0F;
    constexpr float catStartX = 86.0F;
    for (int i = 0; i < 6; ++i) {
        const Rectangle r{catStartX + static_cast<float>(i) * (catW + catGap), catY, catW, catH};
        if (IsButtonTriggered(r)) {
            app->selectedHatsuCategoryIndex = i;
        }
    }

    constexpr float vowStartY = 476.0F;
    constexpr float vowH      = 28.0F;
    constexpr float vowGap    = 4.0F;
    constexpr float col2X     = 700.0F;
    for (int i = 0; i < 12; ++i) {
        const bool  inCol2 = i >= 6;
        const float vx     = inCol2 ? col2X : 86.0F;
        const float vy     = vowStartY + static_cast<float>(inCol2 ? i - 6 : i) * (vowH + vowGap);
        const Rectangle checkRect{vx, vy + 4.0F, 16.0F, 16.0F};
        if (IsButtonTriggered(checkRect)) {
            const uint32_t bit       = 1u << static_cast<uint32_t>(i);
            const bool     alreadyOn = (app->selectedVowMask & bit) != 0u;
            if (alreadyOn) {
                app->selectedVowMask &= ~bit;
            } else {
                if (std::popcount(app->selectedVowMask) < 2) {
                    app->selectedVowMask |= bit;
                } else {
                    app->statusMessage = "Maximum 2 vows allowed.";
                }
            }
        }
    }

    const Rectangle continueBtn{86.0F, 754.0F, 320.0F, 62.0F};
    if ((IsKeyPressed(KEY_ENTER) || IsButtonTriggered(continueBtn)) &&
        !app->draftHatsuName.empty()) {
        const auto category =
            static_cast<nen::HatsuCategory>(app->selectedHatsuCategoryIndex);
        std::vector<int> vowIndices;
        for (int i = 0; i < 12; ++i) {
            if ((app->selectedVowMask & (1u << static_cast<uint32_t>(i))) != 0u) {
                vowIndices.push_back(i);
            }
        }
        app->player.hatsuName    = app->draftHatsuName;
        app->player.hatsuPotency = nen::GenerateHatsuPotency(app->player.name);
        NEN_INFO("hatsu defined  name=\"%s\"  category=%s  vows=%u  potency=%d",
                 app->draftHatsuName.c_str(),
                 nen::CategoryLabel(
                     static_cast<nen::HatsuCategory>(app->selectedHatsuCategoryIndex)),
                 static_cast<unsigned>(std::popcount(app->selectedVowMask)),
                 app->player.hatsuPotency);
        app->cachedHatsuSpec = nen::BuildUserHatsuSpec(
            app->draftHatsuName, app->player.naturalType,
            app->player.hatsuPotency, category, vowIndices);
        StartWorld(app, app->player);
        SaveCurrentCharacter(app);
        RefreshStoredCharacters(app);
    }
}

// ── World update helpers ──────────────────────────────────────────────────────

void MoveEnemy(AppState *app, float dt) {
    EnemyState &enemy = app->enemy;
    enemy.hitFlashTimer      = std::max(0.0F, enemy.hitFlashTimer - dt);
    enemy.manipulatedTimer   = std::max(0.0F, enemy.manipulatedTimer - dt);
    enemy.vulnerabilityTimer = std::max(0.0F, enemy.vulnerabilityTimer - dt);
    enemy.elasticTimer       = std::max(0.0F, enemy.elasticTimer - dt);
    if (enemy.elasticTimer <= 0.0F) {
        enemy.elasticStrength = 0.0F;
    }

    if (enemy.manipulatedTimer > 0.0F) {
        const Vector2 toPlayer{app->playerPosition.x - enemy.position.x,
                               app->playerPosition.y - enemy.position.y};
        const float distance =
            std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
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

    if (enemy.elasticTimer > 0.0F) {
        const Vector2 playerCenter{
            app->playerPosition.x + app->playerSize.x * 0.5F,
            app->playerPosition.y + app->playerSize.y * 0.5F,
        };
        const Vector2 toPlayer{playerCenter.x - enemy.position.x,
                               playerCenter.y - enemy.position.y};
        const float distance =
            std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
        if (distance > 0.001F) {
            const Vector2 dir{toPlayer.x / distance, toPlayer.y / distance};
            const float restLength  = 120.0F;
            if (distance > restLength) {
                const float stretch     = distance - restLength;
                const float springForce =
                    std::min(760.0F, stretch * (3.2F + enemy.elasticStrength));
                enemy.velocity.x += dir.x * springForce * dt;
                enemy.velocity.y += dir.y * springForce * dt;
            }
            enemy.velocity.x *= 0.985F;
            enemy.velocity.y *= 0.985F;
        }
    }

    const float left   = kArenaBounds.x + enemy.radius;
    const float right  = kArenaBounds.x + kArenaBounds.width  - enemy.radius;
    const float top    = kArenaBounds.y + enemy.radius;
    const float bottom = kArenaBounds.y + kArenaBounds.height - enemy.radius;

    if (enemy.position.x < left)  { enemy.position.x = left;  enemy.velocity.x =  std::abs(enemy.velocity.x); }
    if (enemy.position.x > right) { enemy.position.x = right; enemy.velocity.x = -std::abs(enemy.velocity.x); }
    if (enemy.position.y < top)   { enemy.position.y = top;   enemy.velocity.y =  std::abs(enemy.velocity.y); }
    if (enemy.position.y > bottom){ enemy.position.y = bottom; enemy.velocity.y = -std::abs(enemy.velocity.y); }
}

void UpdateCombatCamera(AppState *app, float dt) {
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0F) {
        app->cameraDistance = std::clamp(app->cameraDistance - wheel * 1.1F, 9.5F, 28.0F);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        const Vector2 delta = GetMouseDelta();
        app->cameraOrbitAngle -= delta.x * 0.0055F;
        app->cameraHeight = std::clamp(app->cameraHeight + delta.y * 0.02F, 5.8F, 15.0F);
    }

    const Vector2 playerCenter{
        app->playerPosition.x + app->playerSize.x * 0.5F,
        app->playerPosition.y + app->playerSize.y * 0.5F,
    };
    const Vector2 face = Normalize2D(app->facingDirection);
    const Vector2 focus2D{
        playerCenter.x + face.x * 58.0F,
        playerCenter.y + face.y * 58.0F,
    };
    const Vector3 focus       = ArenaToWorld(focus2D, 1.0F);
    const float   orbitDistance = app->cameraDistance;
    const Vector3 desiredPos{
        focus.x + std::cos(app->cameraOrbitAngle) * orbitDistance,
        focus.y + app->cameraHeight,
        focus.z + std::sin(app->cameraOrbitAngle) * orbitDistance,
    };

    const float follow = std::clamp(dt * 5.2F, 0.02F, 0.28F);
    app->camera.target.x = Lerp(app->camera.target.x, focus.x, follow);
    app->camera.target.y = Lerp(app->camera.target.y, focus.y, follow);
    app->camera.target.z = Lerp(app->camera.target.z, focus.z, follow);
    app->camera.position.x = Lerp(app->camera.position.x, desiredPos.x, follow);
    app->camera.position.y = Lerp(app->camera.position.y, desiredPos.y, follow);
    app->camera.position.z = Lerp(app->camera.position.z, desiredPos.z, follow);
}

void ClampPlayerToArena(AppState *app) {
    const float left   = kArenaBounds.x + 8.0F;
    const float right  = kArenaBounds.x + kArenaBounds.width  - app->playerSize.x - 8.0F;
    const float top    = kArenaBounds.y + 8.0F;
    const float bottom = kArenaBounds.y + kArenaBounds.height - app->playerSize.y - 8.0F;
    app->playerPosition.x = std::clamp(app->playerPosition.x, left, right);
    app->playerPosition.y = std::clamp(app->playerPosition.y, top,  bottom);
}

void UpdatePlayerMovement(AppState *app, float dt) {
    Vector2 movement{0.0F, 0.0F};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    { movement.y -= 1.0F; }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  { movement.y += 1.0F; }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  { movement.x -= 1.0F; }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { movement.x += 1.0F; }

    if (movement.x != 0.0F || movement.y != 0.0F) {
        const float length = std::sqrt(movement.x * movement.x + movement.y * movement.y);
        movement.x /= length;
        movement.y /= length;
        app->facingDirection = movement;
    }

    float speed = app->chargingAura ? 0.0F : 320.0F;
    if (app->queuedAction.queued) {
        speed *= 0.58F;
    }
    app->playerMoveSpeed   = speed * std::sqrt(movement.x * movement.x + movement.y * movement.y);
    app->playerPosition.x += movement.x * speed * dt;
    app->playerPosition.y += movement.y * speed * dt;
    ClampPlayerToArena(app);
}

void RechargeAura(AppState *app, float dt) {
    const bool holdRecharge = IsKeyDown(KEY_R);
    app->chargingAura = holdRecharge;
    if (!holdRecharge && app->cachedNenStats.regenMultiplier <= 0.0F) {
        return;
    }
    if (holdRecharge) {
        app->chargeEffectTimer += dt;
    }
    const float typeBonus = app->player.naturalType == nen::Type::Enhancer ? 14.0F : 0.0F;
    const float baseRate  = app->player.auraPool.regenRate + typeBonus;
    const float regenMult = holdRecharge ? app->cachedNenStats.regenMultiplier
                                         : app->cachedNenStats.regenMultiplier * 0.25F;
    const float gain = baseRate * regenMult * dt;
    app->player.auraPool.current =
        std::min(app->player.auraPool.max, app->player.auraPool.current + gain);
    if (holdRecharge) {
        app->statusMessage = "Channeling aura... hold R to recover.";
    }
}

void UpdateBaseTypeSelection(AppState *app) {
    if (IsKeyPressed(KEY_ONE))   { app->selectedBaseType = nen::Type::Enhancer; }
    else if (IsKeyPressed(KEY_TWO))   { app->selectedBaseType = nen::Type::Transmuter; }
    else if (IsKeyPressed(KEY_THREE)) { app->selectedBaseType = nen::Type::Emitter; }
    else if (IsKeyPressed(KEY_FOUR))  { app->selectedBaseType = nen::Type::Conjurer; }
    else if (IsKeyPressed(KEY_FIVE))  { app->selectedBaseType = nen::Type::Manipulator; }
    else if (IsKeyPressed(KEY_SIX))   { app->selectedBaseType = nen::Type::Specialist; }
}

void ExecuteBaseAttack(AppState *app, Vector2 target) {
    const Vector2 origin{app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.38F};
    const Vector2 aim = Normalize2D({target.x - origin.x, target.y - origin.y});
    app->facingDirection = aim;
    const int rawDamage =
        nen::ComputeAttackDamage(app->player.naturalType, app->selectedBaseType, kBaseAttackPower);
    const int damage = static_cast<int>(
        std::round(static_cast<float>(rawDamage) * app->cachedNenStats.damageMultiplier));
    SpawnBaseAttack(&app->activeAttacks, app->selectedBaseType, origin, target, damage);
    app->statusMessage =
        "Base aura attack cast: " + std::string(nen::ToString(app->selectedBaseType)) + ".";
}

void ExecuteHatsu(AppState *app, Vector2 target) {
    const Vector2 origin{app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.3F};
    const Vector2 aim = Normalize2D({target.x - origin.x, target.y - origin.y});
    app->facingDirection = aim;
    const int   potency  = std::max(70, app->player.hatsuPotency);
    const float vowMult  = nen::ComputeVowMultiplier(app->cachedHatsuSpec);
    const int rawDamage = static_cast<int>(std::round(
        nen::ComputeAttackDamage(app->player.naturalType, app->player.naturalType,
                                 kHatsuAttackPower) *
        (static_cast<float>(potency) / 100.0F) * vowMult));
    const int damage = static_cast<int>(
        std::round(static_cast<float>(rawDamage) * app->cachedNenStats.damageMultiplier));
    SpawnHatsuAttack(&app->activeAttacks, app->player.naturalType, origin, target, damage);
    app->statusMessage = "Hatsu activated: " + app->player.hatsuName;
}

bool QueueAction(AppState *app, bool hatsu, Vector2 target) {
    if (app->queuedAction.queued) {
        app->statusMessage = "Action in progress.";
        return false;
    }
    if (hatsu && !app->cachedNenStats.canUseHatsu) {
        app->statusMessage = "Hatsu blocked — technique state prevents it.";
        return false;
    }
    if (!hatsu && !app->cachedNenStats.canAttack) {
        app->statusMessage = "Zetsu active — cannot attack.";
        return false;
    }
    if (hatsu) {
        if (app->hatsuCooldown > 0.0F) {
            app->statusMessage = "Hatsu still cooling down.";
            return false;
        }
        if (!nen::TryConsumeAura(&app->player, kHatsuAuraCost)) {
            app->statusMessage = "Not enough aura for hatsu.";
            return false;
        }
        app->hatsuCooldown = 2.8F;
    } else {
        if (app->baseAttackCooldown > 0.0F) {
            app->statusMessage = "Base attack recharging.";
            return false;
        }
        if (!nen::TryConsumeAura(&app->player, kBaseAttackAuraCost)) {
            app->statusMessage = "Not enough aura for base attack.";
            return false;
        }
        app->baseAttackCooldown = 0.24F;
    }

    const Vector2 origin{app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.4F};
    app->facingDirection = Normalize2D({target.x - origin.x, target.y - origin.y});
    app->queuedAction = QueuedAction{
        .queued         = true,
        .hatsu          = hatsu,
        .fired          = false,
        .target         = target,
        .timer          = 0.0F,
        .triggerSeconds = hatsu ? 0.22F : 0.14F,
        .finishSeconds  = hatsu ? 0.46F : 0.30F,
    };
    app->statusMessage = hatsu ? "Preparing hatsu..." : "Preparing base attack...";
    return true;
}

void UpdateQueuedAction(AppState *app, float dt) {
    if (!app->queuedAction.queued) {
        return;
    }
    app->queuedAction.timer += dt;
    if (!app->queuedAction.fired &&
        app->queuedAction.timer >= app->queuedAction.triggerSeconds) {
        if (app->queuedAction.hatsu) {
            ExecuteHatsu(app, app->queuedAction.target);
        } else {
            ExecuteBaseAttack(app, app->queuedAction.target);
        }
        app->queuedAction.fired = true;
    }
    if (app->queuedAction.timer >= app->queuedAction.finishSeconds) {
        app->queuedAction = QueuedAction{};
    }
}

void UpdatePlayerAnimation(AppState *app, float dt) {
    const float moveNorm = std::clamp(app->playerMoveSpeed / 320.0F, 0.0F, 1.0F);
    app->proceduralAnimTime += dt * (1.4F + moveNorm * 6.0F);
    const float idleWave   = std::sin(app->proceduralAnimTime * 1.7F);
    const float strideWave = std::sin(app->proceduralAnimTime * (3.0F + moveNorm * 2.2F));
    app->modelBobOffset = idleWave * 0.02F + strideWave * (0.085F * moveNorm);
    app->modelStrideOffset = strideWave * (0.07F * moveNorm);
    if (app->chargingAura) {
        app->modelBobOffset += std::sin(app->chargeEffectTimer * 5.4F) * 0.05F;
    }
    app->modelLeanDegrees = std::sin(app->proceduralAnimTime * 0.9F) * (12.0F * moveNorm);
    app->modelCastLeanDegrees =
        app->queuedAction.queued ? std::sin(app->queuedAction.timer * 14.0F) * 4.5F : 0.0F;
    app->modelScalePulse =
        idleWave * 0.02F + moveNorm * std::sin(app->proceduralAnimTime * 3.2F) * 0.03F;
    if (app->chargingAura) {
        app->modelScalePulse += std::sin(app->chargeEffectTimer * 7.4F) * 0.04F;
    }
    if (app->queuedAction.queued) {
        app->modelScalePulse += std::sin(app->queuedAction.timer * 18.0F) * 0.03F;
    }

    AnimState targetState = AnimState::Idle;
    if (app->queuedAction.queued) {
        targetState = app->queuedAction.hatsu ? AnimState::CastHatsu : AnimState::CastBase;
    } else if (app->chargingAura) {
        targetState = AnimState::Charge;
    } else if (moveNorm > 0.18F) {
        targetState = AnimState::Move;
    }
    SetAnimationState(app, targetState);

    if (app->playerAnimations == nullptr || app->playerAnimationCount <= 0) {
        return;
    }
    const int animationIndex = AnimationIndexForState(*app, app->animationState);
    if (animationIndex < 0 || animationIndex >= app->playerAnimationCount) {
        return;
    }
    const ModelAnimation animation = app->playerAnimations[animationIndex];
    if (!IsModelAnimationValid(app->playerModel, animation) || animation.frameCount <= 0) {
        return;
    }

    const bool  loop  = app->animationState != AnimState::CastBase &&
                        app->animationState != AnimState::CastHatsu;
    const float speed =
        (app->animationState == AnimState::CastBase ||
         app->animationState == AnimState::CastHatsu)
            ? 34.0F
            : 24.0F + moveNorm * 8.0F;
    app->activeAnimationIndex = animationIndex;
    if (!loop && app->queuedAction.queued) {
        const float clipDuration =
            static_cast<float>(animation.frameCount) / std::max(1.0F, speed);
        app->queuedAction.triggerSeconds =
            app->queuedAction.hatsu ? clipDuration * 0.44F : clipDuration * 0.34F;
        app->queuedAction.finishSeconds = clipDuration + 0.07F;
    }
    app->activeAnimationFrame += dt * speed;

    int frame = 0;
    if (loop) {
        frame = static_cast<int>(app->activeAnimationFrame) % animation.frameCount;
    } else {
        frame = std::min(animation.frameCount - 1,
                         static_cast<int>(app->activeAnimationFrame));
    }
    UpdateModelAnimation(app->playerModel, animation, frame);
}

void ApplyAttackOutcome(AppState *app, const AttackOutcome &outcome) {
    if (outcome.damage <= 0 && outcome.manipulationSeconds <= 0.0F &&
        outcome.vulnerabilitySeconds <= 0.0F && outcome.elasticSeconds <= 0.0F) {
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
    if (outcome.elasticSeconds > 0.0F) {
        app->enemy.elasticTimer =
            std::max(app->enemy.elasticTimer, outcome.elasticSeconds);
        app->enemy.elasticStrength =
            std::max(app->enemy.elasticStrength, outcome.elasticStrength);
        app->statusMessage = "Transmuter elastic tether latched.";
    }

    int damage = outcome.damage;
    if (app->enemy.vulnerabilityTimer > 0.0F) {
        damage = static_cast<int>(std::round(static_cast<float>(damage) * 1.35F));
    }
    damage = std::max(0, damage);
    if (damage <= 0) {
        return;
    }

    app->enemy.health        = std::max(0, app->enemy.health - damage);
    app->enemy.hitFlashTimer = 0.12F;
    app->statusMessage       = "Hit for " + std::to_string(damage) + " damage.";
    NEN_DEBUG("hit  dmg=%d  enemy_hp=%d/%d", damage, app->enemy.health, app->enemy.maxHealth);

    // Impact particle bursts
    {
        const Vector3 hitPos = ArenaToWorld(EnemyCenter(*app), 0.8F);
        const Color   typeCol = TypeColor(app->player.naturalType);
        BurstAt(&app->particleSystem, MakeImpactFlashConfig(typeCol), hitPos);
        BurstAt(&app->particleSystem, MakeImpactSmokeConfig(typeCol), hitPos);
    }

    if (app->enemy.health > 0) {
        SaveCurrentCharacter(app);
        return;
    }

    app->enemy.health    = app->enemy.maxHealth;
    app->enemy.position  = {kArenaBounds.x + kArenaBounds.width - 160.0F,
                             kArenaBounds.y + 160.0F +
                             static_cast<float>(GetRandomValue(0, 220))};
    app->enemy.velocity  = {static_cast<float>(GetRandomValue(-220, -140)),
                             static_cast<float>(GetRandomValue(120, 210))};
    app->enemy.manipulatedTimer   = 0.0F;
    app->enemy.vulnerabilityTimer = 0.0F;
    app->enemy.elasticTimer       = 0.0F;
    app->enemy.elasticStrength    = 0.0F;
    app->player.auraPool.current  =
        std::min(app->player.auraPool.max, app->player.auraPool.current + 68.0F);
    app->statusMessage = "Enemy overwhelmed. +68 aura.";
    NEN_INFO("enemy defeated  +68 aura  total=%.0f", app->player.auraPool.current);
    SaveCurrentCharacter(app);
}

void UpdateWorld(AppState *app) {
    const float dt = GetFrameTime();

    if (IsKeyPressed(KEY_ESCAPE)) {
        SaveCurrentCharacter(app);
        UnloadParticleSystem(&app->particleSystem);
        app->screen        = Screen::MainMenu;
        app->statusMessage = "Returned to main menu.";
        return;
    }

    app->baseAttackCooldown = std::max(0.0F, app->baseAttackCooldown - dt);
    app->hatsuCooldown      = std::max(0.0F, app->hatsuCooldown - dt);

    const bool scaleUpPressed =
        IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD) ||
        IsKeyPressed(KEY_RIGHT_BRACKET);
    const bool scaleDownPressed =
        IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT) ||
        IsKeyPressed(KEY_LEFT_BRACKET);
    if (app->hasPlayerModel && scaleUpPressed) {
        app->playerModelScale =
            std::min(15.0F, app->playerModelScale * 1.35F);
        app->statusMessage =
            TextFormat("Model scale increased to %.3f.", app->playerModelScale);
    }
    if (app->hasPlayerModel && scaleDownPressed) {
        app->playerModelScale =
            std::max(0.005F, app->playerModelScale * 0.75F);
        app->statusMessage =
            TextFormat("Model scale decreased to %.3f.", app->playerModelScale);
    }

    UpdateBaseTypeSelection(app);

    const nen::NenInputIntent nenIntent{
        .holdZetsu = IsKeyDown(KEY_Z),
        .holdRen   = IsKeyDown(KEY_E),
        .toggleGyo = IsKeyPressed(KEY_G),
        .holdEn    = IsKeyDown(KEY_N),
        .armKo     = IsKeyDown(KEY_K),
    };
    const auto nenEvents    = nen::UpdateNenState(app->player, nenIntent, dt);
    app->cachedNenStats     = nen::ComputeNenStats(app->player);

    for (const auto &ev : nenEvents) {
        switch (ev) {
        case nen::NenEvent::EnteredTen:
            NEN_DEBUG("technique -> Ten");
            break;
        case nen::NenEvent::EnteredRen:
            NEN_INFO("technique -> Ren");
            break;
        case nen::NenEvent::EnteredZetsu:
            NEN_INFO("technique -> Zetsu");
            break;
        case nen::NenEvent::KoArmed:
            NEN_INFO("Ko armed");
            break;
        case nen::NenEvent::KoReleased:
            NEN_INFO("Ko released");
            break;
        case nen::NenEvent::AuraDepletedWarning:
            NEN_WARN("aura low!  %.0f / %.0f",
                     app->player.auraPool.current, app->player.auraPool.max);
            break;
        default:
            break;
        }
    }

    // Handle aura emitter transitions on nen state changes
    for (const auto &ev : nenEvents) {
        if (ev == nen::NenEvent::EnteredRen   ||
            ev == nen::NenEvent::EnteredTen   ||
            ev == nen::NenEvent::EnteredZetsu) {
            KillEmitter(&app->particleSystem, app->nenAuraEmitter);
            const Vector2 p2{app->playerPosition.x + app->playerSize.x * 0.5F,
                             app->playerPosition.y + app->playerSize.y * 0.5F};
            const Vector3 playerPos = ArenaToWorld(p2);
            const Color   typeCol   = TypeColor(app->player.naturalType);
            BurstAt(&app->particleSystem, MakeTransitionBurstConfig(typeCol), playerPos);
            if (ev == nen::NenEvent::EnteredZetsu) {
                app->nenAuraEmitter = kNullEmitter;
            } else if (ev == nen::NenEvent::EnteredRen) {
                app->nenAuraEmitter = SpawnEmitter(
                    &app->particleSystem, MakeAuraRenConfig(typeCol), playerPos);
            } else {
                app->nenAuraEmitter = SpawnEmitter(
                    &app->particleSystem, MakeAuraTenConfig(typeCol), playerPos);
            }
        }
        if (ev == nen::NenEvent::KoArmed) {
            KillEmitter(&app->particleSystem, app->koEmitter);
            const Vector2 p2{app->playerPosition.x + app->playerSize.x * 0.5F,
                             app->playerPosition.y + app->playerSize.y * 0.5F};
            const Vector3 playerPos = ArenaToWorld(p2);
            const Color   typeCol   = TypeColor(app->player.naturalType);
            app->koEmitter = SpawnEmitter(
                &app->particleSystem, MakeKoChargeConfig(typeCol), playerPos);
        }
        if (ev == nen::NenEvent::KoReleased) {
            KillEmitter(&app->particleSystem, app->koEmitter);
            app->koEmitter = kNullEmitter;
        }
    }

    // Keep aura emitter following player
    if (!app->nenAuraEmitter.IsNull()) {
        const Vector2 p2{app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.5F};
        MoveEmitter(&app->particleSystem, app->nenAuraEmitter, ArenaToWorld(p2));
    }
    if (!app->koEmitter.IsNull()) {
        const Vector2 p2{app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.5F};
        MoveEmitter(&app->particleSystem, app->koEmitter, ArenaToWorld(p2));
    }

    RechargeAura(app, dt);
    UpdatePlayerMovement(app, dt);

    Vector2 target     = EnemyCenter(*app);
    Vector2 arenaPoint{};
    if (MouseToArenaPoint(app->camera, &arenaPoint)) {
        target = arenaPoint;
    }

    const bool baseCast  = IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool hatsuCast = IsKeyPressed(KEY_Q)     || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (baseCast && !app->chargingAura) {
        QueueAction(app, false, target);
    }
    if (hatsuCast && !app->chargingAura) {
        QueueAction(app, true, target);
    }

    // Attack trail bursts
    {
        const Color typeCol = TypeColor(app->player.naturalType);
        for (const auto &attack : app->activeAttacks) {
            BurstAt(&app->particleSystem, MakeAttackTrailConfig(typeCol),
                    ArenaToWorld(attack.position, 0.55F));
        }
    }

    UpdateParticleSystem(&app->particleSystem, dt);
    UpdatePlayerAnimation(app, dt);
    UpdateQueuedAction(app, dt);
    MoveEnemy(app, dt);
    UpdateCombatCamera(app, dt);

    const AttackOutcome outcome =
        UpdateAttackEffects(&app->activeAttacks, dt, EnemyCenter(*app), app->enemy.radius);
    ApplyAttackOutcome(app, outcome);

    if (IsKeyPressed(KEY_F5)) {
        SaveCurrentCharacter(app);
        RefreshStoredCharacters(app);
        app->statusMessage = "Character saved.";
    }
}

} // namespace

// ── Raylib trace callback ────────────────────────────────────────────────────

void RaylibTraceCallback(int logLevel, const char *text, va_list args) {
    char buf[512];
    std::vsnprintf(buf, sizeof(buf), text, args);
    switch (logLevel) {
    case LOG_WARNING: NEN_WARN("[raylib] %s",  buf); break;
    case LOG_ERROR:   NEN_ERROR("[raylib] %s", buf); break;
    default:          break;
    }
}

// ── Entry point ──────────────────────────────────────────────────────────────

int Run() {
    SetTraceLogCallback(RaylibTraceCallback);
    SetTraceLogLevel(LOG_WARNING);

    NEN_INFO("nen_world starting up");
    InitWindow(kWidth, kHeight, "Nen World - Nen Combat Prototype");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    AppState app;
    TryLoadPlayerModel(&app);
    if (app.hasPlayerModel) {
        NEN_INFO("model loaded: %s  scale=%.4f",
                 app.playerModelPath.c_str(), app.playerModelScale);
    } else {
        NEN_WARN("no character model found — using fallback geometry");
    }
    RefreshStoredCharacters(&app);
    NEN_INFO("save directory: %s  (%zu characters found)",
             app.saveDir.string().c_str(), app.storedCharacters.size());

    bool running = true;
    while (running && !WindowShouldClose()) {
        const float dt = GetFrameTime();

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
        case Screen::HatsuCreation:
            UpdateHatsuCreation(&app);
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
        case Screen::HatsuCreation:
            DrawHatsuCreation(app);
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
    UnloadParticleSystem(&app.particleSystem);
    UnloadPlayerModel(&app);
    CloseWindow();
    return 0;
}

} // namespace game
