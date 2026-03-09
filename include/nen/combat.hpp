#pragma once

#include "nen/types.hpp"

namespace nen {

double DamageModifier(Type naturalType, Type attackType);
int ComputeAttackDamage(Type naturalType, Type attackType, int basePower);
bool TryConsumeAura(Character *character, int auraCost);

} // namespace nen
