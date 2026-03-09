#include "nen/combat.hpp"

#include <algorithm>
#include <cmath>

#include "nen/affinity.hpp"

namespace nen {

double DamageModifier(Type naturalType, Type attackType) {
    const int efficiency = EfficiencyPercent(naturalType, attackType);
    switch (efficiency) {
    case 100:
        return 1.8;
    case 80:
        return 1.3;
    case 60:
        return 1.0;
    case 40:
        return 0.65;
    default:
        return 0.5;
    }
}

int ComputeAttackDamage(Type naturalType, Type attackType, int basePower) {
    const double modifier = DamageModifier(naturalType, attackType);
    return std::max(1, static_cast<int>(std::round(static_cast<double>(basePower) * modifier)));
}

bool TryConsumeAura(Character *character, int auraCost) {
    if (character == nullptr || auraCost < 0 || character->auraPool < auraCost) {
        return false;
    }
    character->auraPool -= auraCost;
    return true;
}

} // namespace nen
