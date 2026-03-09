#include <cstdlib>
#include <iostream>

#include "nen/affinity.hpp"
#include "nen/combat.hpp"
#include "nen/quiz.hpp"
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

    std::cout << "All tests passed.\n";
    return 0;
}
