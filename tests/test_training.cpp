#include <cstdlib>
#include <iostream>

#include "nen/affinity.hpp"
#include "nen/training.hpp"
#include "nen/types.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "Test failed: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

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
    Expect(plan.focusOrder[0].type == nen::Type::Manipulator, "first plan slot must be natural type");
    Expect(plan.focusOrder[0].efficiencyPercent == 100, "first plan slot must be 100%");
    Expect(plan.focusOrder[1].efficiencyPercent == 80, "second plan slot must be 80%");
    Expect(plan.focusOrder[2].efficiencyPercent == 80, "third plan slot must be 80%");

    std::cout << "All tests passed.\n";
    return 0;
}
