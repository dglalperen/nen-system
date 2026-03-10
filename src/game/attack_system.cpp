#include "game/attack_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace game {

namespace {

constexpr float kEpsilon = 0.001F;

Vector2 NormalizeSafe(Vector2 input) {
    const float length = std::sqrt(input.x * input.x + input.y * input.y);
    if (length <= kEpsilon) {
        return {1.0F, 0.0F};
    }
    return {input.x / length, input.y / length};
}

Vector2 Rotate(Vector2 vector, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {vector.x * c - vector.y * s, vector.x * s + vector.y * c};
}

Color AttackColor(nen::Type type) {
    switch (type) {
    case nen::Type::Enhancer:
        return {108, 222, 120, 255};
    case nen::Type::Transmuter:
        return {79, 201, 255, 255};
    case nen::Type::Emitter:
        return {253, 117, 82, 255};
    case nen::Type::Conjurer:
        return {176, 140, 255, 255};
    case nen::Type::Manipulator:
        return {255, 195, 89, 255};
    case nen::Type::Specialist:
        return {255, 95, 191, 255};
    }
    return WHITE;
}

AttackEffect MakeEffect(nen::Type type, bool hatsu, Vector2 origin, Vector2 target,
                        Vector2 velocity, float radius, float life, int damage, float phase = 0.0F,
                        float manipulation = 0.0F, float vulnerability = 0.0F,
                        float homingStrength = 0.0F) {
    return AttackEffect{
        .type = type,
        .hatsu = hatsu,
        .origin = origin,
        .target = target,
        .position = origin,
        .velocity = velocity,
        .homingStrength = homingStrength,
        .radius = radius,
        .lifetime = 0.0F,
        .maxLifetime = life,
        .phase = phase,
        .damage = damage,
        .hasHit = false,
        .manipulationSeconds = manipulation,
        .vulnerabilitySeconds = vulnerability,
    };
}

void SpawnEnhancer(std::vector<AttackEffect> *effects, bool hatsu, Vector2 origin, Vector2 target,
                   int damage) {
    effects->push_back(MakeEffect(nen::Type::Enhancer, hatsu, origin, target, {0.0F, 0.0F},
                                  hatsu ? 34.0F : 24.0F, hatsu ? 0.34F : 0.24F,
                                  hatsu ? static_cast<int>(damage * 1.3F) : damage));
}

void SpawnTransmuter(std::vector<AttackEffect> *effects, bool hatsu, Vector2 origin, Vector2 target,
                     int damage) {
    const Vector2 direction = NormalizeSafe({target.x - origin.x, target.y - origin.y});
    effects->push_back(MakeEffect(
        nen::Type::Transmuter, hatsu, origin, target,
        {direction.x * (hatsu ? 610.0F : 500.0F), direction.y * (hatsu ? 610.0F : 500.0F)},
        hatsu ? 12.0F : 10.0F, hatsu ? 1.25F : 1.0F, damage,
        static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD, hatsu ? 1.1F : 0.0F,
        hatsu ? 1.0F : 0.0F, hatsu ? 0.45F : 0.0F));
    if (hatsu) {
        const Vector2 alt = Rotate(direction, 0.2F);
        effects->push_back(MakeEffect(nen::Type::Transmuter, true, origin, target,
                                      {alt.x * 540.0F, alt.y * 540.0F}, 10.0F, 1.1F,
                                      static_cast<int>(damage * 0.8F),
                                      static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD));
    }
}

void SpawnEmitter(std::vector<AttackEffect> *effects, bool hatsu, Vector2 origin, Vector2 target,
                  int damage) {
    const Vector2 direction = NormalizeSafe({target.x - origin.x, target.y - origin.y});
    effects->push_back(MakeEffect(
        nen::Type::Emitter, hatsu, origin, target,
        {direction.x * (hatsu ? 890.0F : 700.0F), direction.y * (hatsu ? 890.0F : 700.0F)},
        hatsu ? 14.0F : 9.0F, hatsu ? 0.92F : 0.78F,
        hatsu ? static_cast<int>(damage * 1.2F) : damage));
    if (hatsu) {
        effects->push_back(MakeEffect(nen::Type::Emitter, true, origin, target,
                                      {direction.x * 760.0F, direction.y * 760.0F}, 10.0F, 0.95F,
                                      static_cast<int>(damage * 0.55F), 0.0F, 0.0F, 0.0F, 0.0F));
    }
}

void SpawnConjurer(std::vector<AttackEffect> *effects, bool hatsu, Vector2 origin, Vector2 target,
                   int damage) {
    const Vector2 direction = NormalizeSafe({target.x - origin.x, target.y - origin.y});
    const int count = hatsu ? 3 : 1;
    for (int i = 0; i < count; ++i) {
        const float angle = hatsu ? (-0.18F + static_cast<float>(i) * 0.18F) : 0.0F;
        const Vector2 v = Rotate(direction, angle);
        effects->push_back(MakeEffect(nen::Type::Conjurer, hatsu, origin, target,
                                      {v.x * 410.0F, v.y * 410.0F}, hatsu ? 14.0F : 12.0F, 1.45F,
                                      hatsu ? static_cast<int>(damage * 0.7F) : damage, angle, 0.0F,
                                      0.0F, hatsu ? 1.25F : 0.8F));
    }
}

void SpawnManipulator(std::vector<AttackEffect> *effects, bool hatsu, Vector2 origin,
                      Vector2 target, int damage) {
    const Vector2 direction = NormalizeSafe({target.x - origin.x, target.y - origin.y});
    effects->push_back(MakeEffect(
        nen::Type::Manipulator, hatsu, origin, target,
        {direction.x * (hatsu ? 350.0F : 300.0F), direction.y * (hatsu ? 350.0F : 300.0F)},
        hatsu ? 16.0F : 12.0F, hatsu ? 1.5F : 1.35F, damage, 0.0F, hatsu ? 3.5F : 1.9F,
        hatsu ? 3.5F : 1.8F));
}

void SpawnSpecialist(std::vector<AttackEffect> *effects, bool hatsu, Vector2 origin, Vector2 target,
                     int damage) {
    const Vector2 direction = NormalizeSafe({target.x - origin.x, target.y - origin.y});
    const int shardCount = hatsu ? 5 : 3;
    for (int i = 0; i < shardCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(std::max(1, shardCount - 1));
        const float angle = -0.35F + t * 0.7F;
        const Vector2 v = Rotate(direction, angle);
        effects->push_back(MakeEffect(
            nen::Type::Specialist, hatsu, origin, target, {v.x * 530.0F, v.y * 530.0F},
            hatsu ? 10.0F : 8.0F, 1.12F,
            hatsu ? static_cast<int>(damage * 0.55F) : static_cast<int>(damage * 0.45F), angle));
    }
}

void SpawnForType(std::vector<AttackEffect> *effects, nen::Type type, bool hatsu, Vector2 origin,
                  Vector2 target, int damage) {
    switch (type) {
    case nen::Type::Enhancer:
        SpawnEnhancer(effects, hatsu, origin, target, damage);
        break;
    case nen::Type::Transmuter:
        SpawnTransmuter(effects, hatsu, origin, target, damage);
        break;
    case nen::Type::Emitter:
        SpawnEmitter(effects, hatsu, origin, target, damage);
        break;
    case nen::Type::Conjurer:
        SpawnConjurer(effects, hatsu, origin, target, damage);
        break;
    case nen::Type::Manipulator:
        SpawnManipulator(effects, hatsu, origin, target, damage);
        break;
    case nen::Type::Specialist:
        SpawnSpecialist(effects, hatsu, origin, target, damage);
        break;
    }
}

} // namespace

