#pragma once

#include <vector>

#include <raylib.h>

#include "nen/types.hpp"

namespace game {

struct AttackEffect {
    nen::Type type;
    bool hatsu;
    Vector2 origin;
    Vector2 target;
    Vector2 position;
    Vector2 velocity;
    float homingStrength;
    float radius;
    float lifetime;
    float maxLifetime;
    float phase;
    int damage;
    bool hasHit;
    float manipulationSeconds;
    float vulnerabilitySeconds;
    float elasticSeconds;
    float elasticStrength;
};

struct AttackOutcome {
    int damage = 0;
    float manipulationSeconds = 0.0F;
    float vulnerabilitySeconds = 0.0F;
    float elasticSeconds = 0.0F;
    float elasticStrength = 0.0F;
};

void SpawnBaseAttack(std::vector<AttackEffect> *effects, nen::Type attackType, Vector2 origin,
                     Vector2 target, int damage);

void SpawnHatsuAttack(std::vector<AttackEffect> *effects, nen::Type attackType, Vector2 origin,
                      Vector2 target, int damage);

AttackOutcome UpdateAttackEffects(std::vector<AttackEffect> *effects, float dt,
                                  Vector2 targetCenter, float targetRadius);

void DrawAttackEffects(const std::vector<AttackEffect> &effects);

} // namespace game
