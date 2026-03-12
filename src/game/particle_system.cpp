#include "game/particle_system.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include <raylib.h>
#include <raymath.h>

namespace game {

namespace {

// ── Particle pool helpers ─────────────────────────────────────────────────────

bool SpawnParticle(ParticleSystem *ps, const Particle &p) {
    if (ps->particleCount >= kMaxParticles) {
        return false;
    }
    ps->particles[static_cast<std::size_t>(ps->particleCount)] = p;
    ps->particleCount += 1;
    return true;
}

// ── Per-shape spawn helpers ───────────────────────────────────────────────────

void SpawnFromEmitter(ParticleSystem *ps, int emitterIdx) {
    EmitterState &es  = ps->emitters[static_cast<std::size_t>(emitterIdx)];
    const EmitterConfig &cfg = es.config;

    Particle p{};
    p.color       = cfg.colorStart;
    p.sizeStart   = cfg.sizeStart;
    p.sizeEnd     = cfg.sizeEnd;
    p.blendMode   = (cfg.blend == EmitterBlend::Additive) ? 1u : 0u;
    p.emitterSlot = static_cast<uint16_t>(emitterIdx);
    p.lifetime    = 0.0F;
    p.maxLifetime = cfg.lifetimeMin + static_cast<float>(GetRandomValue(0, 1000)) / 1000.0F *
                    (cfg.lifetimeMax - cfg.lifetimeMin);

    const float speed = cfg.speedMin + static_cast<float>(GetRandomValue(0, 1000)) / 1000.0F *
                        (cfg.speedMax - cfg.speedMin);

    switch (cfg.shape) {
    case EmitterShape::Point: {
        // Random spherical direction, biased toward cfg.direction
        const float theta = static_cast<float>(GetRandomValue(0, 628)) * 0.01F; // 0..2π
        const float phi   = static_cast<float>(GetRandomValue(0, 314)) * 0.01F; // 0..π
        p.position  = es.origin;
        p.velocity  = {
            std::sin(phi) * std::cos(theta) * speed,
            std::sin(phi) * std::sin(theta) * speed,
            std::cos(phi) * speed,
        };
        break;
    }
    case EmitterShape::Ring: {
        const float angle = static_cast<float>(GetRandomValue(0, static_cast<int>(cfg.arc * 100.0F))) / 100.0F;
        const float r     = cfg.radius;
        p.position = {
            es.origin.x + std::cos(angle) * r,
            es.origin.y,
            es.origin.z + std::sin(angle) * r,
        };
        // Outward radial velocity + upward bias
        const float upBias = static_cast<float>(GetRandomValue(10, 35)) * 0.01F;
        p.velocity = {
            std::cos(angle) * speed,
            upBias,
            std::sin(angle) * speed,
        };
        break;
    }
    case EmitterShape::Cone: {
        // Inward convergence toward origin (used for Ko charge)
        const float theta = static_cast<float>(GetRandomValue(0, 628)) * 0.01F;
        const float phi   = static_cast<float>(GetRandomValue(0, static_cast<int>(cfg.coneAngle * 100.0F))) / 100.0F;
        const float r     = cfg.radius;
        // Spawn on sphere of radius r, velocity inward
        const Vector3 spawnPos = {
            es.origin.x + std::sin(phi) * std::cos(theta) * r,
            es.origin.y + std::cos(phi) * r,
            es.origin.z + std::sin(phi) * std::sin(theta) * r,
        };
        p.position = spawnPos;
        // Direction toward origin
        const float dx = es.origin.x - spawnPos.x;
        const float dy = es.origin.y - spawnPos.y;
        const float dz = es.origin.z - spawnPos.z;
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len > 0.0001F) {
            p.velocity = {dx / len * speed, dy / len * speed, dz / len * speed};
        } else {
            p.velocity = {0.0F, speed, 0.0F};
        }
        break;
    }
    }

    // Apply gravity once to velocity direction (gravity is a constant accel — applied per tick)
    SpawnParticle(ps, p);
}

// ── Emitter free-list ─────────────────────────────────────────────────────────

int FindFreeEmitter(const ParticleSystem *ps) {
    for (int i = 0; i < kMaxEmitters; ++i) {
        if (!ps->emitters[static_cast<std::size_t>(i)].active) {
            return i;
        }
    }
    return -1;
}

bool ValidateHandle(const ParticleSystem *ps, EmitterHandle h) {
    if (h.IsNull()) {
        return false;
    }
    const auto slot = static_cast<int>(h.Slot());
    if (slot >= kMaxEmitters) {
        return false;
    }
    const EmitterState &es = ps->emitters[static_cast<std::size_t>(slot)];
    return es.active && es.generation == h.Gen();
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

void InitParticleSystem(ParticleSystem *ps) {
    if (ps == nullptr) {
        return;
    }
    ps->particleCount = 0;

    // Generate 32×32 RGBA soft-circle texture
    Image img = GenImageColor(32, 32, {0, 0, 0, 0});
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const float dx = static_cast<float>(x) - 15.5F;
            const float dy = static_cast<float>(y) - 15.5F;
            const float d  = std::sqrt(dx * dx + dy * dy) / 15.5F;
            const float a  = 1.0F - d * d;
            const float alpha = (a > 0.0F) ? (a * a * 255.0F) : 0.0F;
            const unsigned char alphaB =
                static_cast<unsigned char>(std::clamp(alpha, 0.0F, 255.0F));
            ImageDrawPixel(&img, x, y, {255, 255, 255, alphaB});
        }
    }
    ps->particleTex = LoadTextureFromImage(img);
    UnloadImage(img);
    ps->texLoaded = (ps->particleTex.id > 0);
}