void SpawnBaseAttack(std::vector<AttackEffect> *effects, nen::Type attackType, Vector2 origin,
                     Vector2 target, int damage) {
    if (effects == nullptr) {
        return;
    }
    SpawnForType(effects, attackType, false, origin, target, damage);
}

void SpawnHatsuAttack(std::vector<AttackEffect> *effects, nen::Type attackType, Vector2 origin,
                      Vector2 target, int damage) {
    if (effects == nullptr) {
        return;
    }
    SpawnForType(effects, attackType, true, origin, target, damage);
}

AttackOutcome UpdateAttackEffects(std::vector<AttackEffect> *effects, float dt,
                                  Vector2 targetCenter, float targetRadius) {
    AttackOutcome outcome;
    if (effects == nullptr) {
        return outcome;
    }

    for (auto &effect : *effects) {
        effect.lifetime += dt;
        effect.phase += dt * 8.0F;

        switch (effect.type) {
        case nen::Type::Enhancer:
            effect.radius += (effect.hatsu ? 480.0F : 390.0F) * dt;
            effect.position = effect.origin;
            break;
        case nen::Type::Transmuter: {
            effect.position.x += effect.velocity.x * dt;
            effect.position.y += effect.velocity.y * dt;
            const Vector2 direction = NormalizeSafe(effect.velocity);
            const Vector2 tangent{-direction.y, direction.x};
            if (effect.homingStrength > 0.001F) {
                const Vector2 toTarget = NormalizeSafe(
                    {effect.target.x - effect.position.x, effect.target.y - effect.position.y});
                effect.velocity.x += toTarget.x * effect.homingStrength * 160.0F * dt;
                effect.velocity.y += toTarget.y * effect.homingStrength * 160.0F * dt;
            }
            const float sway = std::sin(effect.phase * 7.0F) * (effect.hatsu ? 170.0F : 95.0F) * dt;
            effect.position.x += tangent.x * sway;
            effect.position.y += tangent.y * sway;
            break;
        }
        case nen::Type::Emitter:
            effect.position.x += effect.velocity.x * dt;
            effect.position.y += effect.velocity.y * dt;
            break;
        case nen::Type::Conjurer:
            effect.position.x += effect.velocity.x * dt;
            effect.position.y += effect.velocity.y * dt;
            if (effect.homingStrength > 0.001F) {
                const Vector2 toTarget = NormalizeSafe(
                    {effect.target.x - effect.position.x, effect.target.y - effect.position.y});
                effect.velocity.x += toTarget.x * effect.homingStrength * 170.0F * dt;
                effect.velocity.y += toTarget.y * effect.homingStrength * 170.0F * dt;
            }
            effect.velocity.x *= 0.988F;
            effect.velocity.y *= 0.988F;
            break;
        case nen::Type::Manipulator:
            effect.velocity = Rotate(effect.velocity, 0.92F * dt);
            effect.position.x += effect.velocity.x * dt;
            effect.position.y += effect.velocity.y * dt;
            break;
        case nen::Type::Specialist:
            effect.velocity = Rotate(effect.velocity, std::sin(effect.phase) * 0.04F);
            effect.position.x += effect.velocity.x * dt;
            effect.position.y += effect.velocity.y * dt;
            break;
        }

        if (effect.hasHit) {
            continue;
        }

        const float distance =
            std::sqrt((effect.position.x - targetCenter.x) * (effect.position.x - targetCenter.x) +
                      (effect.position.y - targetCenter.y) * (effect.position.y - targetCenter.y));
        if (distance > effect.radius + targetRadius) {
            continue;
        }

        effect.hasHit = true;
        outcome.damage += effect.damage;
        outcome.manipulationSeconds =
            std::max(outcome.manipulationSeconds, effect.manipulationSeconds);
        outcome.vulnerabilitySeconds =
            std::max(outcome.vulnerabilitySeconds, effect.vulnerabilitySeconds);

        if (effect.type == nen::Type::Enhancer) {
            effect.maxLifetime = std::min(effect.maxLifetime, effect.lifetime + 0.08F);
        } else {
            effect.maxLifetime = effect.lifetime;
        }
    }

    effects->erase(std::remove_if(effects->begin(), effects->end(),
                                  [](const AttackEffect &effect) {
                                      return effect.lifetime >= effect.maxLifetime;
                                  }),
                   effects->end());

    return outcome;
}

