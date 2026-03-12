#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <raylib.h>

#include "game/attack_system.hpp"
#include "game/game_constants.hpp"
#include "game/particle_system.hpp"
#include "game/persistence.hpp"
#include "nen/hatsu_spec.hpp"
#include "nen/nen_system.hpp"
#include "nen/quiz.hpp"
#include "nen/types.hpp"

namespace game {

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
    bool    queued         = false;
    bool    hatsu          = false;
    bool    fired          = false;
    Vector2 target         = {0.0F, 0.0F};
    float   timer          = 0.0F;
    float   triggerSeconds = 0.14F;
    float   finishSeconds  = 0.32F;
};

struct EnemyState {
    Vector2 position          = {810.0F, 450.0F};
    Vector2 velocity          = {180.0F, 120.0F};
    float   radius            = 42.0F;
    int     maxHealth         = 430;
    int     health            = 430;
    float   manipulatedTimer  = 0.0F;
    float   vulnerabilityTimer = 0.0F;
    float   elasticTimer      = 0.0F;
    float   elasticStrength   = 0.0F;
    float   hitFlashTimer     = 0.0F;
};

struct AppState {
    Screen screen       = Screen::MainMenu;
    int    menuSelection = 0;
    int    loadSelection = 0;

    std::filesystem::path        saveDir = DefaultSaveDirectory();
    std::vector<StoredCharacter> storedCharacters;

    std::string    draftName;
    nen::QuizScores quizScores{};
    std::size_t    quizQuestionIndex = 0;
    float          revealTimer       = 0.0F;

    // HatsuCreation state
    std::string draftHatsuName;
    int         selectedHatsuCategoryIndex = 0; // index into HatsuCategory enum (0..5)
    uint32_t    selectedVowMask            = 0; // bitmask for 12 canonical vows

    nen::Character player{
        .name        = "",
        .naturalType = nen::Type::Enhancer,
        .auraPool    = nen::AuraPool{.current = 120.0F},
    };
    nen::DerivedNenStats cachedNenStats{};
    nen::HatsuSpec       cachedHatsuSpec{};

    ParticleSystem particleSystem{};
    EmitterHandle  nenAuraEmitter{};
    EmitterHandle  koEmitter{};
    EmitterHandle  chargeEmitter{};

    bool    hasCharacter = false;
    Vector2 playerPosition{kArenaBounds.x + 110.0F,
                           kArenaBounds.y + kArenaBounds.height - 130.0F};
    Vector2 playerSize{36.0F, 52.0F};
    Vector2 facingDirection{0.0F, -1.0F};
    bool    chargingAura     = false;
    float   chargeEffectTimer = 0.0F;
    nen::Type selectedBaseType = nen::Type::Enhancer;

    float                   baseAttackCooldown = 0.0F;
    float                   hatsuCooldown      = 0.0F;
    std::vector<AttackEffect> activeAttacks;
    EnemyState              enemy;
    Model                   playerModel{};
    bool                    hasPlayerModel        = false;
    float                   playerModelScale      = 1.0F;
    Vector3                 playerModelPivot      = {0.0F, 0.0F, 0.0F};
    float                   playerModelAuraRadius = 0.5F;
    float                   playerModelYawOffset  = 180.0F;
    bool                    playerModelHasTexture = false;
    std::string             playerModelPath;
    std::string             playerModelStatus = "Model not loaded";
    float                   cameraOrbitAngle  = 0.88F;
    float                   cameraDistance    = 15.0F;
    float                   cameraHeight      = 9.0F;
    float                   playerMoveSpeed   = 0.0F;
    float                   proceduralAnimTime = 0.0F;
    float                   modelBobOffset    = 0.0F;
    float                   modelStrideOffset = 0.0F;
    float                   modelLeanDegrees  = 0.0F;
    float                   modelCastLeanDegrees = 0.0F;
    float                   modelScalePulse   = 0.0F;
    ModelAnimation         *playerAnimations     = nullptr;
    int                     playerAnimationCount = 0;
    int                     idleAnimationIndex   = -1;
    int                     moveAnimationIndex   = -1;
    int                     chargeAnimationIndex = -1;
    int                     baseCastAnimationIndex  = -1;
    int                     hatsuCastAnimationIndex = -1;
    int                     activeAnimationIndex    = -1;
    float                   activeAnimationFrame    = 0.0F;
    AnimState               animationState = AnimState::Idle;
    QueuedAction            queuedAction{};
    Camera3D                camera{
        .position   = {0.0F, 13.0F, 16.0F},
        .target     = {0.0F, 0.0F, 0.0F},
        .up         = {0.0F, 1.0F, 0.0F},
        .fovy       = 52.0F,
        .projection = CAMERA_PERSPECTIVE,
    };

    std::string statusMessage = "Select New Character or Load Character.";
};

} // namespace game
