#include "game/game.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <filesystem>
#include <string>
#include <vector>

#include <raylib.h>

#include "game/attack_system.hpp"
#include "game/persistence.hpp"
#include "nen/combat.hpp"
#include "nen/hatsu.hpp"
#include "nen/hatsu_spec.hpp"
#include "nen/log.hpp"
#include "nen/nen_system.hpp"
#include "nen/quiz.hpp"
#include "nen/types.hpp"

namespace game {

namespace {

// ── Aura particle (presentation layer, not serialized) ────────────────────
struct AuraParticle {
    Vector3 position{};
    Vector3 velocity{};
    float   size        = 0.06F;
    float   alpha       = 0.3F;
    float   lifetime    = 0.0F;
    float   maxLifetime = 1.2F;
    Color   color       = RAYWHITE;
};

constexpr int kWidth = 1760;
constexpr int kHeight = 980;
constexpr int kEnemyMaxHealth = 430;

constexpr int kBaseAttackAuraCost = 16;
constexpr int kHatsuAuraCost = 52;
constexpr int kBaseAttackPower = 25;
constexpr int kHatsuAttackPower = 44;
constexpr float kWorldScale = 0.035F;
constexpr float kPlayerModelTargetHeight = 1.18F;

constexpr float kSidebarMargin = 28.0F;
constexpr float kSidebarTop = 88.0F;
constexpr float kSidebarBottomMargin = 94.0F;
const Rectangle kArenaBounds{40.0F, 110.0F, 1180.0F, 760.0F};

enum class Screen {
    MainMenu,
    NameEntry,
    LoadCharacter,
    Quiz,
    Reveal,
    HatsuCreation,
    World,
};

enum class AnimState {
    Idle,
    Move,
    Charge,
    CastBase,
    CastHatsu,
};

struct QueuedAction {
    bool queued = false;
    bool hatsu = false;
    bool fired = false;
    Vector2 target{0.0F, 0.0F};
    float timer = 0.0F;
    float triggerSeconds = 0.14F;
    float finishSeconds = 0.32F;
};

struct EnemyState {
    Vector2 position{810.0F, 450.0F};
    Vector2 velocity{180.0F, 120.0F};
    float radius = 42.0F;
    int maxHealth = kEnemyMaxHealth;
    int health = kEnemyMaxHealth;
    float manipulatedTimer = 0.0F;
    float vulnerabilityTimer = 0.0F;
    float elasticTimer = 0.0F;
    float elasticStrength = 0.0F;
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

    // HatsuCreation state
    std::string draftHatsuName;
    int selectedHatsuCategoryIndex = 0;  // index into HatsuCategory enum (0..5)
    uint32_t selectedVowMask = 0;        // bitmask for 12 canonical vows

    nen::Character player{
        .name        = "",
        .naturalType = nen::Type::Enhancer,
        .auraPool    = nen::AuraPool{.current = 120.0F},
    };
    nen::DerivedNenStats cachedNenStats{};
    nen::HatsuSpec       cachedHatsuSpec{};
    std::vector<AuraParticle> auraParticles;
    float auraSpawnAccum = 0.0F;
    bool hasCharacter = false;
    Vector2 playerPosition{kArenaBounds.x + 110.0F, kArenaBounds.y + kArenaBounds.height - 130.0F};
    Vector2 playerSize{36.0F, 52.0F};
    Vector2 facingDirection{0.0F, -1.0F};
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
    Vector3 playerModelPivot{0.0F, 0.0F, 0.0F};
    float playerModelAuraRadius = 0.5F;
    float playerModelYawOffset = 180.0F;
    bool playerModelHasTexture = false;
    std::string playerModelPath;
    std::string playerModelStatus = "Model not loaded";
    float cameraOrbitAngle = 0.88F;
    float cameraDistance = 15.0F;
    float cameraHeight = 9.0F;
    float playerMoveSpeed = 0.0F;
    float proceduralAnimTime = 0.0F;
    float modelBobOffset = 0.0F;
    float modelStrideOffset = 0.0F;
    float modelLeanDegrees = 0.0F;
    float modelCastLeanDegrees = 0.0F;
    float modelScalePulse = 0.0F;
    ModelAnimation *playerAnimations = nullptr;
    int playerAnimationCount = 0;
    int idleAnimationIndex = -1;
    int moveAnimationIndex = -1;
    int chargeAnimationIndex = -1;
    int baseCastAnimationIndex = -1;
    int hatsuCastAnimationIndex = -1;
    int activeAnimationIndex = -1;
    float activeAnimationFrame = 0.0F;
    AnimState animationState = AnimState::Idle;
    QueuedAction queuedAction{};
    Camera3D camera{
        .position = {0.0F, 13.0F, 16.0F},
        .target = {0.0F, 0.0F, 0.0F},
        .up = {0.0F, 1.0F, 0.0F},
        .fovy = 52.0F,
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

float DrawWrappedText(std::string_view text, Rectangle bounds, int fontSize, float spacing,
                      Color tint, int maxLines = 0) {
    Font font = GetFontDefault();
    std::string line;
    std::string word;
    float y = bounds.y;
    int linesDrawn = 0;

    auto flushLine = [&](bool forceEllipsis) {
        if (line.empty()) {
            return true;
        }
        if (y + static_cast<float>(fontSize) > bounds.y + bounds.height) {
            return false;
        }
        std::string toDraw = line;
        if (forceEllipsis && toDraw.size() > 3) {
            toDraw = toDraw.substr(0, toDraw.size() - 3) + "...";
        }
        DrawTextEx(font, toDraw.c_str(), {bounds.x, y}, static_cast<float>(fontSize), spacing,
                   tint);
        line.clear();
        y += static_cast<float>(fontSize) + 5.0F;
        linesDrawn += 1;
        if (maxLines > 0 && linesDrawn >= maxLines) {
            return false;
        }
        return true;
    };

    auto appendWord = [&](std::string_view nextWord) {
        if (nextWord.empty()) {
            return true;
        }
        std::string candidate = line;
        if (!candidate.empty()) {
            candidate.push_back(' ');
        }
        candidate.append(nextWord);

        const float width =
            MeasureTextEx(font, candidate.c_str(), static_cast<float>(fontSize), spacing).x;
        if (width <= bounds.width) {
            line = std::move(candidate);
            return true;
        }
        if (!flushLine(false)) {
            return false;
        }

        line = std::string(nextWord);
        const float singleWidth =
            MeasureTextEx(font, line.c_str(), static_cast<float>(fontSize), spacing).x;
        if (singleWidth <= bounds.width) {
            return true;
        }

        std::string fragmented;
        for (char c : line) {
            std::string test = fragmented;
            test.push_back(c);
            const float testWidth =
                MeasureTextEx(font, test.c_str(), static_cast<float>(fontSize), spacing).x;
            if (testWidth > bounds.width && !fragmented.empty()) {
                line = fragmented;
                if (!flushLine(false)) {
                    return false;
                }
                fragmented.clear();
            }
            fragmented.push_back(c);
        }
        line = fragmented;
        return true;
    };

    for (char c : std::string(text)) {
        if (c == '\n') {
            if (!appendWord(word)) {
                return y;
            }
            word.clear();
            if (!flushLine(false)) {
                return y;
            }
            continue;
        }
        if (c == ' ' || c == '\t') {
            if (!appendWord(word)) {
                return y;
            }
            word.clear();
            continue;
        }
        word.push_back(c);
    }

    if (!appendWord(word)) {
        return y;
    }
    flushLine(false);
    return y;
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
        NEN_ERROR("save failed: %s", error.c_str());
    } else {
        NEN_DEBUG("character saved  name=%s  aura=%.0f",
                  app->player.name.c_str(), app->player.auraPool.current);
    }
}

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

    app->idleAnimationIndex = FindAnimationIndex(*app, {"idle", "stand", "breath"});
    app->moveAnimationIndex = FindAnimationIndex(*app, {"walk", "run", "move", "jog"});
    app->chargeAnimationIndex = FindAnimationIndex(*app, {"charge", "focus", "channel", "cast"});
    app->baseCastAnimationIndex =
        FindAnimationIndex(*app, {"attack", "punch", "kick", "slash", "shoot", "fire"});
    app->hatsuCastAnimationIndex =
        FindAnimationIndex(*app, {"skill", "special", "spell", "ultimate", "cast"});

