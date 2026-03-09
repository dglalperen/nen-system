#include "nen/quiz.hpp"

#include <algorithm>

namespace nen {

const std::array<QuizQuestion, 6> &PersonalityQuestions() {
    static constexpr std::array<QuizQuestion, 6> kQuestions{{
        {"A difficult challenge appears. You...",
         {{
             {"Break through it with direct force.", Type::Enhancer},
             {"Adapt your approach and improvise.", Type::Transmuter},
             {"Keep distance and control tempo.", Type::Emitter},
         }}},
        {"Your friends describe you as...",
         {{
             {"Reliable and straightforward.", Type::Enhancer},
             {"Unpredictable and curious.", Type::Transmuter},
             {"Calm and strategic.", Type::Manipulator},
         }}},
        {"When learning a skill, you prefer...",
         {{
             {"Hands-on repetition.", Type::Enhancer},
             {"Experimenting with unusual methods.", Type::Conjurer},
             {"Mastering control and precision.", Type::Manipulator},
         }}},
        {"In a team fight you usually...",
         {{
             {"Lead from the front.", Type::Emitter},
             {"Set up tricky support tools.", Type::Conjurer},
             {"Watch for a special opening.", Type::Specialist},
         }}},
        {"Your biggest strength is...",
         {{
             {"Raw determination.", Type::Enhancer},
             {"Creativity.", Type::Conjurer},
             {"Long-term planning.", Type::Manipulator},
         }}},
        {"Pick a battle philosophy:",
         {{
             {"Power and momentum.", Type::Emitter},
             {"Control and conditions.", Type::Transmuter},
             {"Something only I can do.", Type::Specialist},
         }}},
    }};
    return kQuestions;
}

void ApplyQuizAnswer(QuizScores *scores, const QuizAnswerOption &option) {
    if (scores == nullptr) {
        return;
    }
    const int index = static_cast<int>(option.primaryType) - 1;
    (*scores)[index] += 3;
    (*scores)[(index + 1) % static_cast<int>(scores->size())] += 1;
    (*scores)[(index + static_cast<int>(scores->size()) - 1) % static_cast<int>(scores->size())] +=
        1;
}

Type DetermineNenType(const QuizScores &scores) {
    Type bestType = Type::Enhancer;
    int bestScore = scores[0];

    for (std::size_t i = 1; i < kAllTypes.size(); ++i) {
        if (scores[i] > bestScore) {
            bestScore = scores[i];
            bestType = kAllTypes[i];
        }
    }
    return bestType;
}

std::string_view WaterDivinationEffect(Type type) {
    switch (type) {
    case Type::Enhancer:
        return "Water overflows the glass. Pure amplification.";
    case Type::Transmuter:
        return "Water taste shifts. Aura changed its quality.";
    case Type::Emitter:
        return "Water color changes. Aura projects outward.";
    case Type::Conjurer:
        return "Impurities appear. Form is being materialized.";
    case Type::Manipulator:
        return "The leaf moves. Control-type aura dominates.";
    case Type::Specialist:
        return "An unusual reaction appears. Specialist potential detected.";
    }
    return "No reaction.";
}

} // namespace nen