void UnloadParticleSystem(ParticleSystem *ps) {
    if (ps == nullptr) {
        return;
    }
    if (ps->texLoaded) {
        UnloadTexture(ps->particleTex);
        ps->texLoaded = false;
    }
    ps->particleCount = 0;
}

EmitterHandle SpawnEmitter(ParticleSystem *ps, const EmitterConfig &cfg, Vector3 origin) {
    if (ps == nullptr) {
        return kNullEmitter;
    }
    const int slot = FindFreeEmitter(ps);
    if (slot < 0) {
        return kNullEmitter;
    }
    EmitterState &es = ps->emitters[static_cast<std::size_t>(slot)];
    es.config      = cfg;
    es.origin      = origin;
    es.accumulator = 0.0F;
    es.age         = 0.0F;
    es.active      = true;
    // Increment generation, skip 0 (null sentinel)
    es.generation = (es.generation + 1u == 0u) ? 1u : es.generation + 1u;
    return EmitterHandle::Make(static_cast<uint32_t>(slot), es.generation);
}

void MoveEmitter(ParticleSystem *ps, EmitterHandle h, Vector3 newOrigin) {
    if (ps == nullptr || !ValidateHandle(ps, h)) {
        return;
    }
    ps->emitters[h.Slot()].origin = newOrigin;
}

void KillEmitter(ParticleSystem *ps, EmitterHandle h) {
    if (ps == nullptr || !ValidateHandle(ps, h)) {
        return;
    }
    ps->emitters[h.Slot()].active = false;
}

void BurstAt(ParticleSystem *ps, const EmitterConfig &cfg, Vector3 position) {
    if (ps == nullptr) {
        return;
    }
    // Temporarily create an emitter state just for spawning
    EmitterState tempEs{};
    tempEs.config = cfg;
    tempEs.origin = position;

    const int count = cfg.burstCount > 0 ? cfg.burstCount : 1;

    // We need a slot to call SpawnFromEmitter, use a fake index trick:
    // Find a free slot or use -1 => set emitterSlot = 0xFFFF for burst
    const int freeSlot = FindFreeEmitter(ps);
    if (freeSlot >= 0) {
        ps->emitters[static_cast<std::size_t>(freeSlot)] = tempEs;
        ps->emitters[static_cast<std::size_t>(freeSlot)].active = true;
        for (int i = 0; i < count; ++i) {
            SpawnFromEmitter(ps, freeSlot);
        }
        // Mark particles as burst (no emitter)
        // The particles were just added to [particleCount - count .. particleCount - 1]
        const int start = ps->particleCount - count;
        for (int i = std::max(0, start); i < ps->particleCount; ++i) {
            ps->particles[static_cast<std::size_t>(i)].emitterSlot = 0xFFFFu;
        }
        ps->emitters[static_cast<std::size_t>(freeSlot)].active = false;
    }
}

void UpdateParticleSystem(ParticleSystem *ps, float dt) {
    if (ps == nullptr) {
        return;
    }

    // 1. Update emitters
    for (int i = 0; i < kMaxEmitters; ++i) {
        EmitterState &es = ps->emitters[static_cast<std::size_t>(i)];
        if (!es.active) {
            continue;
        }
        es.age += dt;
        if (es.config.emitterLife > 0.0F && es.age >= es.config.emitterLife) {
            es.active = false;
            continue;
        }
        if (es.config.burstCount > 0) {
            // burst emitters are one-shot via BurstAt — skip rate-based spawn
            continue;
        }
        if (es.config.spawnRate > 0.0F) {
            es.accumulator += es.config.spawnRate * dt;
            while (es.accumulator >= 1.0F) {
                es.accumulator -= 1.0F;
                SpawnFromEmitter(ps, i);
            }
        }
    }

    // 2. Update particles (slot-swap kill)
    for (int i = 0; i < ps->particleCount; ) {
        Particle &p = ps->particles[static_cast<std::size_t>(i)];
        p.lifetime += dt;

        // Apply gravity
        const uint16_t slot = p.emitterSlot;
        if (slot < kMaxEmitters) {
            const EmitterState &es = ps->emitters[static_cast<std::size_t>(slot)];
            p.velocity.x += es.config.gravity.x * dt;
            p.velocity.y += es.config.gravity.y * dt;
            p.velocity.z += es.config.gravity.z * dt;
        }

        // Integrate
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;

        if (p.lifetime >= p.maxLifetime) {
            // Slot-swap kill
            ps->particleCount -= 1;
            if (i < ps->particleCount) {
                ps->particles[static_cast<std::size_t>(i)] =
                    ps->particles[static_cast<std::size_t>(ps->particleCount)];
            }
            // Do NOT increment i — re-process this slot
        } else {
            ++i;
        }
    }
}

