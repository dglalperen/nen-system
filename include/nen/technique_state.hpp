#pragma once

namespace nen {

enum class AuraMode { Ten, Zetsu, Ren };

struct TechniqueState {
    AuraMode auraMode         = AuraMode::Ten;
    bool     gyoActive        = false;  // perception focus
    bool     enActive         = false;  // expanded radar
    bool     koPrepared       = false;  // concentration armed
    float    koCharge         = 0.0F;   // seconds spent concentrating
    bool     defenseCollapsed = false;  // true while ko is active
};

// Returns false if transition is illegal (e.g. Ren while Zetsu)
bool CanEnterMode(const TechniqueState &state, AuraMode requested);
void ApplyModeTransition(TechniqueState &state, AuraMode requested);

} // namespace nen
