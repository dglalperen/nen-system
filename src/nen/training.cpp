#include "nen/training.hpp"

#include "nen/affinity.hpp"

namespace nen {

namespace {

Type NextType(Type type) {
    switch (type) {
        case Type::Enhancer:
            return Type::Transmuter;
        case Type::Transmuter:
            return Type::Emitter;
        case Type::Emitter:
            return Type::Conjurer;
        case Type::Conjurer:
            return Type::Manipulator;
        case Type::Manipulator:
            return Type::Specialist;
        case Type::Specialist:
            return Type::Enhancer;
    }
    return Type::Enhancer;
}

Type PreviousType(Type type) {
    switch (type) {
        case Type::Enhancer:
            return Type::Specialist;
        case Type::Transmuter:
            return Type::Enhancer;
        case Type::Emitter:
            return Type::Transmuter;
        case Type::Conjurer:
            return Type::Emitter;
        case Type::Manipulator:
            return Type::Conjurer;
        case Type::Specialist:
            return Type::Manipulator;
    }
    return Type::Enhancer;
}

}  // namespace

TrainingPlan BuildStarterPlan(Type naturalType) {
    return TrainingPlan{
        std::array<TrainingFocus, 3>{
            TrainingFocus{naturalType, EfficiencyPercent(naturalType, naturalType)},
            TrainingFocus{NextType(naturalType), EfficiencyPercent(naturalType, NextType(naturalType))},
            TrainingFocus{PreviousType(naturalType),
                          EfficiencyPercent(naturalType, PreviousType(naturalType))},
        }};
}

}  // namespace nen