    if (app->idleAnimationIndex < 0) {
        app->idleAnimationIndex = 0;
    }
    if (app->moveAnimationIndex < 0) {
        app->moveAnimationIndex = app->idleAnimationIndex;
    }
    if (app->chargeAnimationIndex < 0) {
        app->chargeAnimationIndex = app->idleAnimationIndex;
    }
    if (app->baseCastAnimationIndex < 0) {
        app->baseCastAnimationIndex = app->moveAnimationIndex;
    }
    if (app->hatsuCastAnimationIndex < 0) {
        app->hatsuCastAnimationIndex = app->baseCastAnimationIndex;
    }
}

int AnimationIndexForState(const AppState &app, AnimState state) {
    switch (state) {
    case AnimState::Idle:
        return app.idleAnimationIndex;
    case AnimState::Move:
        return app.moveAnimationIndex;
    case AnimState::Charge:
        return app.chargeAnimationIndex;
    case AnimState::CastBase:
        return app.baseCastAnimationIndex;
    case AnimState::CastHatsu:
        return app.hatsuCastAnimationIndex;
    }
    return app.idleAnimationIndex;
}

const char *ToString(AnimState state) {
    switch (state) {
    case AnimState::Idle:
        return "Idle";
    case AnimState::Move:
        return "Move";
    case AnimState::Charge:
        return "Charge";
    case AnimState::CastBase:
        return "CastBase";
    case AnimState::CastHatsu:
        return "CastHatsu";
    }
    return "Unknown";
}

void SetAnimationState(AppState *app, AnimState state) {
    if (app == nullptr || app->animationState == state) {
        return;
    }
    app->animationState = state;
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
        const BoundingBox meshBounds = GetMeshBoundingBox(model.meshes[i]);
        const int vertexCount = model.meshes[i].vertexCount;
        if (!IsUsableMeshBounds(meshBounds, vertexCount)) {
            continue;
        }
        candidates.push_back({meshBounds, vertexCount});
        maxVertexCount = std::max(maxVertexCount, vertexCount);
    }

    if (candidates.empty()) {
        *outBounds = GetModelBoundingBox(model);
        *outSelectedMeshes = 0;
        return;
    }

    const int threshold = std::max(6, maxVertexCount / 10);
    bool hasMergedBounds = false;
    BoundingBox mergedBounds{};
    int selectedMeshes = 0;
    for (const auto &[bounds, vertexCount] : candidates) {
        if (vertexCount < threshold) {
            continue;
        }
        if (!hasMergedBounds) {
            mergedBounds = bounds;
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
                mergedBounds = bounds;
                hasMergedBounds = true;
            } else {
                mergedBounds = MergeBounds(mergedBounds, bounds);
            }
            selectedMeshes += 1;
        }
    }

    *outBounds = hasMergedBounds ? mergedBounds : GetModelBoundingBox(model);
    *outSelectedMeshes = selectedMeshes;
}

void TryLoadPlayerModel(AppState *app) {
    if (app == nullptr) {
        return;
    }

    const std::string modelPath = ResolveCharacterModelPath();
    app->playerModelPath = modelPath;
    if (modelPath.empty()) {
        app->playerModelStatus = "No model found (tried hisoka_hxh.glb, hisoka.glb, killua.glb)";
        app->statusMessage = "Character model not found. Using fallback geometry.";
        return;
    }

    Model model = LoadModel(modelPath.c_str());
    if (!IsModelValid(model)) {
        app->playerModelStatus = "Model file found but failed to load";
        app->statusMessage = "Model file found but loading failed. Using fallback.";
        return;
    }

    bool hasRenderableTexture = false;
    int whiteMaterialCount = 0;
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
            Color{147, 102, 87, 255}, Color{234, 228, 205, 255}, Color{118, 72, 82, 255},
            Color{211, 186, 153, 255}, Color{92, 111, 163, 255},
        };
        for (int i = 0; i < model.materialCount; ++i) {
            Texture2D tex = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
            if (tex.width <= 1 || tex.height <= 1) {
                model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{};
            }
            const Color palette = kMaterialPalette[static_cast<std::size_t>(i) % kMaterialPalette.size()];
            model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = palette;
            model.materials[i].maps[MATERIAL_MAP_EMISSION].color =
                Color{static_cast<unsigned char>(palette.r / 2), static_cast<unsigned char>(palette.g / 2),
                      static_cast<unsigned char>(palette.b / 2), 255};
        }
    }

    BoundingBox chosenBounds{};
    int selectedMeshes = 0;
    ResolveModelBounds(model, &chosenBounds, &selectedMeshes);
    const float height = chosenBounds.max.y - chosenBounds.min.y;
    const float width = chosenBounds.max.x - chosenBounds.min.x;
    const float depth = chosenBounds.max.z - chosenBounds.min.z;
    const float centerX = (chosenBounds.max.x + chosenBounds.min.x) * 0.5F;
    const float centerZ = (chosenBounds.max.z + chosenBounds.min.z) * 0.5F;

    app->playerModel = model;
    app->hasPlayerModel = true;
    app->playerModelHasTexture = hasRenderableTexture;
    app->playerModelStatus =
        "Model loaded: " + std::filesystem::path(modelPath).filename().string();
    if (forcePalette) {
        app->playerModelStatus += " | palette shading";
    }

    if (height > 0.001F) {
        app->playerModelScale = std::clamp(kPlayerModelTargetHeight / height, 0.03F, 4.0F);
        app->playerModelPivot = {centerX, chosenBounds.min.y, centerZ};
        app->playerModelAuraRadius =
            std::max(width, depth) * app->playerModelScale * 0.65F + 0.26F;
    } else {
        app->playerModelScale = 0.65F;
        app->playerModelPivot = {0.0F, 0.0F, 0.0F};
        app->playerModelAuraRadius = 0.62F;
    }

    int animationCount = 0;
    ModelAnimation *animations = LoadModelAnimations(modelPath.c_str(), &animationCount);
    if (animations != nullptr && animationCount > 0) {
        app->playerAnimations = animations;
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
        app->playerAnimations = nullptr;
        app->playerAnimationCount = 0;
    }
    if (!app->hasPlayerModel) {
        app->playerModelStatus = "Model unloaded";
        return;
    }
    UnloadModel(app->playerModel);
    app->hasPlayerModel = false;
    app->playerModelHasTexture = false;
    app->idleAnimationIndex = -1;
    app->moveAnimationIndex = -1;
    app->chargeAnimationIndex = -1;
    app->baseCastAnimationIndex = -1;
    app->hatsuCastAnimationIndex = -1;
    app->activeAnimationIndex = -1;
    app->activeAnimationFrame = 0.0F;
    app->playerModelStatus = "Model unloaded";
}

