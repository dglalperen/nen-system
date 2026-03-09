#pragma once

#include <array>
#include <string>
#include <string_view>

namespace nen {

enum class Type {
    Enhancer = 1,
    Transmuter,
    Emitter,
    Conjurer,
    Manipulator,
    Specialist
};

constexpr std::array<Type, 6> kAllTypes = {
    Type::Enhancer,
    Type::Transmuter,
    Type::Emitter,
    Type::Conjurer,
    Type::Manipulator,
    Type::Specialist
};

constexpr std::string_view ToString(Type type) {
    switch (type) {
        case Type::Enhancer:
            return "Enhancer";
        case Type::Transmuter:
            return "Transmuter";
        case Type::Emitter:
            return "Emitter";
        case Type::Conjurer:
            return "Conjurer";
        case Type::Manipulator:
            return "Manipulator";
        case Type::Specialist:
            return "Specialist";
    }
    return "Unknown";
}

struct Character {
    std::string name;
    Type naturalType;
    int auraPool;
};

}  // namespace nen
