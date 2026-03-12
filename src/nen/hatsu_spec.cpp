#include "nen/hatsu_spec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace nen {

namespace {

constexpr uint32_t kFnvPrime  = 16777619u;
constexpr uint32_t kFnvOffset = 2166136261u;

uint32_t HashString(std::string_view s) {
    uint32_t h = kFnvOffset;
    for (char c : s) {
        h ^= static_cast<uint8_t>(c);
        h *= kFnvPrime;
    }
    return h;
}

uint32_t HashMix(uint32_t h, uint32_t salt) {
    h ^= salt;
    h *= kFnvPrime;
    return h;
}

// 12 canonical vows
struct VowTemplate {
    std::string_view description;
    float            multiplier;
    bool             requiresContact;
    bool             forbidsConsecutive;
    float            maxRangeOverride;
    bool             penaltyOnViolation;
};

constexpr std::array<VowTemplate, 12> kVowPool = {{
    {"Cannot be used consecutively",           1.15F, false, true,  -1.0F, false},
    {"Requires physical contact to activate",  1.40F, true,  false, -1.0F, false},
    {"Only usable within 3m of target",        1.30F, false, false,  3.0F, false},
    {"Cannot be used against fleeing targets", 1.20F, false, false, -1.0F, true},
    {"Doubles aura cost in open terrain",      1.20F, false, false, -1.0F, false},
    {"Only one active target at a time",       1.25F, false, false, -1.0F, false},
    {"User cannot speak while active",         1.35F, false, false, -1.0F, false},
    {"Cannot be canceled once started",        1.20F, false, false, -1.0F, false},
    {"Activates only after being hit first",   1.50F, false, false, -1.0F, false},
    {"Usable only three times per encounter",  1.45F, false, false, -1.0F, true},
    {"Visible glow reveals user location",     1.30F, false, false, -1.0F, false},
    {"Costs double when not at full health",   1.25F, false, false, -1.0F, false},
}};

// Category weight tables by type (indices into HatsuCategory)
using CatList = std::array<HatsuCategory, 3>;
constexpr std::array<CatList, 6> kTypeCategoryWeights = {{
    // Enhancer
    {HatsuCategory::Projectile, HatsuCategory::Passive,  HatsuCategory::Control},
    // Transmuter
    {HatsuCategory::Counter,    HatsuCategory::Control,  HatsuCategory::Projectile},
    // Emitter
    {HatsuCategory::Projectile, HatsuCategory::Zone,     HatsuCategory::Control},
    // Conjurer
    {HatsuCategory::Summon,     HatsuCategory::Zone,     HatsuCategory::Projectile},
    // Manipulator
    {HatsuCategory::Control,    HatsuCategory::Zone,     HatsuCategory::Summon},
    // Specialist (uniform — map to three broad ones)
    {HatsuCategory::Projectile, HatsuCategory::Counter,  HatsuCategory::Zone},
}};

} // namespace

float ComputeVowMultiplier(const HatsuSpec &spec) {
    float mult = 1.0F;
    for (const auto &vow : spec.vows) {
        mult *= vow.powerMultiplier;
    }
    return mult;
}