void StartWorld(AppState *app, const nen::Character &character) {
    NEN_INFO("entering world  name=%s  type=%s  aura=%.0f  hatsu=%s  potency=%d",
             character.name.c_str(),
             nen::ToString(character.naturalType).data(),
             character.auraPool.current,
             character.hatsuName.c_str(),
             character.hatsuPotency);
    app->screen = Screen::World;
    app->player = character;
    app->hasCharacter = true;
    app->playerPosition = {kArenaBounds.x + 110.0F, kArenaBounds.y + kArenaBounds.height - 130.0F};
    app->facingDirection = {0.0F, -1.0F};
    app->chargingAura = false;
    app->chargeEffectTimer = 0.0F;
    app->playerMoveSpeed = 0.0F;
    app->proceduralAnimTime = 0.0F;
    app->modelBobOffset = 0.0F;
    app->modelStrideOffset = 0.0F;
    app->modelLeanDegrees = 0.0F;
    app->modelCastLeanDegrees = 0.0F;
    app->modelScalePulse = 0.0F;
    app->animationState = AnimState::Idle;
    app->activeAnimationIndex = -1;
    app->activeAnimationFrame = 0.0F;
    app->queuedAction = QueuedAction{};
    app->selectedBaseType = character.naturalType;
    app->baseAttackCooldown = 0.0F;
    app->hatsuCooldown = 0.0F;
    app->activeAttacks.clear();
    app->enemy = EnemyState{};
    app->cameraOrbitAngle = 0.88F;
    app->cameraDistance = 15.0F;
    app->cameraHeight = 9.0F;
    app->camera.position = {0.0F, 13.0F, 16.0F};
    app->camera.target = {0.0F, 0.0F, 0.0F};
    app->camera.up = {0.0F, 1.0F, 0.0F};
    app->camera.fovy = 52.0F;
    app->camera.projection = CAMERA_PERSPECTIVE;
    app->auraParticles.clear();
    app->auraSpawnAccum   = 0.0F;
    app->cachedNenStats   = nen::ComputeNenStats(app->player);
    app->cachedHatsuSpec  = nen::BuildProceduralHatsuSpec(
        app->player.name, app->player.naturalType, app->player.hatsuPotency);
    app->statusMessage = "LMB/SPACE attack, RMB/Q hatsu, hold R recharge, E Ren, Z Zetsu, G Gyo, N En, K Ko.";
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
        auto &loaded = app->storedCharacters[static_cast<std::size_t>(clickedIndex)].character;
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
    const auto &question = questions[app->quizQuestionIndex];

    std::array<Rectangle, 3> optionRects{{
        {86.0F, 258.0F, 960.0F, 72.0F},
        {86.0F, 340.0F, 960.0F, 72.0F},
        {86.0F, 422.0F, 960.0F, 72.0F},
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
        .name         = app->draftName,
        .naturalType  = nen::DetermineNenType(app->quizScores),
        .auraPool     = nen::AuraPool{.current = 180.0F},
        .hatsuPotency = nen::GenerateHatsuPotency(app->draftName),
    };
    app->hasCharacter = true;
    app->revealTimer  = 0.0F;
    app->screen       = Screen::Reveal;
    app->statusMessage = "Divination complete. Define your hatsu.";
}

void UpdateReveal(AppState *app, float dt) {
    app->revealTimer += dt;
    const Rectangle continueButton{86.0F, 716.0F, 340.0F, 66.0F};
    if (app->revealTimer < 1.1F) {
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsButtonTriggered(continueButton)) {
        // Move to hatsu creation — user defines their ability
        app->draftHatsuName.clear();
        app->selectedHatsuCategoryIndex = 0;
        app->selectedVowMask = 0;
        app->screen = Screen::HatsuCreation;
        app->statusMessage = "Name your hatsu and choose its nature.";
    }
}

// ── Hatsu Creation ────────────────────────────────────────────────────────────

// Natural affinity categories per type (2 highlighted per type)
bool IsCategoryNatural(nen::Type type, nen::HatsuCategory cat) {
    using C = nen::HatsuCategory;
    using T = nen::Type;
    switch (type) {
    case T::Enhancer:   return cat == C::Projectile || cat == C::Passive;
    case T::Transmuter: return cat == C::Counter    || cat == C::Control;
    case T::Emitter:    return cat == C::Projectile || cat == C::Zone;
    case T::Conjurer:   return cat == C::Summon     || cat == C::Zone;
    case T::Manipulator:return cat == C::Control    || cat == C::Summon;
    case T::Specialist: return true;  // all natural for Specialist
    }
    return false;
}

void UpdateHatsuCreation(AppState *app) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        // Go back to reveal
        app->screen = Screen::Reveal;
        app->revealTimer = 1.5F;  // skip the intro pause
        return;
    }

    HandleNameInput(&app->draftHatsuName);

    // Category selection via number keys 1-6
    if (IsKeyPressed(KEY_ONE))   { app->selectedHatsuCategoryIndex = 0; }
    if (IsKeyPressed(KEY_TWO))   { app->selectedHatsuCategoryIndex = 1; }
    if (IsKeyPressed(KEY_THREE)) { app->selectedHatsuCategoryIndex = 2; }
    if (IsKeyPressed(KEY_FOUR))  { app->selectedHatsuCategoryIndex = 3; }
    if (IsKeyPressed(KEY_FIVE))  { app->selectedHatsuCategoryIndex = 4; }
    if (IsKeyPressed(KEY_SIX))   { app->selectedHatsuCategoryIndex = 5; }

    // Category click hitboxes — 6 buttons in one row
    constexpr float catY = 330.0F;
    constexpr float catH = 64.0F;
    constexpr float catW = 166.0F;
    constexpr float catGap = 10.0F;
    constexpr float catStartX = 86.0F;
    for (int i = 0; i < 6; ++i) {
        const Rectangle r{catStartX + static_cast<float>(i) * (catW + catGap), catY, catW, catH};
        if (IsButtonTriggered(r)) {
            app->selectedHatsuCategoryIndex = i;
        }
    }

    // Vow toggles — two-column layout matching DrawHatsuCreation exactly
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
            const uint32_t bit = 1u << static_cast<uint32_t>(i);
            const bool alreadyOn = (app->selectedVowMask & bit) != 0u;
            if (alreadyOn) {
                app->selectedVowMask &= ~bit;
            } else {
                // Max 2 vows
                if (__builtin_popcount(app->selectedVowMask) < 2) {
                    app->selectedVowMask |= bit;
                } else {
                    app->statusMessage = "Maximum 2 vows allowed.";
                }
            }
        }
    }

    // Continue — y must match DrawHatsuCreation
    const Rectangle continueBtn{86.0F, 754.0F, 320.0F, 62.0F};
    if ((IsKeyPressed(KEY_ENTER) || IsButtonTriggered(continueBtn)) &&
        !app->draftHatsuName.empty()) {
        // Build HatsuSpec from user choices
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
                 nen::CategoryLabel(static_cast<nen::HatsuCategory>(
                     app->selectedHatsuCategoryIndex)),
                 static_cast<unsigned>(__builtin_popcount(app->selectedVowMask)),
                 app->player.hatsuPotency);
        app->cachedHatsuSpec = nen::BuildUserHatsuSpec(
            app->draftHatsuName, app->player.naturalType,
            app->player.hatsuPotency, category, vowIndices);
        StartWorld(app, app->player);
        SaveCurrentCharacter(app);
        RefreshStoredCharacters(app);
    }
}

