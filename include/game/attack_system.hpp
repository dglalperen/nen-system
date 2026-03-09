#pragma once

#include <string>
#include <vector>

#include <raylib.h>

#include "nen/types.hpp"

namespace game {

struct AttackEffect {
    nen::Type type;
    Vector2 origin;
    Vector2 position;
    Vector2 velocity;
    float radius;
    float lifetime;
    float maxLifetime;
    float phase;
    int damage;
    bool hasHit;
};

void SpawnAttackEffect(std::vector<AttackEffect> *effects, nen::Type attackType, Vector2 origin,
                       Vector2 target, int damage);

int UpdateAttackEffects(std::vector<AttackEffect> *effects, float dt, Vector2 targetCenter,
                        float targetRadius);

void DrawAttackEffects(const std::vector<AttackEffect> &effects);

} // namespace game
