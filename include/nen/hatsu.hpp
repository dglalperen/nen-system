#pragma once

#include <string>
#include <string_view>

#include "nen/types.hpp"

namespace nen {

std::string GenerateHatsuName(std::string_view characterName, Type naturalType);
int GenerateHatsuPotency(std::string_view characterName);
std::string_view HatsuAbilityName(Type naturalType);
std::string_view HatsuAbilityDescription(Type naturalType);

} // namespace nen
