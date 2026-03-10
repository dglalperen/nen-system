#include "nen/hatsu.hpp"

#include <array>
#include <functional>

namespace nen {

namespace {

std::size_t StableHash(std::string_view value) { return std::hash<std::string_view>{}(value); }

} // namespace

std::string GenerateHatsuName(std::string_view characterName, Type naturalType) {
    static constexpr std::array<std::string_view, 8> kPrefixes = {
        "Crimson", "Silent", "Storm", "Moon", "Arc", "Echo", "Iron", "Phantom"};
    static constexpr std::array<std::string_view, 8> kSuffixes = {
        "Pulse", "Thread", "Lance", "Crown", "Prism", "Gate", "Brand", "Orbit"};
    static constexpr std::array<std::string_view, 6> kCoreWords = {"Drive", "Shift", "Burst",
                                                                   "Bind",  "Bloom", "Nova"};

    const std::size_t seed =
        StableHash(characterName) ^ (static_cast<std::size_t>(static_cast<int>(naturalType)) << 9U);
    const auto prefix = kPrefixes[seed % kPrefixes.size()];
    const auto core = kCoreWords[(seed / 7U) % kCoreWords.size()];
    const auto suffix = kSuffixes[(seed / 13U) % kSuffixes.size()];

    return std::string(prefix) + " " + std::string(core) + " " + std::string(suffix);
}

int GenerateHatsuPotency(std::string_view characterName) {
    const std::size_t seed = StableHash(characterName);
    return 90 + static_cast<int>(seed % 41U); // 90..130
}

std::string_view HatsuAbilityName(Type naturalType) {
    switch (naturalType) {
    case Type::Enhancer:
        return "Impact Dominion";
    case Type::Transmuter:
        return "Phase Spark";
    case Type::Emitter:
        return "Skyline Burst";
    case Type::Conjurer:
        return "Arsenal Weave";
    case Type::Manipulator:
        return "Command Thread";
    case Type::Specialist:
        return "Mirrored Fate";
    }
    return "Unknown Ability";
}

std::string_view HatsuAbilityDescription(Type naturalType) {
    switch (naturalType) {
    case Type::Enhancer:
        return "Detonates a close-range aura quake with heavy burst damage.";
    case Type::Transmuter:
        return "Launches volatile lightning arcs with erratic movement.";
    case Type::Emitter:
        return "Fires a fast long-range beamlike aura projectile.";
    case Type::Conjurer:
        return "Summons multiple shaped weapons that converge on target.";
    case Type::Manipulator:
        return "Binds target movement and increases follow-up damage taken.";
    case Type::Specialist:
        return "Splits into unstable shards that strike from strange angles.";
    }
    return "No description.";
}

} // namespace nen