void DrawParticleSystem(const ParticleSystem &ps, const Camera3D &cam) {
    if (!ps.texLoaded) {
        return;
    }

    // Bucket into alpha and additive index arrays (stack-allocated)
    std::array<int, kMaxParticles> alphaIdx{};
    std::array<int, kMaxParticles> addIdx{};
    int alphaCount = 0;
    int addCount   = 0;

    for (int i = 0; i < ps.particleCount; ++i) {
        const Particle &p = ps.particles[static_cast<std::size_t>(i)];
        if (p.blendMode == 1) {
            addIdx[static_cast<std::size_t>(addCount++)] = i;
        } else {
            alphaIdx[static_cast<std::size_t>(alphaCount++)] = i;
        }
    }

    // Alpha pass
    for (int k = 0; k < alphaCount; ++k) {
        const Particle &p = ps.particles[static_cast<std::size_t>(alphaIdx[static_cast<std::size_t>(k)])];
        const float t       = (p.maxLifetime > 0.0F) ? (p.lifetime / p.maxLifetime) : 1.0F;
        const float size    = p.sizeStart + (p.sizeEnd - p.sizeStart) * t;
        const float alphaMul = 1.0F - t;
        Color c = p.color;
        c.a = static_cast<unsigned char>(
            std::clamp(static_cast<float>(c.a) * alphaMul, 0.0F, 255.0F));
        DrawBillboard(cam, ps.particleTex, p.position, size, c);
    }

    // Additive pass
    BeginBlendMode(BLEND_ADDITIVE);
    for (int k = 0; k < addCount; ++k) {
        const Particle &p = ps.particles[static_cast<std::size_t>(addIdx[static_cast<std::size_t>(k)])];
        const float t       = (p.maxLifetime > 0.0F) ? (p.lifetime / p.maxLifetime) : 1.0F;
        const float size    = p.sizeStart + (p.sizeEnd - p.sizeStart) * t;
        const float alphaMul = 1.0F - t;
        Color c = p.color;
        c.a = static_cast<unsigned char>(
            std::clamp(static_cast<float>(c.a) * alphaMul, 0.0F, 255.0F));
        DrawBillboard(cam, ps.particleTex, p.position, size, c);
    }
    EndBlendMode();
}

int ActiveParticleCount(const ParticleSystem &ps) {
    return ps.particleCount;
}

// ── Pre-built nen effect configs ──────────────────────────────────────────────

// Nen aura — manga-accurate white smoke rising from the body.
// typeColor is kept for hatsu/attack effects where the type manifests visually.
// Base aura (Ten/Ren/Ko) is near-white like vapor from skin.

// Subtle cool-white puff blending gently toward typeColor at low alpha
static Color AuraSmokeColor(Color typeColor) {
    // Near-white with a very faint type tint
    return Color{
        static_cast<unsigned char>(220 + (typeColor.r - 220) / 8),
        static_cast<unsigned char>(225 + (typeColor.g - 225) / 8),
        static_cast<unsigned char>(235 + (typeColor.b - 235) / 8),
        160,
    };
}

EmitterConfig MakeAuraTenConfig(Color typeColor) {
    // Ten: passive containment — barely visible wisps hugging the body.
    // Very close spawn (radius 0.18), slow upward drift, alpha smoke.
    EmitterConfig cfg{};
    cfg.shape       = EmitterShape::Ring;
    cfg.radius      = 0.18F;   // tight to body surface
    cfg.arc         = 6.2832F;
    cfg.spawnRate   = 6.0F;
    cfg.blend       = EmitterBlend::Alpha;
    cfg.speedMin    = 0.03F;
    cfg.speedMax    = 0.10F;
    cfg.lifetimeMin = 1.0F;
    cfg.lifetimeMax = 1.8F;
    cfg.sizeStart   = 0.06F;
    cfg.sizeEnd     = 0.02F;
    cfg.colorStart  = AuraSmokeColor(typeColor);
    cfg.emitterLife = -1.0F;
    return cfg;
}

