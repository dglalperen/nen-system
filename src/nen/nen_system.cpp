#include "nen/nen_system.hpp"

#include <algorithm>

#include "nen/aura_pool.hpp"
#include "nen/technique_state.hpp"

namespace nen {

DerivedNenStats ComputeNenStats(const Character &character) {
    const TechniqueState &t = character.techniques;
    DerivedNenStats stats{};

    switch (t.auraMode) {
    case AuraMode::Ten:
        stats.regenMultiplier   = 1.2F;
        stats.damageMultiplier  = 1.0F;
        stats.defenseMultiplier = 1.15F;
        stats.auraVisibility    = 0.9F;
        stats.canAttack         = true;
        stats.canUseHatsu       = true;
        break;
    case AuraMode::Zetsu:
        stats.regenMultiplier   = 0.0F;
        stats.damageMultiplier  = 0.0F;
        stats.defenseMultiplier = 0.6F;
        stats.auraVisibility    = 0.0F;
        stats.canAttack         = false;
        stats.canUseHatsu       = false;
        break;
    case AuraMode::Ren:
        stats.regenMultiplier   = 0.6F;
        stats.damageMultiplier  = 1.65F;
        stats.defenseMultiplier = 1.25F;
        stats.auraVisibility    = 1.5F;
        stats.canAttack         = true;
        stats.canUseHatsu       = true;
        break;
    }

    if (t.gyoActive) {
        stats.defenseMultiplier -= 0.05F;
        stats.revealsHidden = true;
    }
    if (t.enActive) {
        stats.defenseMultiplier -= 0.08F;
        stats.detectionRadius = 15.0F;  // world units
    }
    if (t.koPrepared) {
        stats.regenMultiplier   = 0.0F;
        stats.damageMultiplier *= std::max(1.0F, t.koCharge * 3.0F);
        stats.defenseMultiplier = 0.3F;
        stats.auraVisibility    = 2.0F;
    }

    return stats;
}

std::vector<NenEvent> UpdateNenState(Character &character, const NenInputIntent &intent, float dt) {
    std::vector<NenEvent> events;
    TechniqueState       &t    = character.techniques;
    const AuraPool       &pool = character.auraPool;

    // Mode transitions — priority: Zetsu > Ren > Ten (default)
    const AuraMode previousMode = t.auraMode;
    if (intent.holdZetsu) {
        ApplyModeTransition(t, AuraMode::Zetsu);
    } else if (intent.holdRen) {
        if (CanEnterMode(t, AuraMode::Ren)) {
            ApplyModeTransition(t, AuraMode::Ren);
        }
    } else {
        ApplyModeTransition(t, AuraMode::Ten);
    }

    if (t.auraMode != previousMode) {
        switch (t.auraMode) {
        case AuraMode::Ten:
            events.push_back(NenEvent::EnteredTen);
            break;
        case AuraMode::Ren:
            events.push_back(NenEvent::EnteredRen);
            break;
        case AuraMode::Zetsu:
            events.push_back(NenEvent::EnteredZetsu);
            break;
        }
    }

    // Gyo toggle (blocked in Zetsu)
    if (intent.toggleGyo && t.auraMode != AuraMode::Zetsu) {
        t.gyoActive = !t.gyoActive;
        events.push_back(NenEvent::GyoToggled);
    }

    // En hold (blocked in Zetsu)
    const bool prevEn = t.enActive;
    t.enActive = intent.holdEn && t.auraMode != AuraMode::Zetsu;
    if (t.enActive != prevEn) {
        events.push_back(NenEvent::EnToggled);
    }

    // Ko arm (blocked in Zetsu)
    if (intent.armKo && t.auraMode != AuraMode::Zetsu) {
        if (!t.koPrepared) {
            t.koPrepared       = true;
            t.koCharge         = 0.0F;
            t.defenseCollapsed = true;
            events.push_back(NenEvent::KoArmed);
        }
        t.koCharge += dt;
    } else if (t.koPrepared) {
        t.koPrepared       = false;
        t.defenseCollapsed = false;
        t.koCharge         = 0.0F;
        events.push_back(NenEvent::KoReleased);
    }

    if (pool.current < pool.max * 0.1F) {
        events.push_back(NenEvent::AuraDepletedWarning);
    }

    return events;
}

} // namespace nen