HatsuSpec BuildProceduralHatsuSpec(std::string_view characterName, Type naturalType,
                                   int basePotency) {
    const uint32_t nameHash = HashString(characterName);

    // Determine category from type-weighted table
    const int typeIndex = static_cast<int>(naturalType) - 1;  // 0..5
    const auto &cats = kTypeCategoryWeights[static_cast<std::size_t>(typeIndex)];
    const HatsuCategory category = cats[HashMix(nameHash, 0x1A2B3Cu) % cats.size()];

    // Activation type
    constexpr std::array<ActivationType, 5> kActivations = {
        ActivationType::Instant, ActivationType::Charged, ActivationType::Channeled,
        ActivationType::Triggered, ActivationType::Passive,
    };
    const ActivationType activation =
        kActivations[HashMix(nameHash, 0x4D5E6Fu) % kActivations.size()];

    // Targeting type
    constexpr std::array<TargetingType, 5> kTargetings = {
        TargetingType::Self, TargetingType::Target, TargetingType::Area,
        TargetingType::Direction, TargetingType::BoundTarget,
    };
    const TargetingType targeting =
        kTargetings[HashMix(nameHash, 0x7A8B9Cu) % kTargetings.size()];

    // Base cost model
    const float upfront =
        10.0F + static_cast<float>(HashMix(nameHash, 0xABCDEFu) % 40u);
    const float upkeep =
        static_cast<float>(HashMix(nameHash, 0x123456u) % 8u);
    const AuraCostModel cost{.upfrontCost = upfront, .upkeepPerSecond = upkeep};

    // Range and count
    const float range = 5.0F + static_cast<float>(HashMix(nameHash, 0xFEDCBAu) % 20u);
    const int count =
        category == HatsuCategory::Projectile || category == HatsuCategory::Summon
            ? 1 + static_cast<int>(HashMix(nameHash, 0xCAFEu) % 3u)
            : 1;

    // Duration
    const float duration = 1.0F + static_cast<float>(HashMix(nameHash, 0xBEEFu) % 5u);

    // Flags
    const bool homing  = (HashMix(nameHash, 0x11u) % 4u) == 0u;
    const bool binding = category == HatsuCategory::Control
                             ? (HashMix(nameHash, 0x22u) % 2u) == 0u
                             : false;
    const bool lingering = duration > 3.0F;
    const bool piercing  = category == HatsuCategory::Projectile
                               ? (HashMix(nameHash, 0x33u) % 3u) == 0u
                               : false;

    // Select 0–2 vows deterministically
    std::vector<VowConstraint> vows;
    const int vowCount = static_cast<int>(HashMix(nameHash, 0xD00Du) % 3u);  // 0, 1, or 2
    std::array<int, 12> indices{};
    for (int i = 0; i < 12; ++i) {
        indices[static_cast<std::size_t>(i)] = i;
    }
    const uint32_t vowSeed = HashMix(nameHash, 0xFACEu);
    for (int v = 0; v < vowCount; ++v) {
        const int remaining = 12 - v;
        const int pick = static_cast<int>(HashMix(vowSeed, static_cast<uint32_t>(v)) %
                                          static_cast<uint32_t>(remaining));
        const int vowIdx = indices[static_cast<std::size_t>(pick)];
        // Swap to avoid duplicates
        indices[static_cast<std::size_t>(pick)] =
            indices[static_cast<std::size_t>(remaining - 1)];
        const auto &tmpl = kVowPool[static_cast<std::size_t>(vowIdx)];
        vows.push_back(VowConstraint{
            .description        = tmpl.description,
            .powerMultiplier    = tmpl.multiplier,
            .requiresContact    = tmpl.requiresContact,
            .forbidsConsecutive = tmpl.forbidsConsecutive,
            .maxRangeOverride   = tmpl.maxRangeOverride,
            .penaltyOnViolation = tmpl.penaltyOnViolation,
        });
    }

    HatsuSpec spec;
    spec.name          = "";  // caller can use hatsu.hpp GenerateHatsuName
    spec.primaryType   = naturalType;
    spec.category      = category;
    spec.activationType = activation;
    spec.targetingType = targeting;
    spec.cost          = cost;
    spec.range         = range;
    spec.count         = count;
    spec.duration      = duration;
    spec.homing        = homing;
    spec.binding       = binding;
    spec.lingering     = lingering;
    spec.piercing      = piercing;
    spec.vows          = std::move(vows);

    // potency = base × vow multipliers (affinity is 100% for natural type)
    spec.potencyBudget = static_cast<float>(basePotency) * ComputeVowMultiplier(spec);

    return spec;
}

