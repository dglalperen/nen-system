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
    const float cosValue = std::cos(radians);
    const float sinValue = std::sin(radians);
    return {vector.x * cosValue - vector.y * sinValue, vector.x * sinValue + vector.y * cosValue};
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

AttackEffect CreateEffect(nen::Type type, Vector2 origin, Vector2 velocity, float radius,
                          float maxLifetime, int damage, float phase) {
    return AttackEffect{
        .type = type,
        .origin = origin,
        .position = origin,
        .velocity = velocity,
        .radius = radius,
        .lifetime = 0.0F,
        .maxLifetime = maxLifetime,
        .phase = phase,
        .damage = damage,
        .hasHit = false,
    };
}

} // namespace

void SpawnAttackEffect(std::vector<AttackEffect> *effects, nen::Type attackType, Vector2 origin,
                       Vector2 target, int damage) {
    if (effects == nullptr) {
        return;
    }

    const Vector2 direction = NormalizeSafe({target.x - origin.x, target.y - origin.y});

    switch (attackType) {
    case nen::Type::Enhancer:
        effects->push_back(
            CreateEffect(attackType, origin, {0.0F, 0.0F}, 20.0F, 0.28F, damage, 0.0F));
        break;
    case nen::Type::Transmuter:
        effects->push_back(
            CreateEffect(attackType, origin, {direction.x * 470.0F, direction.y * 470.0F}, 11.0F,
                         1.05F, damage, static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD));
        break;
    case nen::Type::Emitter:
        effects->push_back(CreateEffect(attackType, origin,
                                        {direction.x * 560.0F, direction.y * 560.0F}, 9.0F, 1.10F,
                                        damage, 0.0F));
        break;
    case nen::Type::Conjurer:
        effects->push_back(CreateEffect(attackType, origin,
                                        {direction.x * 370.0F, direction.y * 370.0F}, 13.0F, 1.20F,
                                        damage, 0.0F));
        break;
    case nen::Type::Manipulator:
        effects->push_back(
            CreateEffect(attackType, origin, {direction.x * 330.0F, direction.y * 330.0F}, 11.0F,
                         1.35F, damage, static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD));
        break;
    case nen::Type::Specialist: {
        constexpr std::array<float, 3> kAngles = {-0.28F, 0.0F, 0.28F};
        for (const float angle : kAngles) {
            const Vector2 rotated = Rotate(direction, angle);
            effects->push_back(CreateEffect(
                attackType, origin, {rotated.x * 500.0F, rotated.y * 500.0F}, 8.5F, 1.05F,
                std::max(1, static_cast<int>(std::round(damage * 0.45F))), angle));
        }
        break;
    }
    }
}

int UpdateAttackEffects(std::vector<AttackEffect> *effects, float dt, Vector2 targetCenter,
                        float targetRadius) {
    if (effects == nullptr) {
        return 0;
    }

    int totalDamage = 0;

    for (auto &effect : *effects) {
        effect.lifetime += dt;
        effect.phase += dt * 8.0F;

        switch (effect.type) {
        case nen::Type::Enhancer: {
            effect.radius += 420.0F * dt;
            effect.position = effect.origin;
            break;
        }
        case nen::Type::Transmuter: {
            effect.position.x += effect.velocity.x * dt;
            effect.position.y += effect.velocity.y * dt;
            const Vector2 direction = NormalizeSafe(effect.velocity);
            const Vector2 tangent{-direction.y, direction.x};
            const float sway = std::sin(effect.phase * 6.0F) * 95.0F * dt;
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
            effect.velocity.x *= 0.993F;
            effect.velocity.y *= 0.993F;
            break;
        case nen::Type::Manipulator:
            effect.velocity = Rotate(effect.velocity, 0.8F * dt);
            effect.position.x += effect.velocity.x * dt;
            effect.position.y += effect.velocity.y * dt;
            break;
        case nen::Type::Specialist:
            effect.velocity = Rotate(effect.velocity, std::sin(effect.phase) * 0.03F);
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

        if (distance <= effect.radius + targetRadius) {
            effect.hasHit = true;
            totalDamage += effect.damage;

            if (effect.type == nen::Type::Enhancer) {
                effect.maxLifetime = std::min(effect.maxLifetime, effect.lifetime + 0.08F);
            } else {
                effect.maxLifetime = effect.lifetime;
            }
        }
    }

    effects->erase(std::remove_if(effects->begin(), effects->end(),
                                  [](const AttackEffect &effect) {
                                      return effect.lifetime >= effect.maxLifetime;
                                  }),
                   effects->end());

    return totalDamage;
}

void DrawAttackEffects(const std::vector<AttackEffect> &effects) {
    for (const auto &effect : effects) {
        const float lifeRatio = 1.0F - (effect.lifetime / effect.maxLifetime);
        const Color baseColor = AttackColor(effect.type);
        const Color soft = Fade(baseColor, std::clamp(lifeRatio, 0.15F, 1.0F));

        switch (effect.type) {
        case nen::Type::Enhancer:
            DrawCircleLinesV(effect.position, effect.radius, soft);
            DrawCircleV(effect.position, effect.radius * 0.35F, Fade(baseColor, 0.24F));
            break;
        case nen::Type::Transmuter: {
            const Vector2 direction = NormalizeSafe(effect.velocity);
            const Vector2 start{effect.position.x - direction.x * 24.0F,
                                effect.position.y - direction.y * 24.0F};
            const Vector2 end{effect.position.x + direction.x * 10.0F,
                              effect.position.y + direction.y * 10.0F};
            DrawLineEx(start, end, 4.0F, soft);
            DrawCircleV(effect.position, effect.radius, Fade(baseColor, 0.55F));
            break;
        }
        case nen::Type::Emitter:
            DrawCircleV(effect.position, effect.radius * 1.45F, Fade(baseColor, 0.18F));
            DrawCircleV(effect.position, effect.radius, soft);
            DrawCircleLinesV(effect.position, effect.radius * 1.9F, Fade(baseColor, 0.35F));
            break;
        case nen::Type::Conjurer:
            DrawPoly(effect.position, 4, effect.radius * 1.6F, effect.phase * 320.0F, soft);
            DrawPolyLinesEx(effect.position, 4, effect.radius * 1.6F, effect.phase * 320.0F, 2.0F,
                            WHITE);
            break;
        case nen::Type::Manipulator: {
            DrawCircleV(effect.position, effect.radius, soft);
            const float orbit = effect.radius * 1.7F;
            DrawCircle(effect.position.x + std::cos(effect.phase * 7.0F) * orbit,
                       effect.position.y + std::sin(effect.phase * 7.0F) * orbit, 3.0F,
                       Fade(baseColor, 0.9F));
            DrawCircle(effect.position.x + std::cos(effect.phase * 7.0F + PI) * orbit,
                       effect.position.y + std::sin(effect.phase * 7.0F + PI) * orbit, 3.0F,
                       Fade(baseColor, 0.8F));
            break;
        }
        case nen::Type::Specialist:
            DrawPoly(effect.position, 6, effect.radius * 1.8F, effect.phase * 260.0F, soft);
            DrawCircleV(effect.position, effect.radius * 0.72F, Fade(baseColor, 0.85F));
            break;
        }
    }
}

} // namespace game