void DrawHatsuCreation(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});

    const Color typeCol = TypeColor(app.player.naturalType);
    DrawText("Design Your Hatsu", 86, 46, 48, RAYWHITE);
    DrawText(TextFormat("You are a %s — choose the nature of your power.",
                        nen::ToString(app.player.naturalType).data()),
             86, 104, 24, typeCol);

    // ── Hatsu name input ──────────────────────────────────────────────────
    DrawText("Hatsu Name", 86, 152, 22, Fade(WHITE, 0.60F));
    const Rectangle inputBox{86.0F, 178.0F, 540.0F, 60.0F};
    DrawRectangleRounded(inputBox, 0.18F, 8, {24, 38, 64, 255});
    DrawRectangleRoundedLinesEx(inputBox, 0.18F, 8, 2.0F, WHITE);
    const bool cursorBlink = static_cast<int>(GetTime() * 2.0) % 2 == 0;
    const std::string displayName = app.draftHatsuName + (cursorBlink ? "_" : " ");
    DrawText(displayName.c_str(), 106, 198, 30, RAYWHITE);
    if (app.draftHatsuName.empty()) {
        DrawText("e.g. \"Crimson Bind Thread\"", 110, 200, 26, Fade(WHITE, 0.28F));
    }

    // ── Category selector ─────────────────────────────────────────────────
    DrawText("Category  (1-6 keys or click)", 86, 302, 22, Fade(WHITE, 0.60F));
    constexpr float catY      = 330.0F;
    constexpr float catH      = 64.0F;
    constexpr float catW      = 166.0F;
    constexpr float catGap    = 10.0F;
    constexpr float catStartX = 86.0F;
    constexpr std::array<nen::HatsuCategory, 6> kCategories = {
        nen::HatsuCategory::Projectile, nen::HatsuCategory::Zone,
        nen::HatsuCategory::Control,    nen::HatsuCategory::Counter,
        nen::HatsuCategory::Summon,     nen::HatsuCategory::Passive,
    };
    for (int i = 0; i < 6; ++i) {
        const nen::HatsuCategory cat = kCategories[static_cast<std::size_t>(i)];
        const Rectangle r{catStartX + static_cast<float>(i) * (catW + catGap), catY, catW, catH};
        const bool selected = app.selectedHatsuCategoryIndex == i;
        const bool natural  = IsCategoryNatural(app.player.naturalType, cat);
        const bool hovered  = IsInside(r, GetMousePosition());

        Color fill = selected ? Color{57, 87, 143, 255}
                              : (hovered ? Color{40, 60, 100, 255} : Color{24, 38, 64, 255});
        DrawRectangleRounded(r, 0.18F, 8, fill);
        Color border = selected ? WHITE
                                : (natural ? Fade(typeCol, 0.75F) : Fade(WHITE, 0.30F));
        DrawRectangleRoundedLinesEx(r, 0.18F, 8, selected ? 2.5F : 1.5F, border);

        DrawText(nen::CategoryLabel(cat),
                 static_cast<int>(r.x + 10.0F), static_cast<int>(r.y + 8.0F), 20,
                 selected ? RAYWHITE : (natural ? typeCol : LIGHTGRAY));
        DrawText(nen::CategoryDescription(cat),
                 static_cast<int>(r.x + 10.0F), static_cast<int>(r.y + 34.0F), 13,
                 Fade(LIGHTGRAY, selected ? 0.9F : 0.55F));
        if (natural) {
            DrawText("*", static_cast<int>(r.x + catW - 16.0F),
                     static_cast<int>(r.y + 6.0F), 16, Fade(typeCol, 0.85F));
        }
    }
    DrawText("* = natural affinity for your type", 86, catY + catH + 6.0F, 16,
             Fade(typeCol, 0.55F));

    // ── Vow selector ──────────────────────────────────────────────────────
    DrawText("Vows  (optional — pick 0-2 for a power bonus)",
             86, 450.0F, 22, Fade(WHITE, 0.60F));

    // Show all 12 vows in two columns
    constexpr float vowStartY = 476.0F;
    constexpr float vowH      = 28.0F;
    constexpr float vowGap    = 4.0F;
    constexpr float col2X     = 700.0F;

    // The vow descriptions — must match kVowPool order in hatsu_spec.cpp
    constexpr std::array<std::pair<std::string_view, float>, 12> kVowDisplay = {{
        {"Cannot be used consecutively",           1.15F},
        {"Requires physical contact to activate",  1.40F},
        {"Only usable within 3m of target",        1.30F},
        {"Cannot be used against fleeing targets", 1.20F},
        {"Doubles aura cost in open terrain",      1.20F},
        {"Only one active target at a time",       1.25F},
        {"User cannot speak while active",         1.35F},
        {"Cannot be canceled once started",        1.20F},
        {"Activates only after being hit first",   1.50F},
        {"Usable only three times per encounter",  1.45F},
        {"Visible glow reveals user location",     1.30F},
        {"Costs double when not at full health",   1.25F},
    }};

    for (int i = 0; i < 12; ++i) {
        const bool inCol2 = i >= 6;
        const float vx = inCol2 ? col2X : 86.0F;
        const float vy = vowStartY + static_cast<float>(inCol2 ? i - 6 : i) * (vowH + vowGap);
        const uint32_t bit     = 1u << static_cast<uint32_t>(i);
        const bool    checked  = (app.selectedVowMask & bit) != 0u;
        const Rectangle checkR{vx, vy + 4.0F, 16.0F, 16.0F};
        DrawRectangle(static_cast<int>(checkR.x), static_cast<int>(checkR.y),
                      static_cast<int>(checkR.width), static_cast<int>(checkR.height),
                      checked ? Color{220, 180, 80, 255} : Color{30, 46, 76, 255});
        DrawRectangleLines(static_cast<int>(checkR.x), static_cast<int>(checkR.y),
                           static_cast<int>(checkR.width), static_cast<int>(checkR.height),
                           Fade(WHITE, 0.40F));
        const Color textCol = checked ? Color{220, 180, 80, 255} : Fade(WHITE, 0.65F);
        DrawText(kVowDisplay[static_cast<std::size_t>(i)].first.data(),
                 static_cast<int>(vx + 24.0F), static_cast<int>(vy + 5.0F), 16, textCol);
        DrawText(TextFormat("x%.2f", kVowDisplay[static_cast<std::size_t>(i)].second),
                 static_cast<int>(vx + 24.0F + 336.0F), static_cast<int>(vy + 5.0F), 16,
                 Fade(textCol, 0.75F));
    }

    // Potency preview
    {
        const int   basePot  = app.player.hatsuPotency;
        float       vowMult  = 1.0F;
        for (int i = 0; i < 12; ++i) {
            if ((app.selectedVowMask & (1u << static_cast<uint32_t>(i))) != 0u) {
                vowMult *= kVowDisplay[static_cast<std::size_t>(i)].second;
            }
        }
        const int finalPot = static_cast<int>(static_cast<float>(basePot) * vowMult);
        DrawText(TextFormat("Potency: %d base  x%.2f vows  =  %d", basePot, vowMult, finalPot),
                 86, 720.0F, 22, RAYWHITE);
    }

    // Continue button
    const bool canContinue = !app.draftHatsuName.empty();
    const Rectangle continueBtn{86.0F, 754.0F, 320.0F, 62.0F};
    DrawButton(continueBtn, "Confirm Hatsu", false);
    if (!canContinue) {
        DrawText("Enter a name to continue.", 420, 772, 20, Fade(WHITE, 0.40F));
    }

    DrawText("ESC to go back", 86, 828.0F, 18, Fade(WHITE, 0.35F));
}

// ── End Hatsu Creation ────────────────────────────────────────────────────────

Vector2 EnemyCenter(const AppState &app) { return app.enemy.position; }

