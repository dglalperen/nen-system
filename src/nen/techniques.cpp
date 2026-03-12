#include "nen/technique_state.hpp"

namespace nen {

bool CanEnterMode(const TechniqueState &state, AuraMode requested) {
    if (requested == AuraMode::Ten) {
        return true;  // always ok to relax to Ten
    }
    if (requested == AuraMode::Ren) {
        // Cannot spike Ren directly from Zetsu — must release Zetsu first
        return state.auraMode != AuraMode::Zetsu;
    }
    // Zetsu can always be entered
    return true;
}

void ApplyModeTransition(TechniqueState &state, AuraMode requested) {
    if (!CanEnterMode(state, requested)) {
        return;
    }
    state.auraMode = requested;
    // Zetsu collapses all active overlays
    if (requested == AuraMode::Zetsu) {
        state.gyoActive        = false;
        state.enActive         = false;
        state.koPrepared       = false;
        state.koCharge         = 0.0F;
        state.defenseCollapsed = false;
    }
}

} // namespace nen