void DrawAttackEffects(const std::vector<AttackEffect> &effects) {
    for (const auto &effect : effects) {
        const float lifeRatio = 1.0F - (effect.lifetime / effect.maxLifetime);
        const Color baseColor = AttackColor(effect.type);
        const Color soft = Fade(baseColor, std::clamp(lifeRatio, 0.16F, 1.0F));
        const float hatsuScale = effect.hatsu ? 1.25F : 1.0F;

        switch (effect.type) {
        case nen::Type::Enhancer:
            DrawCircleLinesV(effect.position, effect.radius, soft);
            DrawCircleV(effect.position, effect.radius * 0.38F,
                        Fade(baseColor, 0.24F * hatsuScale));
            break;
        case nen::Type::Transmuter: {
            const Vector2 direction = NormalizeSafe(effect.velocity);
            const Vector2 start{effect.position.x - direction.x * 28.0F,
                                effect.position.y - direction.y * 28.0F};
            const Vector2 end{effect.position.x + direction.x * 12.0F,
                              effect.position.y + direction.y * 12.0F};
            DrawLineEx(start, end, 4.2F * hatsuScale, soft);
            DrawCircleV(effect.position, effect.radius, Fade(baseColor, 0.55F));
            break;
        }
        case nen::Type::Emitter:
            DrawCircleV(effect.position, effect.radius * 1.65F, Fade(baseColor, 0.18F));
            DrawCircleV(effect.position, effect.radius, soft);
            DrawCircleLinesV(effect.position, effect.radius * 2.0F, Fade(baseColor, 0.35F));
            break;
        case nen::Type::Conjurer:
            DrawPoly(effect.position, 4, effect.radius * 1.7F, effect.phase * 320.0F, soft);
            DrawPolyLinesEx(effect.position, 4, effect.radius * 1.7F, effect.phase * 320.0F, 2.0F,
                            WHITE);
            break;
        case nen::Type::Manipulator: {
            DrawCircleV(effect.position, effect.radius, soft);
            const float orbit = effect.radius * 1.9F;
            DrawCircle(effect.position.x + std::cos(effect.phase * 7.0F) * orbit,
                       effect.position.y + std::sin(effect.phase * 7.0F) * orbit, 3.2F,
                       Fade(baseColor, 0.9F));
            DrawCircle(effect.position.x + std::cos(effect.phase * 7.0F + PI) * orbit,
                       effect.position.y + std::sin(effect.phase * 7.0F + PI) * orbit, 3.2F,
                       Fade(baseColor, 0.8F));
            break;
        }
        case nen::Type::Specialist:
            DrawPoly(effect.position, 6, effect.radius * 1.9F, effect.phase * 260.0F, soft);
            DrawCircleV(effect.position, effect.radius * 0.75F, Fade(baseColor, 0.85F));
            break;
        }
    }
}

} // namespace game