void MoveEnemy(AppState *app, float dt) {
    EnemyState &enemy = app->enemy;
    enemy.hitFlashTimer = std::max(0.0F, enemy.hitFlashTimer - dt);
    enemy.manipulatedTimer = std::max(0.0F, enemy.manipulatedTimer - dt);
    enemy.vulnerabilityTimer = std::max(0.0F, enemy.vulnerabilityTimer - dt);
    enemy.elasticTimer = std::max(0.0F, enemy.elasticTimer - dt);
    if (enemy.elasticTimer <= 0.0F) {
        enemy.elasticStrength = 0.0F;
    }

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

    if (enemy.elasticTimer > 0.0F) {
        const Vector2 playerCenter{
            app->playerPosition.x + app->playerSize.x * 0.5F,
            app->playerPosition.y + app->playerSize.y * 0.5F,
        };
        const Vector2 toPlayer{playerCenter.x - enemy.position.x, playerCenter.y - enemy.position.y};
        const float distance = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
        if (distance > 0.001F) {
            const Vector2 dir{toPlayer.x / distance, toPlayer.y / distance};
            const float restLength = 120.0F;
            if (distance > restLength) {
                const float stretch = distance - restLength;
                const float springForce =
                    std::min(760.0F, stretch * (3.2F + enemy.elasticStrength));
                enemy.velocity.x += dir.x * springForce * dt;
                enemy.velocity.y += dir.y * springForce * dt;
            }
            enemy.velocity.x *= 0.985F;
            enemy.velocity.y *= 0.985F;
        }
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
    const Vector3 focus = ArenaToWorld(focus2D, 1.0F);
    const float orbitDistance = app->cameraDistance;
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
    const float left = kArenaBounds.x + 8.0F;
    const float right = kArenaBounds.x + kArenaBounds.width - app->playerSize.x - 8.0F;
    const float top = kArenaBounds.y + 8.0F;
    const float bottom = kArenaBounds.y + kArenaBounds.height - app->playerSize.y - 8.0F;
    app->playerPosition.x = std::clamp(app->playerPosition.x, left, right);
    app->playerPosition.y = std::clamp(app->playerPosition.y, top, bottom);
}

void UpdatePlayerMovement(AppState *app, float dt) {
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
        app->facingDirection = movement;
    }

    float speed = app->chargingAura ? 0.0F : 320.0F;
    if (app->queuedAction.queued) {
        speed *= 0.58F;
    }
    app->playerMoveSpeed = speed * std::sqrt(movement.x * movement.x + movement.y * movement.y);
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
    const float typeBonus  = app->player.naturalType == nen::Type::Enhancer ? 14.0F : 0.0F;
    const float baseRate   = app->player.auraPool.regenRate + typeBonus;
    const float regenMult  = holdRecharge ? app->cachedNenStats.regenMultiplier
                                          : app->cachedNenStats.regenMultiplier * 0.25F;
    const float gain = baseRate * regenMult * dt;
    app->player.auraPool.current = std::min(app->player.auraPool.max,
                                            app->player.auraPool.current + gain);
    if (holdRecharge) {
        app->statusMessage = "Channeling aura... hold R to recover.";
    }
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
    const int potency = std::max(70, app->player.hatsuPotency);
    const float vowMult = nen::ComputeVowMultiplier(app->cachedHatsuSpec);
    const int rawDamage = static_cast<int>(
        std::round(nen::ComputeAttackDamage(app->player.naturalType, app->player.naturalType,
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
    app->queuedAction = {
        .queued = true,
        .hatsu = hatsu,
        .fired = false,
        .target = target,
        .timer = 0.0F,
        .triggerSeconds = hatsu ? 0.22F : 0.14F,
        .finishSeconds = hatsu ? 0.46F : 0.30F,
    };
    app->statusMessage = hatsu ? "Preparing hatsu..." : "Preparing base attack...";
    return true;
}

void UpdateQueuedAction(AppState *app, float dt) {
    if (!app->queuedAction.queued) {
        return;
    }

    app->queuedAction.timer += dt;
    if (!app->queuedAction.fired && app->queuedAction.timer >= app->queuedAction.triggerSeconds) {
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
    const float idleWave = std::sin(app->proceduralAnimTime * 1.7F);
    const float strideWave = std::sin(app->proceduralAnimTime * (3.0F + moveNorm * 2.2F));
    app->modelBobOffset = idleWave * 0.02F + strideWave * (0.085F * moveNorm);
    app->modelStrideOffset = strideWave * (0.07F * moveNorm);
    if (app->chargingAura) {
        app->modelBobOffset += std::sin(app->chargeEffectTimer * 5.4F) * 0.05F;
    }
    app->modelLeanDegrees = std::sin(app->proceduralAnimTime * 0.9F) * (12.0F * moveNorm);
    app->modelCastLeanDegrees =
        app->queuedAction.queued ? std::sin(app->queuedAction.timer * 14.0F) * 4.5F : 0.0F;
    app->modelScalePulse = idleWave * 0.02F + moveNorm * std::sin(app->proceduralAnimTime * 3.2F) * 0.03F;
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

    const bool loop = app->animationState != AnimState::CastBase &&
                      app->animationState != AnimState::CastHatsu;
    const float speed =
        (app->animationState == AnimState::CastBase || app->animationState == AnimState::CastHatsu)
            ? 34.0F
            : 24.0F + moveNorm * 8.0F;
    app->activeAnimationIndex = animationIndex;
    if (!loop && app->queuedAction.queued) {
        const float clipDuration = static_cast<float>(animation.frameCount) / std::max(1.0F, speed);
        app->queuedAction.triggerSeconds =
            app->queuedAction.hatsu ? clipDuration * 0.44F : clipDuration * 0.34F;
        app->queuedAction.finishSeconds = clipDuration + 0.07F;
    }
    app->activeAnimationFrame += dt * speed;

    int frame = 0;
    if (loop) {
        frame = static_cast<int>(app->activeAnimationFrame) % animation.frameCount;
    } else {
        frame = std::min(animation.frameCount - 1, static_cast<int>(app->activeAnimationFrame));
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
        app->enemy.elasticTimer = std::max(app->enemy.elasticTimer, outcome.elasticSeconds);
        app->enemy.elasticStrength = std::max(app->enemy.elasticStrength, outcome.elasticStrength);
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

    app->enemy.health = std::max(0, app->enemy.health - damage);
    app->enemy.hitFlashTimer = 0.12F;
    app->statusMessage = "Hit for " + std::to_string(damage) + " damage.";
    NEN_DEBUG("hit  dmg=%d  enemy_hp=%d/%d", damage, app->enemy.health, app->enemy.maxHealth);

    if (app->enemy.health > 0) {
        SaveCurrentCharacter(app);
        return;
    }

    app->enemy.health = app->enemy.maxHealth;
    app->enemy.position = {kArenaBounds.x + kArenaBounds.width - 160.0F,
                           kArenaBounds.y + 160.0F + static_cast<float>(GetRandomValue(0, 220))};
    app->enemy.velocity = {static_cast<float>(GetRandomValue(-220, -140)),
                           static_cast<float>(GetRandomValue(120, 210))};
    app->enemy.manipulatedTimer = 0.0F;
    app->enemy.vulnerabilityTimer = 0.0F;
    app->enemy.elasticTimer = 0.0F;
    app->enemy.elasticStrength = 0.0F;
    app->player.auraPool.current = std::min(app->player.auraPool.max,
                                            app->player.auraPool.current + 68.0F);
    app->statusMessage = "Enemy overwhelmed. +68 aura.";
    NEN_INFO("enemy defeated  +68 aura  total=%.0f", app->player.auraPool.current);
    SaveCurrentCharacter(app);
}

// Forward declarations (defined further below with Draw* functions)
void UpdateAuraParticles(AppState *app, float dt);

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
    const bool scaleUpPressed =
        IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_RIGHT_BRACKET);
    const bool scaleDownPressed =
        IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_LEFT_BRACKET);
    if (app->hasPlayerModel && scaleUpPressed) {
        app->playerModelScale = std::min(15.0F, app->playerModelScale * 1.35F);
        app->statusMessage = TextFormat("Model scale increased to %.3f.", app->playerModelScale);
    }
    if (app->hasPlayerModel && scaleDownPressed) {
        app->playerModelScale = std::max(0.005F, app->playerModelScale * 0.75F);
        app->statusMessage = TextFormat("Model scale decreased to %.3f.", app->playerModelScale);
    }

    UpdateBaseTypeSelection(app);

    // Nen technique input
    const nen::NenInputIntent nenIntent{
        .holdZetsu = IsKeyDown(KEY_Z),
        .holdRen   = IsKeyDown(KEY_E),
        .toggleGyo = IsKeyPressed(KEY_G),
        .holdEn    = IsKeyDown(KEY_N),
        .armKo     = IsKeyDown(KEY_K),
    };
    const auto nenEvents = nen::UpdateNenState(app->player, nenIntent, dt);
    app->cachedNenStats  = nen::ComputeNenStats(app->player);
    for (const auto &ev : nenEvents) {
        switch (ev) {
        case nen::NenEvent::EnteredTen:   NEN_DEBUG("technique -> Ten");   break;
        case nen::NenEvent::EnteredRen:   NEN_INFO("technique -> Ren");    break;
        case nen::NenEvent::EnteredZetsu: NEN_INFO("technique -> Zetsu");  break;
        case nen::NenEvent::KoArmed:      NEN_INFO("Ko armed");            break;
        case nen::NenEvent::KoReleased:   NEN_INFO("Ko released");         break;
        case nen::NenEvent::AuraDepletedWarning: NEN_WARN("aura low!  %.0f / %.0f",
            app->player.auraPool.current, app->player.auraPool.max);      break;
        default: break;
        }
    }

    RechargeAura(app, dt);
    UpdatePlayerMovement(app, dt);

    Vector2 target = EnemyCenter(*app);
    Vector2 arenaPoint;
    if (MouseToArenaPoint(app->camera, &arenaPoint)) {
        target = arenaPoint;
    }

    const bool baseCast = IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool hatsuCast = IsKeyPressed(KEY_Q) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (baseCast && !app->chargingAura) {
        QueueAction(app, false, target);
    }
    if (hatsuCast && !app->chargingAura) {
        QueueAction(app, true, target);
    }

    UpdateAuraParticles(app, dt);
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
    DrawText("Create New Hunter", 86, 60, 52, RAYWHITE);
    DrawText("Your name determines your hatsu's identity.", 86, 126, 26, LIGHTGRAY);

    // Name input box
    const Rectangle inputBox{86.0F, 188.0F, 540.0F, 66.0F};
    DrawRectangleRounded(inputBox, 0.18F, 8, {24, 38, 64, 255});
    DrawRectangleRoundedLinesEx(inputBox, 0.18F, 8, 2.0F, WHITE);
    // Blinking cursor appended to draft name
    const bool cursorVisible = static_cast<int>(GetTime() * 2.0) % 2 == 0;
    const std::string displayName = app.draftName + (cursorVisible ? "_" : " ");
    DrawText(displayName.c_str(), 106, 208, 34, RAYWHITE);

    DrawText("After the quiz, you will name your own hatsu.", 86, 278, 22, Fade(WHITE, 0.38F));

    DrawButton({86.0F, 390.0F, 300.0F, 62.0F}, "Continue", false);
    DrawButton({402.0F, 390.0F, 184.0F, 62.0F}, "Back", false);

    DrawText("Up to 20 characters. Press Enter or click Continue.", 86, 470, 20,
             Fade(WHITE, 0.40F));
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
                                 " | aura " + std::to_string(static_cast<int>(ch.auraPool.current));
        DrawText(line.c_str(), 96, y + 8, 24, RAYWHITE);
        y += 48;
    }
}

void DrawQuiz(const AppState &app) {
    const auto &questions = nen::PersonalityQuestions();
    const auto &question  = questions[app.quizQuestionIndex];
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});

    DrawText("Nen Personality Assessment", 86, 54, 48, RAYWHITE);

    // Progress bar
    {
        const int total    = static_cast<int>(questions.size());
        const int current  = static_cast<int>(app.quizQuestionIndex);
        constexpr float barX = 86.0F;
        constexpr float barY = 112.0F;
        constexpr float barW = 700.0F;
        constexpr float barH = 8.0F;
        DrawRectangle(static_cast<int>(barX), static_cast<int>(barY), static_cast<int>(barW),
                      static_cast<int>(barH), {30, 46, 76, 255});
        const float filled = barW * (static_cast<float>(current) / static_cast<float>(total));
        DrawRectangle(static_cast<int>(barX), static_cast<int>(barY), static_cast<int>(filled),
                      static_cast<int>(barH), {90, 160, 255, 200});
        // Segment ticks
        for (int i = 1; i < total; ++i) {
            const float tx = barX + barW * (static_cast<float>(i) / static_cast<float>(total));
            DrawLine(static_cast<int>(tx), static_cast<int>(barY),
                     static_cast<int>(tx), static_cast<int>(barY + barH),
                     Fade(WHITE, 0.25F));
        }
        DrawText(TextFormat("%d / %d", current + 1, total), static_cast<int>(barX + barW + 12),
                 static_cast<int>(barY - 2), 20, LIGHTGRAY);
    }

    // Question box with wrapped text
    DrawRectangleRounded({86.0F, 138.0F, 960.0F, 96.0F}, 0.12F, 6, {24, 38, 64, 255});
    DrawRectangleRoundedLinesEx({86.0F, 138.0F, 960.0F, 96.0F}, 0.12F, 6, 1.5F, Fade(WHITE, 0.4F));
    DrawWrappedText(question.prompt, {108.0F, 158.0F, 920.0F, 76.0F}, 28, 1.0F, RAYWHITE, 2);

    // Option buttons with wrapped labels
    constexpr float optX = 86.0F;
    constexpr float optW = 960.0F;
    constexpr float optH = 72.0F;
    constexpr float optGap = 86.0F;
    for (int i = 0; i < 3; ++i) {
        const float oy = 256.0F + static_cast<float>(i) * (optH + optGap - optH);
        const Rectangle rect{optX, 258.0F + static_cast<float>(i) * 82.0F, optW, optH};
        const bool hovered = IsInside(rect, GetMousePosition());
        DrawRectangleRounded(rect, 0.12F, 8,
                             hovered ? Color{57, 87, 143, 255} : Color{28, 44, 74, 255});
        DrawRectangleRoundedLinesEx(rect, 0.12F, 8, hovered ? 2.0F : 1.5F,
                                    hovered ? WHITE : Fade(WHITE, 0.45F));
        const std::string label =
            std::to_string(i + 1) + ".  " +
            std::string(question.options[static_cast<std::size_t>(i)].text);
        DrawWrappedText(label, {rect.x + 18.0F, rect.y + 14.0F, rect.width - 36.0F, 44.0F},
                        24, 1.0F, RAYWHITE, 1);
        (void)oy;
    }

    DrawText("Press 1/2/3 or click an option", 86, 512, 20, Fade(WHITE, 0.40F));
}

