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

} // namespace nen
