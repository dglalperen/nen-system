#include <cstdlib>
#include <iostream>

#include "nen/affinity.hpp"
#include "nen/combat.hpp"
#include "nen/hatsu.hpp"
#include "nen/hatsu_spec.hpp"
#include "nen/nen_system.hpp"
#include "nen/quiz.hpp"
#include "nen/technique_state.hpp"
#include "nen/training.hpp"
#include "nen/types.hpp"

namespace {

void Expect(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "Test failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    Expect(nen::EfficiencyPercent(nen::Type::Enhancer, nen::Type::Enhancer) == 100,
           "same type should be 100%");
    Expect(nen::EfficiencyPercent(nen::Type::Enhancer, nen::Type::Transmuter) == 80,
           "adjacent type should be 80%");
    Expect(nen::EfficiencyPercent(nen::Type::Enhancer, nen::Type::Emitter) == 60,
           "two steps away should be 60%");
    Expect(nen::EfficiencyPercent(nen::Type::Enhancer, nen::Type::Conjurer) == 40,
           "opposite type should be 40%");

    const nen::TrainingPlan plan = nen::BuildStarterPlan(nen::Type::Manipulator);
    Expect(plan.focusOrder[0].type == nen::Type::Manipulator,
           "first plan slot must be natural type");
    Expect(plan.focusOrder[0].efficiencyPercent == 100, "first plan slot must be 100%");
    Expect(plan.focusOrder[1].efficiencyPercent == 80, "second plan slot must be 80%");
    Expect(plan.focusOrder[2].efficiencyPercent == 80, "third plan slot must be 80%");

    Expect(nen::DamageModifier(nen::Type::Enhancer, nen::Type::Enhancer) >
               nen::DamageModifier(nen::Type::Enhancer, nen::Type::Conjurer),
           "same-type damage modifier should be stronger than opposite-type");
    Expect(nen::ComputeAttackDamage(nen::Type::Enhancer, nen::Type::Enhancer, 20) >
               nen::ComputeAttackDamage(nen::Type::Enhancer, nen::Type::Conjurer, 20),
           "same-type damage should be greater than opposite-type damage");

    nen::QuizScores scores{};
    scores.fill(0);
    scores[static_cast<int>(nen::Type::Manipulator) - 1] = 12;
    scores[static_cast<int>(nen::Type::Emitter) - 1] = 11;
    Expect(nen::DetermineNenType(scores) == nen::Type::Manipulator,
           "quiz resolver should choose the highest score");
    Expect(!nen::WaterDivinationEffect(nen::Type::Specialist).empty(),
           "water divination reveal text should exist");

    const std::string hatsuNameA = nen::GenerateHatsuName("alperen", nen::Type::Manipulator);
    const std::string hatsuNameB = nen::GenerateHatsuName("alperen", nen::Type::Manipulator);
    Expect(hatsuNameA == hatsuNameB, "hatsu naming should be deterministic for same input");
    const int potency = nen::GenerateHatsuPotency("alperen");
    Expect(potency >= 90 && potency <= 130, "hatsu potency should remain in expected range");
    Expect(!nen::HatsuAbilityName(nen::Type::Manipulator).empty(),
           "hatsu ability name should be available");
    Expect(!nen::HatsuAbilityDescription(nen::Type::Manipulator).empty(),
           "hatsu ability description should be available");

    // Technique transition tests
    {
        nen::TechniqueState s{};
        Expect(nen::CanEnterMode(s, nen::AuraMode::Ren),   "Ten → Ren should be allowed");
        Expect(nen::CanEnterMode(s, nen::AuraMode::Zetsu), "Ten → Zetsu should be allowed");
        nen::ApplyModeTransition(s, nen::AuraMode::Zetsu);
        Expect(s.auraMode == nen::AuraMode::Zetsu, "mode should be Zetsu after transition");
        Expect(!nen::CanEnterMode(s, nen::AuraMode::Ren),  "Zetsu → Ren should be blocked");
        Expect(nen::CanEnterMode(s, nen::AuraMode::Ten),   "Zetsu → Ten should be allowed");
    }

    // ComputeNenStats output tests
    {
        nen::Character c{.name="Test", .naturalType=nen::Type::Enhancer};

        // Ten (default)
        const nen::DerivedNenStats tenStats = nen::ComputeNenStats(c);
        Expect(tenStats.canAttack,    "Ten: canAttack should be true");
        Expect(tenStats.canUseHatsu,  "Ten: canUseHatsu should be true");
        Expect(tenStats.auraVisibility > 0.0F, "Ten: auraVisibility > 0");

        // Zetsu
        c.techniques.auraMode = nen::AuraMode::Zetsu;
        const nen::DerivedNenStats zetsuStats = nen::ComputeNenStats(c);
        Expect(!zetsuStats.canAttack,   "Zetsu: canAttack should be false");
        Expect(!zetsuStats.canUseHatsu, "Zetsu: canUseHatsu should be false");
        Expect(zetsuStats.auraVisibility == 0.0F, "Zetsu: auraVisibility should be 0");

        // Ren
        c.techniques.auraMode = nen::AuraMode::Ren;
        const nen::DerivedNenStats renStats = nen::ComputeNenStats(c);
        Expect(renStats.damageMultiplier > tenStats.damageMultiplier,
               "Ren: damage multiplier should exceed Ten");

        // Ren+Gyo reduces defense vs plain Ren
        c.techniques.gyoActive = true;
        const nen::DerivedNenStats renGyoStats = nen::ComputeNenStats(c);
        Expect(renGyoStats.defenseMultiplier < renStats.defenseMultiplier,
               "Gyo: should reduce defense multiplier");
        Expect(renGyoStats.revealsHidden, "Gyo: revealsHidden should be true");
    }

    // UpdateNenState: Zetsu blocks Ren on same frame
    {
        nen::Character c{.name="Test", .naturalType=nen::Type::Emitter};
        nen::NenInputIntent intent{.holdZetsu=true, .holdRen=true};
        nen::UpdateNenState(c, intent, 0.016F);
        Expect(c.techniques.auraMode == nen::AuraMode::Zetsu,
               "Zetsu has priority over Ren on same frame");
    }

    // Procedural HatsuSpec determinism
    {
        const nen::HatsuSpec specA =
            nen::BuildProceduralHatsuSpec("alperen", nen::Type::Manipulator, 100);
        const nen::HatsuSpec specB =
            nen::BuildProceduralHatsuSpec("alperen", nen::Type::Manipulator, 100);
        Expect(specA.category      == specB.category,      "spec category is deterministic");
        Expect(specA.vows.size()   == specB.vows.size(),   "spec vow count is deterministic");
        Expect(specA.potencyBudget == specB.potencyBudget, "spec potency is deterministic");

        // Different names → may differ
        const nen::HatsuSpec specC =
            nen::BuildProceduralHatsuSpec("gon", nen::Type::Manipulator, 100);
        // Can't assert inequality always, but verify it runs without crash
        Expect(specC.primaryType == nen::Type::Manipulator, "spec primary type is preserved");
    }

    // Vow multiplier > 1 when vows are present
    {
        const nen::HatsuSpec spec =
            nen::BuildProceduralHatsuSpec("killua", nen::Type::Transmuter, 100);
        const float vowMult = nen::ComputeVowMultiplier(spec);
        Expect(vowMult >= 1.0F, "vow multiplier should be >= 1.0");
        if (!spec.vows.empty()) {
            Expect(vowMult > 1.0F, "non-empty vows should push multiplier above 1.0");
        }
    }

    std::cout << "All tests passed.\n";
    return 0;
}