void DrawReveal(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {20, 29, 50, 255}, {12, 19, 36, 255});
    DrawText("Water Divination", 86, 52, 50, RAYWHITE);
    DrawText(TextFormat("%s is a %s!", app.player.name.c_str(),
                        nen::ToString(app.player.naturalType).data()),
             86, 116, 44, TypeColor(app.player.naturalType));
    DrawText(nen::WaterDivinationEffect(app.player.naturalType).data(), 86, 174, 26, LIGHTGRAY);

    DrawText(nen::HatsuAbilityName(app.player.naturalType).data(), 86, 216, 26, LIGHTGRAY);
    DrawWrappedText(nen::HatsuAbilityDescription(app.player.naturalType),
                    {86.0F, 250.0F, 820.0F, 52.0F}, 20, 1.0F, Fade(WHITE, 0.60F), 2);

    DrawText("You will name and shape your hatsu on the next screen.",
             86, 318, 22, Fade(WHITE, 0.42F));

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


void DrawElasticTether3D(const AppState &app) {
    if (app.enemy.elasticTimer <= 0.0F) {
        return;
    }

    const Vector2 player2{
        app.playerPosition.x + app.playerSize.x * 0.5F,
        app.playerPosition.y + app.playerSize.y * 0.42F,
    };
    const Vector3 start = ArenaToWorld(player2, 1.1F);
    const Vector3 end = ArenaToWorld(app.enemy.position, 1.08F);
    constexpr int segmentCount = 12;
    const float phase = static_cast<float>(GetTime()) * 12.0F;
    for (int i = 0; i < segmentCount; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(segmentCount);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(segmentCount);
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

void DrawArena3D(const AppState &app) {
    (void)app;

    // Large ground plane — dark stone
    DrawPlane({0.0F, 0.0F, 0.0F}, {200.0F, 200.0F}, {22, 30, 46, 255});

    // Arena boundary — glowing rectangle on the floor
    const Vector3 arenaCenter = ArenaToWorld(
        {kArenaBounds.x + kArenaBounds.width * 0.5F, kArenaBounds.y + kArenaBounds.height * 0.5F});
    const float hw = kArenaBounds.width  * kWorldScale * 0.5F;
    const float hd = kArenaBounds.height * kWorldScale * 0.5F;
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
            const Vector3 tail =
                ArenaToWorld({effect.position.x - dir2D.x * 28.0F, effect.position.y - dir2D.y * 28.0F},
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
                {effect.position.x - dir2D.x * 28.0F, effect.position.y - dir2D.y * 28.0F}, pos.y);
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
                                 pos.y + 0.12F, pos.z + std::cos(effect.phase * 4.2F) * radius * 1.9F};
            const Vector3 shardB{pos.x - std::sin(effect.phase * 4.2F) * radius * 1.9F,
                                 pos.y - 0.12F, pos.z - std::cos(effect.phase * 4.2F) * radius * 1.9F};
            DrawCubeV(shardA, {radius * 0.7F, radius * 0.28F, radius * 1.65F}, Fade(color, 0.8F));
            DrawCubeV(shardB, {radius * 0.7F, radius * 0.28F, radius * 1.65F}, Fade(color, 0.8F));
            DrawCircle3D(pos, radius * 1.1F, {0.0F, 1.0F, 0.0F}, spin, Fade(WHITE, 0.35F));
            break;
        }
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
    const float radius = enemy.radius * kWorldScale * 0.45F;
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
    if (enemy.elasticTimer > 0.0F) {
        DrawCircle3D({center.x, center.y + 0.03F, center.z}, radius + 0.3F, {1.0F, 0.0F, 0.0F},
                     90.0F, {133, 228, 255, 255});
    }
}

