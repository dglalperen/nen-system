#pragma once

#include <string>
#include <string_view>

#include "nen/types.hpp"

namespace nen {

std::string GenerateHatsuName(std::string_view characterName, Type naturalType);
int GenerateHatsuPotency(std::string_view characterName);

} // namespace nen
