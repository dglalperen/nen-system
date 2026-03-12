#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "nen/types.hpp"

namespace nen {

enum class HatsuCategory   { Projectile, Zone, Control, Counter, Summon, Passive };
enum class ActivationType  { Instant, Charged, Channeled, Triggered, Passive };
enum class TargetingType   { Self, Target, Area, Direction, BoundTarget };

struct AuraCostModel {
    float upfrontCost     = 0.0F;
    float upkeepPerSecond = 0.0F;
    float reservedAura    = 0.0F;  // held off-limits while active
};

struct VowConstraint {
    std::string_view description;
    float            powerMultiplier    = 1.0F;
    bool             requiresContact    = false;
    bool             forbidsConsecutive = false;
    float            maxRangeOverride   = -1.0F;  // -1 = no override
    bool             penaltyOnViolation = false;  // burns 20% aura if broken
};

struct HatsuSpec {
    std::string    name;
    Type           primaryType;
    HatsuCategory  category;
    ActivationType activationType;
    TargetingType  targetingType;
    AuraCostModel  cost;
    float  range         = 0.0F;
    int    count         = 1;
    float  duration      = 0.0F;
    bool   homing        = false;
    bool   binding       = false;
    bool   lingering     = false;
    bool   piercing      = false;
    std::vector<VowConstraint> vows;
    float  potencyBudget = 100.0F;  // base × affinity% × vow multipliers
};

// Deterministic procedural generation (used as fallback / for load path)
HatsuSpec BuildProceduralHatsuSpec(std::string_view characterName, Type naturalType,
                                   int basePotency);

// User-authored hatsu: category and vows chosen by the player; other properties hashed from name
HatsuSpec BuildUserHatsuSpec(std::string_view hatsuName, Type naturalType, int basePotency,
                              HatsuCategory category,
                              const std::vector<int> &selectedVowIndices);

float ComputeVowMultiplier(const HatsuSpec &spec);

// Human-readable labels
const char *CategoryLabel(HatsuCategory category);
const char *CategoryDescription(HatsuCategory category);

} // namespace nen
