#pragma once

#include <array>
#include <string_view>

#include "nen/types.hpp"

namespace nen {

struct QuizAnswerOption {
    std::string_view text;
    Type primaryType;
};

struct QuizQuestion {
    std::string_view prompt;
    std::array<QuizAnswerOption, 3> options;
};

using QuizScores = std::array<int, 6>;

const std::array<QuizQuestion, 6> &PersonalityQuestions();
void ApplyQuizAnswer(QuizScores *scores, const QuizAnswerOption &option);
Type DetermineNenType(const QuizScores &scores);
std::string_view WaterDivinationEffect(Type type);

} // namespace nen
