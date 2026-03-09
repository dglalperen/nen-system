#pragma once

#include <array>
#include <string_view>

#include "nen/types.hpp"

namespace nen {

struct TrainingFocus {
    Type type;
    int efficiencyPercent;
};

struct TrainingPlan {
    std::array<TrainingFocus, 3> focusOrder;
};

TrainingPlan BuildStarterPlan(Type naturalType);

}  // namespace nen