EmitterConfig MakeAuraRenConfig(Color /*typeColor*/) {
    // Ren: amplified output — thick white aura bursting off the body,
    // denser and larger than Ten, additive so it glows brightly.
    EmitterConfig cfg{};
    cfg.shape       = EmitterShape::Ring;
    cfg.radius      = 0.22F;   // still close to body, just slightly wider
    cfg.arc         = 6.2832F;
    cfg.spawnRate   = 28.0F;
    cfg.blend       = EmitterBlend::Additive;
    cfg.speedMin    = 0.08F;
    cfg.speedMax    = 0.28F;
    cfg.lifetimeMin = 0.6F;
    cfg.lifetimeMax = 1.2F;
    cfg.sizeStart   = 0.10F;
    cfg.sizeEnd     = 0.01F;
    cfg.colorStart  = {245, 248, 255, 200};  // bright cool white
    cfg.emitterLife = -1.0F;
    return cfg;
}

EmitterConfig MakeKoChargeConfig(Color /*typeColor*/) {
    // Ko: concentrated — all aura focused to one point,
    // convergence shown as warm white streaks rushing inward.
    EmitterConfig cfg{};
    cfg.shape       = EmitterShape::Cone;
    cfg.radius      = 1.2F;
    cfg.coneAngle   = 3.14159F;
    cfg.spawnRate   = 30.0F;
    cfg.blend       = EmitterBlend::Additive;
    cfg.speedMin    = 1.0F;
    cfg.speedMax    = 2.4F;
    cfg.lifetimeMin = 0.15F;
    cfg.lifetimeMax = 0.4F;
    cfg.sizeStart   = 0.08F;
    cfg.sizeEnd     = 0.02F;
    cfg.colorStart  = {255, 252, 235, 220};  // warm white / very light gold
    cfg.emitterLife = -1.0F;
    return cfg;
}

EmitterConfig MakeTransitionBurstConfig(Color typeColor) {
    // Mode switch — white flash burst (type color just barely tinted)
    EmitterConfig cfg{};
    cfg.shape       = EmitterShape::Point;
    cfg.burstCount  = 24;
    cfg.blend       = EmitterBlend::Additive;
    cfg.speedMin    = 0.5F;
    cfg.speedMax    = 1.4F;
    cfg.lifetimeMin = 0.12F;
    cfg.lifetimeMax = 0.35F;
    cfg.sizeStart   = 0.14F;
    cfg.sizeEnd     = 0.0F;
    cfg.colorStart  = {245, 248, 255, 220};
    cfg.emitterLife = 0.4F;
    (void)typeColor;
    return cfg;
}

EmitterConfig MakeAttackTrailConfig(Color typeColor) {
    // Attack trail — type color shows here (hatsu manifests type)
    EmitterConfig cfg{};
    cfg.shape       = EmitterShape::Point;
    cfg.spawnRate   = 0.0F;
    cfg.burstCount  = 3;
    cfg.blend       = EmitterBlend::Additive;
    cfg.speedMin    = 0.02F;
    cfg.speedMax    = 0.08F;
    cfg.lifetimeMin = 0.08F;
    cfg.lifetimeMax = 0.20F;
    cfg.sizeStart   = 0.08F;
    cfg.sizeEnd     = 0.01F;
    cfg.colorStart  = typeColor;
    cfg.emitterLife = 0.25F;
    return cfg;
}

EmitterConfig MakeImpactSmokeConfig(Color typeColor) {
    // Impact smoke — white with slight type tint
    EmitterConfig cfg{};
    cfg.shape       = EmitterShape::Point;
    cfg.burstCount  = 16;
    cfg.blend       = EmitterBlend::Alpha;
    cfg.speedMin    = 0.25F;
    cfg.speedMax    = 0.70F;
    cfg.lifetimeMin = 0.30F;
    cfg.lifetimeMax = 0.65F;
    cfg.sizeStart   = 0.12F;
    cfg.sizeEnd     = 0.0F;
    cfg.colorStart  = AuraSmokeColor(typeColor);
    cfg.emitterLife = 0.7F;
    return cfg;
}

EmitterConfig MakeImpactFlashConfig(Color typeColor) {
    // Impact flash — full type color; this is hatsu energy releasing
    EmitterConfig cfg{};
    cfg.shape       = EmitterShape::Point;
    cfg.burstCount  = 10;
    cfg.blend       = EmitterBlend::Additive;
    cfg.speedMin    = 0.4F;
    cfg.speedMax    = 1.2F;
    cfg.lifetimeMin = 0.06F;
    cfg.lifetimeMax = 0.20F;
    cfg.sizeStart   = 0.20F;
    cfg.sizeEnd     = 0.0F;
    cfg.colorStart  = typeColor;
    cfg.emitterLife = 0.25F;
    return cfg;
}

} // namespace game
