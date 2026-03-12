#pragma once

#include <vector>

#include "nen/nen_event.hpp"
#include "nen/types.hpp"

namespace nen {

struct DerivedNenStats {
    float regenMultiplier  = 1.2F;   // applied to AuraPool.regenRate
    float damageMultiplier = 1.0F;   // applied to attack output
    float defenseMultiplier = 1.15F; // applied to incoming damage
    float auraVisibility   = 0.9F;   // 0=hidden(zetsu), 1=full
    float detectionRadius  = 0.0F;   // 0=none, >0 En is active (world units)
    bool  canAttack        = true;
    bool  canUseHatsu      = true;
    bool  revealsHidden    = false;  // Gyo: see In-hidden effects
};

DerivedNenStats ComputeNenStats(const Character &character);

struct NenInputIntent {
    bool holdZetsu = false;
    bool holdRen   = false;
    bool toggleGyo = false;
    bool holdEn    = false;
    bool armKo     = false;
};

// Returns list of events that fired this tick
std::vector<NenEvent> UpdateNenState(Character &character, const NenInputIntent &intent, float dt);

} // namespace nen
