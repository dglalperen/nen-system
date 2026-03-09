#pragma once

#include "nen/types.hpp"

namespace nen {

// Simplified efficiency model for gameplay prototyping:
// same type: 100, adjacent on ring: 80, two steps away: 60, opposite: 40.
int EfficiencyPercent(Type naturalType, Type targetType);

}  // namespace nen