HatsuSpec BuildUserHatsuSpec(std::string_view hatsuName, Type naturalType, int basePotency,
                              HatsuCategory category,
                              const std::vector<int> &selectedVowIndices) {
    const uint32_t nameHash = HashString(hatsuName);

    // Activation and targeting derived from name hash (reproducible)
    constexpr std::array<ActivationType, 5> kActivations = {
        ActivationType::Instant, ActivationType::Charged, ActivationType::Channeled,
        ActivationType::Triggered, ActivationType::Passive,
    };
    const ActivationType activation =
        kActivations[HashMix(nameHash, 0x4D5E6Fu) % kActivations.size()];

    constexpr std::array<TargetingType, 5> kTargetings = {
        TargetingType::Self, TargetingType::Target, TargetingType::Area,
        TargetingType::Direction, TargetingType::BoundTarget,
    };
    const TargetingType targeting =
        kTargetings[HashMix(nameHash, 0x7A8B9Cu) % kTargetings.size()];

    const float upfront  = 10.0F + static_cast<float>(HashMix(nameHash, 0xABCDEFu) % 40u);
    const float upkeep   = static_cast<float>(HashMix(nameHash, 0x123456u) % 8u);
    const float range    = 5.0F + static_cast<float>(HashMix(nameHash, 0xFEDCBAu) % 20u);
    const int   count    =
        category == HatsuCategory::Projectile || category == HatsuCategory::Summon
            ? 1 + static_cast<int>(HashMix(nameHash, 0xCAFEu) % 3u)
            : 1;
    const float duration = 1.0F + static_cast<float>(HashMix(nameHash, 0xBEEFu) % 5u);
    const bool  homing   = (HashMix(nameHash, 0x11u) % 4u) == 0u;
    const bool  binding  = category == HatsuCategory::Control
                               ? (HashMix(nameHash, 0x22u) % 2u) == 0u : false;
    const bool lingering = duration > 3.0F;
    const bool piercing  = category == HatsuCategory::Projectile
                               ? (HashMix(nameHash, 0x33u) % 3u) == 0u : false;

    // Build vows from user-selected indices
    std::vector<VowConstraint> vows;
    for (int idx : selectedVowIndices) {
        if (idx < 0 || idx >= static_cast<int>(kVowPool.size())) { continue; }
        const auto &tmpl = kVowPool[static_cast<std::size_t>(idx)];
        vows.push_back(VowConstraint{
            .description        = tmpl.description,
            .powerMultiplier    = tmpl.multiplier,
            .requiresContact    = tmpl.requiresContact,
            .forbidsConsecutive = tmpl.forbidsConsecutive,
            .maxRangeOverride   = tmpl.maxRangeOverride,
            .penaltyOnViolation = tmpl.penaltyOnViolation,
        });
    }

    HatsuSpec spec;
    spec.name           = std::string(hatsuName);
    spec.primaryType    = naturalType;
    spec.category       = category;
    spec.activationType = activation;
    spec.targetingType  = targeting;
    spec.cost           = AuraCostModel{.upfrontCost = upfront, .upkeepPerSecond = upkeep};
    spec.range          = range;
    spec.count          = count;
    spec.duration       = duration;
    spec.homing         = homing;
    spec.binding        = binding;
    spec.lingering      = lingering;
    spec.piercing       = piercing;
    spec.vows           = std::move(vows);
    spec.potencyBudget  = static_cast<float>(basePotency) * ComputeVowMultiplier(spec);
    return spec;
}

const char *CategoryLabel(HatsuCategory category) {
    switch (category) {
    case HatsuCategory::Projectile: return "Projectile";
    case HatsuCategory::Zone:       return "Zone";
    case HatsuCategory::Control:    return "Control";
    case HatsuCategory::Counter:    return "Counter";
    case HatsuCategory::Summon:     return "Summon";
    case HatsuCategory::Passive:    return "Passive";
    }
    return "?";
}

const char *CategoryDescription(HatsuCategory category) {
    switch (category) {
    case HatsuCategory::Projectile: return "Fire aura as directed attacks";
    case HatsuCategory::Zone:       return "Create persistent aura fields";
    case HatsuCategory::Control:    return "Bind or command targets";
    case HatsuCategory::Counter:    return "Respond to and deflect attacks";
    case HatsuCategory::Summon:     return "Materialize objects or creatures";
    case HatsuCategory::Passive:    return "Strengthen body and senses";
    }
    return "";
}

} // namespace nen