void DrawPlayer3D(const AppState &app) {
    const Vector2 p2 = {app.playerPosition.x + app.playerSize.x * 0.5F,
                        app.playerPosition.y + app.playerSize.y * 0.5F};
    const Vector2 facing2 = Normalize2D(app.facingDirection);
    const Vector2 strideOffset2D{facing2.x * app.modelStrideOffset, facing2.y * app.modelStrideOffset};
    const Vector2 render2D{p2.x + strideOffset2D.x, p2.y + strideOffset2D.y};
    const Vector3 anchor = ArenaToWorld(render2D, 0.0F + app.modelBobOffset);
    const Color aura = TypeColor(app.player.naturalType);
    const float animatedScale = app.playerModelScale * (1.0F + app.modelScalePulse);

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
                             {animatedScale, animatedScale, animatedScale},
                             Fade(WHITE, 0.26F));
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
        const float t = app.chargeEffectTimer;
        const float pulse = auraRadius + 0.18F + std::sin(t * 5.2F) * 0.07F;
        DrawSphereWires({anchor.x, anchor.y + 0.56F, anchor.z}, pulse, 10, 10, Fade(WHITE, 0.72F));
        DrawCircle3D({anchor.x, anchor.y + 0.22F, anchor.z}, pulse, {1.0F, 0.0F, 0.0F}, 90.0F,
                     WHITE);
        DrawCircle3D({anchor.x, anchor.y + 0.92F, anchor.z}, pulse * 0.88F, {1.0F, 0.0F, 0.0F},
                     90.0F, Fade(WHITE, 0.88F));
    }
}

void UpdateAuraParticles(AppState *app, float dt) {
    const nen::DerivedNenStats &stats = app->cachedNenStats;
    const Color typeCol = TypeColor(app->player.naturalType);

    // Spawn rate table
    float spawnRate = 0.0F;
    float alpha     = 0.0F;
    float size      = 0.06F;
    if (stats.auraVisibility >= 1.9F) {        // Ko armed
        spawnRate = 30.0F;
        alpha     = 0.85F;
        size      = 0.14F;
    } else if (stats.auraVisibility >= 1.4F) { // Ren
        spawnRate = 18.0F;
        alpha     = 0.55F;
        size      = 0.11F;
    } else if (stats.auraVisibility >= 0.5F) { // Ten
        spawnRate = 3.0F;
        alpha     = 0.22F;
        size      = 0.06F;
    }

    // Advance existing particles
    for (auto &p : app->auraParticles) {
        p.lifetime += dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        p.alpha = alpha * (1.0F - p.lifetime / p.maxLifetime);
    }
    app->auraParticles.erase(
        std::remove_if(app->auraParticles.begin(), app->auraParticles.end(),
                       [](const AuraParticle &p) { return p.lifetime >= p.maxLifetime; }),
        app->auraParticles.end());

    if (spawnRate <= 0.0F) {
        return;
    }

    app->auraSpawnAccum += spawnRate * dt;
    const Vector2 p2 = {app->playerPosition.x + app->playerSize.x * 0.5F,
                         app->playerPosition.y + app->playerSize.y * 0.5F};
    const Vector3 origin = ArenaToWorld(p2, 0.6F);

    while (app->auraSpawnAccum >= 1.0F && app->auraParticles.size() < 400) {
        app->auraSpawnAccum -= 1.0F;
        const float angle =
            static_cast<float>(GetRandomValue(0, 628)) * 0.01F;  // 0..2π approx
        const float r = static_cast<float>(GetRandomValue(10, 50)) * 0.01F;
        AuraParticle p{};
        p.position   = {origin.x + std::cos(angle) * r, origin.y, origin.z + std::sin(angle) * r};
        p.velocity   = {std::cos(angle) * 0.18F,
                         0.28F + static_cast<float>(GetRandomValue(0, 30)) * 0.01F,
                         std::sin(angle) * 0.18F};
        p.size       = size;
        p.alpha      = alpha;
        p.lifetime   = 0.0F;
        p.maxLifetime = 0.8F + static_cast<float>(GetRandomValue(0, 40)) * 0.01F;
        p.color      = typeCol;
        app->auraParticles.push_back(p);
    }
}

void DrawAuraParticles(const AppState &app) {
    for (const auto &p : app.auraParticles) {
        if (p.alpha <= 0.01F) {
            continue;
        }
        Color c = p.color;
        c.a = static_cast<unsigned char>(std::clamp(p.alpha * 255.0F, 0.0F, 255.0F));
        DrawSphere(p.position, p.size, c);
    }
}

void DrawEnSphere(const AppState &app) {
    if (app.cachedNenStats.detectionRadius <= 0.0F) {
        return;
    }
    const Vector2 p2 = {app.playerPosition.x + app.playerSize.x * 0.5F,
                         app.playerPosition.y + app.playerSize.y * 0.5F};
    const Vector3 center = ArenaToWorld(p2, 0.0F);
    const float r = app.cachedNenStats.detectionRadius;
    DrawCircle3D(center, r, {1.0F, 0.0F, 0.0F}, 90.0F, Fade({133, 228, 255, 255}, 0.18F));
    DrawCircle3D(center, r, {0.0F, 0.0F, 1.0F}, 0.0F,  Fade({133, 228, 255, 255}, 0.12F));
    DrawSphereWires(center, r, 6, 6, Fade({133, 228, 255, 255}, 0.08F));
}

