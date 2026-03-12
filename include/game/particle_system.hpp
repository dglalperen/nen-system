#pragma once
#include <array>
#include <cstdint>
#include <raylib.h>

namespace game {

// ── Particle ──────────────────────────────────────────────────────────────────

struct Particle {
    Vector3  position{};
    Vector3  velocity{};
    Color    color       = WHITE;
    float    lifetime    = 0.0F;
    float    maxLifetime = 1.0F;
    float    sizeStart   = 0.06F;
    float    sizeEnd     = 0.01F;
    uint16_t emitterSlot = 0xFFFFu; // 0xFFFF = burst (no emitter)
    uint8_t  blendMode   = 0;       // 0=alpha  1=additive
    uint8_t  _pad        = 0;       // NOLINT(readability-identifier-naming)
};
// sizeof(Particle) == 44 bytes; 1024 particles = 44 KB (fits L2)

constexpr int kMaxParticles = 1024;
constexpr int kMaxEmitters  = 64;

// ── Emitter ───────────────────────────────────────────────────────────────────

enum class EmitterShape : uint8_t { Point, Ring, Cone };
enum class EmitterBlend : uint8_t { Alpha, Additive };

struct EmitterConfig {
    EmitterShape shape       = EmitterShape::Point;
    float        radius      = 0.0F;
    float        arc         = 6.2832F;     // Ring: arc in radians (2π = full ring)
    Vector3      direction   = {0, 1, 0};   // Cone/Point: base velocity direction
    float        coneAngle   = 0.6F;        // Cone: half-angle (radians)
    float        speedMin    = 0.10F;
    float        speedMax    = 0.40F;
    Vector3      gravity     = {0, 0, 0};
    Color        colorStart  = WHITE;
    float        sizeStart   = 0.06F;
    float        sizeEnd     = 0.01F;
    EmitterBlend blend       = EmitterBlend::Alpha;
    float        lifetimeMin = 0.6F;
    float        lifetimeMax = 1.2F;
    float        spawnRate   = 10.0F;   // particles/sec; ignored when burstCount>0
    int          burstCount  = 0;       // >0: emit exactly N on BurstAt(), ignore rate
    float        emitterLife = -1.0F;   // -1 = infinite; >0 = self-destruct after N sec
};

// ── Handle ────────────────────────────────────────────────────────────────────

struct EmitterHandle {
    uint32_t value = 0;

    static constexpr uint32_t kSlotBits = 10u;
    static constexpr uint32_t kSlotMask = (1u << kSlotBits) - 1u;

    static EmitterHandle Make(uint32_t slot, uint32_t gen) noexcept {
        return EmitterHandle{(gen << kSlotBits) | (slot & kSlotMask)};
    }
    uint32_t Slot()   const noexcept { return value & kSlotMask; }
    uint32_t Gen()    const noexcept { return value >> kSlotBits; }
    bool     IsNull() const noexcept { return value == 0u; }
};
constexpr EmitterHandle kNullEmitter{};

// ── Internal emitter state (not part of public API) ──────────────────────────

struct EmitterState {
    EmitterConfig config{};
    Vector3       origin{};
    float         accumulator = 0.0F;
    float         age         = 0.0F;
    uint32_t      generation  = 1u; // 0 reserved as "null generation"
    bool          active      = false;
};

// ── System ────────────────────────────────────────────────────────────────────

struct ParticleSystem {
    std::array<Particle,     kMaxParticles> particles{};
    std::array<EmitterState, kMaxEmitters>  emitters{};
    int       particleCount = 0;
    Texture2D particleTex{};
    bool      texLoaded = false;
};

// ── API ───────────────────────────────────────────────────────────────────────

void InitParticleSystem  (ParticleSystem *ps);
void UnloadParticleSystem(ParticleSystem *ps);

// Continuous emitter — returned handle lets you move/stop it
EmitterHandle SpawnEmitter(ParticleSystem *ps, const EmitterConfig &cfg, Vector3 origin);
void          MoveEmitter (ParticleSystem *ps, EmitterHandle h, Vector3 newOrigin);
void          KillEmitter (ParticleSystem *ps, EmitterHandle h);

// One-shot burst at a world position (no handle)
void BurstAt(ParticleSystem *ps, const EmitterConfig &cfg, Vector3 position);

void UpdateParticleSystem(ParticleSystem *ps, float dt);
void DrawParticleSystem  (const ParticleSystem &ps, const Camera3D &cam);

int  ActiveParticleCount (const ParticleSystem &ps);

// ── Pre-built nen effect configs ──────────────────────────────────────────────

EmitterConfig MakeAuraTenConfig            (Color typeColor);
EmitterConfig MakeAuraRenConfig            (Color typeColor);
EmitterConfig MakeKoChargeConfig           (Color typeColor);
EmitterConfig MakeTransitionBurstConfig    (Color typeColor);
EmitterConfig MakeAttackTrailConfig        (Color typeColor);
EmitterConfig MakeImpactSmokeConfig        (Color typeColor);
EmitterConfig MakeImpactFlashConfig        (Color typeColor);

} // namespace game
