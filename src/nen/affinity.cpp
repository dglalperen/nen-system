#include "nen/affinity.hpp"

#include <algorithm>

namespace nen {

namespace {

int ToRingIndex(Type type) {
    return static_cast<int>(type) - 1;
}

}  // namespace

int EfficiencyPercent(Type naturalType, Type targetType) {
    const int ringSize = 6;
    const int from = ToRingIndex(naturalType);
    const int to = ToRingIndex(targetType);

    const int clockwise = (to - from + ringSize) % ringSize;
    const int counterClockwise = (from - to + ringSize) % ringSize;
    const int steps = std::min(clockwise, counterClockwise);

    switch (steps) {
        case 0:
            return 100;
        case 1:
            return 80;
        case 2:
            return 60;
        case 3:
            return 40;
        default:
            return 0;
    }
}

}  // namespace nen