void DrawWorld(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {15, 24, 41, 255}, {8, 14, 27, 255});

    BeginMode3D(app.camera);
    DrawArena3D(app);
    DrawAttackEffects3D(app);
    DrawElasticTether3D(app);
    DrawEnemy3D(app);
    DrawPlayer3D(app);
    DrawAuraParticles(app);
    DrawEnSphere(app);
    EndMode3D();

    const float panelX = kArenaBounds.x + kArenaBounds.width + kSidebarMargin;
    const float panelY = kSidebarTop;
    const float panelW = static_cast<float>(kWidth) - panelX - kSidebarMargin;
    const float panelH = static_cast<float>(kHeight) - panelY - kSidebarBottomMargin;
    const Rectangle panelRect{panelX, panelY, panelW, panelH};
    DrawRectangleRounded(panelRect, 0.03F, 6, {22, 34, 56, 255});
    DrawRectangleRoundedLinesEx(panelRect, 0.03F, 6, 2.0F, Fade(WHITE, 0.6F));

    const float textX   = panelX + 14.0F;
    const float textW   = panelW - 28.0F;
    const float bottom  = panelY + panelH - 4.0F;
    float y = panelY + 14.0F;

    // Clip all sidebar text to the panel bounds
    BeginScissorMode(static_cast<int>(panelX + 2), static_cast<int>(panelY + 2),
                     static_cast<int>(panelW - 4), static_cast<int>(panelH - 4));

    // ── Character ──────────────────────────────────────────────────────────
    DrawText(app.player.name.c_str(), static_cast<int>(textX), static_cast<int>(y), 22, RAYWHITE);
    y += 24.0F;
    DrawText(TextFormat("Type: %s", nen::ToString(app.player.naturalType).data()),
             static_cast<int>(textX), static_cast<int>(y), 20, TypeColor(app.player.naturalType));
    y += 22.0F;

    // Aura bar
    {
        const float barW   = textW;
        const float barH   = 10.0F;
        const float filled = barW * std::clamp(app.player.auraPool.current /
                                               app.player.auraPool.max, 0.0F, 1.0F);
        DrawRectangle(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(barW),
                      static_cast<int>(barH), {30, 46, 76, 255});
        DrawRectangle(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(filled),
                      static_cast<int>(barH), Fade(TypeColor(app.player.naturalType), 0.85F));
        DrawRectangleLines(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(barW),
                           static_cast<int>(barH), Fade(WHITE, 0.3F));
        y += barH + 3.0F;
        DrawText(TextFormat("Aura  %.0f / %.0f", app.player.auraPool.current,
                             app.player.auraPool.max),
                 static_cast<int>(textX), static_cast<int>(y), 16, Fade(WHITE, 0.80F));
        y += 20.0F;
    }

    // ── Technique state ────────────────────────────────────────────────────
    DrawLine(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(textX + textW),
             static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    const nen::TechniqueState &tech = app.player.techniques;
    const char *modeName = (tech.auraMode == nen::AuraMode::Zetsu) ? "Zetsu"
                         : (tech.auraMode == nen::AuraMode::Ren)   ? "Ren"
                                                                    : "Ten";
    const Color modeColor = (tech.auraMode == nen::AuraMode::Zetsu) ? Color{200, 200, 200, 255}
                          : (tech.auraMode == nen::AuraMode::Ren)   ? Color{255, 120, 80, 255}
                                                                     : Color{120, 200, 255, 255};
    DrawText(TextFormat("Mode: %s  Dmg x%.2f  Def x%.2f", modeName,
                        app.cachedNenStats.damageMultiplier, app.cachedNenStats.defenseMultiplier),
             static_cast<int>(textX), static_cast<int>(y), 16, modeColor);
    y += 20.0F;

    // Active overlays inline
    {
        std::string overlays;
        if (tech.gyoActive) { overlays += "Gyo "; }
        if (tech.enActive)  { overlays += "En "; }
        if (tech.koPrepared) {
            overlays += TextFormat("Ko:%.1fs ", tech.koCharge);
        }
        if (!overlays.empty()) {
            DrawText(overlays.c_str(), static_cast<int>(textX), static_cast<int>(y), 16,
                     {220, 220, 100, 255});
            y += 20.0F;
        }
    }

    // ── Hatsu ──────────────────────────────────────────────────────────────
    DrawLine(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(textX + textW),
             static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    y = DrawWrappedText(app.player.hatsuName, {textX, y, textW, 44.0F}, 18, 1.0F, RAYWHITE, 2);
    y = DrawWrappedText(nen::HatsuAbilityName(app.player.naturalType),
                        {textX, y + 2.0F, textW, 22.0F}, 16, 1.0F, LIGHTGRAY, 1);
    DrawText(TextFormat("Potency %d  |  Base: %s", app.player.hatsuPotency,
                        nen::ToString(app.selectedBaseType).data()),
             static_cast<int>(textX), static_cast<int>(y + 2.0F), 16, Fade(WHITE, 0.65F));
    y += 22.0F;

    // Vow constraints
    if (!app.cachedHatsuSpec.vows.empty()) {
        DrawText("Vows", static_cast<int>(textX), static_cast<int>(y), 16, {220, 180, 80, 255});
        y += 18.0F;
        for (const auto &vow : app.cachedHatsuSpec.vows) {
            y = DrawWrappedText(vow.description, {textX + 6.0F, y, textW - 6.0F, 20.0F}, 15, 1.0F,
                                Fade({220, 180, 80, 255}, 0.85F), 1);
        }
        DrawText(TextFormat("x%.2f potency", nen::ComputeVowMultiplier(app.cachedHatsuSpec)),
                 static_cast<int>(textX), static_cast<int>(y), 15, Fade({220, 180, 80, 255}, 0.7F));
        y += 18.0F;
    }

    // ── Model/Anim ─────────────────────────────────────────────────────────
    DrawLine(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(textX + textW),
             static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    const Color modelStatusColor =
        app.hasPlayerModel ? Color{150, 234, 170, 255} : Color{250, 190, 128, 255};
    DrawText(TextFormat("Model: %s  Scale: %.2f", app.hasPlayerModel ? "OK" : "fallback",
                        app.playerModelScale),
             static_cast<int>(textX), static_cast<int>(y), 15, modelStatusColor);
    y += 18.0F;
    {
        const char *animMode = app.playerAnimationCount > 0 ? "clip" : "proc";
        DrawText(TextFormat("Anim: %s (%s)", animMode, ToString(app.animationState)),
                 static_cast<int>(textX), static_cast<int>(y), 15, Fade(WHITE, 0.55F));
        y += 18.0F;
    }

    // ── Combat ─────────────────────────────────────────────────────────────
    DrawLine(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(textX + textW),
             static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    DrawText(TextFormat("CD  base %.2fs  hatsu %.2fs",
                        app.baseAttackCooldown, app.hatsuCooldown),
             static_cast<int>(textX), static_cast<int>(y), 16, LIGHTGRAY);
    y += 20.0F;
    DrawText(TextFormat("Enemy HP: %d / %d", app.enemy.health, app.enemy.maxHealth),
             static_cast<int>(textX), static_cast<int>(y), 16, LIGHTGRAY);
    y += 20.0F;

    if (app.enemy.manipulatedTimer > 0.0F) {
        DrawText(TextFormat("Manipulated %.1fs", app.enemy.manipulatedTimer),
                 static_cast<int>(textX), static_cast<int>(y), 15, {255, 210, 120, 255});
        y += 18.0F;
    }
    if (app.enemy.vulnerabilityTimer > 0.0F) {
        DrawText(TextFormat("Vulnerable %.1fs", app.enemy.vulnerabilityTimer),
                 static_cast<int>(textX), static_cast<int>(y), 15, {255, 121, 210, 255});
        y += 18.0F;
    }
    if (app.enemy.elasticTimer > 0.0F) {
        DrawText(TextFormat("Elastic %.1fs", app.enemy.elasticTimer),
                 static_cast<int>(textX), static_cast<int>(y), 15, {133, 228, 255, 255});
        y += 18.0F;
    }

    // ── Hatsu description ──────────────────────────────────────────────────
    DrawLine(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(textX + textW),
             static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    DrawText("Hatsu", static_cast<int>(textX), static_cast<int>(y), 16, RAYWHITE);
    y += 20.0F;
    y = DrawWrappedText(nen::HatsuAbilityDescription(app.player.naturalType),
                        {textX, y, textW, 48.0F}, 15, 1.0F, Fade(WHITE, 0.70F), 3);
    y += 4.0F;

    // ── Controls ───────────────────────────────────────────────────────────
    DrawLine(static_cast<int>(textX), static_cast<int>(y), static_cast<int>(textX + textW),
             static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    if (y < bottom - 20.0F) {
        DrawText("Controls", static_cast<int>(textX), static_cast<int>(y), 16, RAYWHITE);
        y += 20.0F;
        const char *controlLines[] = {
            "WASD move  |  LMB/SPC attack  |  RMB/Q hatsu",
            "1-6 base type  |  R recharge aura",
            "E Ren  Z Zetsu  G Gyo  N En  K Ko",
            "MMB orbit  |  wheel zoom  |  +/- scale",
            "F5 save  |  ESC back",
        };
        for (const char *line : controlLines) {
            if (y >= bottom - 16.0F) { break; }
            y = DrawWrappedText(line, {textX, y, textW, 18.0F}, 14, 1.0F,
                                Fade(WHITE, 0.50F), 1);
        }
    }

    EndScissorMode();
}

} // namespace

// Redirect raylib's internal trace log through our logger.
// Suppresses the verbose INFO flood (VAO uploads, texture IDs, etc.) and
// formats WARN/ERROR lines consistently with the rest of the game output.
void RaylibTraceCallback(int logLevel, const char *text, va_list args) {
    char buf[512];
    std::vsnprintf(buf, sizeof(buf), text, args);
    switch (logLevel) {
    case LOG_WARNING: NEN_WARN("[raylib] %s", buf); break;
    case LOG_ERROR:   NEN_ERROR("[raylib] %s", buf); break;
    // Silently drop LOG_INFO and LOG_DEBUG — too noisy for normal use.
    // Change sMinLevel to Debug to see them.
    default: break;
    }
}

int Run() {
    // Suppress raylib's own console spam; we handle WARN/ERROR via our callback.
    SetTraceLogCallback(RaylibTraceCallback);
    SetTraceLogLevel(LOG_WARNING);

    NEN_INFO("nen_world starting up");
    InitWindow(kWidth, kHeight, "Nen World - Nen Combat Prototype");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    AppState app;
    TryLoadPlayerModel(&app);
    if (app.hasPlayerModel) {
        NEN_INFO("model loaded: %s  scale=%.4f", app.playerModelPath.c_str(),
                 app.playerModelScale);
    } else {
        NEN_WARN("no character model found — using fallback geometry");
    }
    RefreshStoredCharacters(&app);
    NEN_INFO("save directory: %s  (%zu characters found)",
             app.saveDir.string().c_str(), app.storedCharacters.size());

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
    UnloadPlayerModel(&app);
    CloseWindow();
    return 0;
}

} // namespace game
