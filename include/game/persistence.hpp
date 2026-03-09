#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "nen/types.hpp"

namespace game {

struct StoredCharacter {
    std::filesystem::path filePath;
    nen::Character character;
};

std::filesystem::path DefaultSaveDirectory();
bool SaveCharacter(const nen::Character &character, const std::filesystem::path &saveDir,
                   std::string *errorMessage);
std::vector<StoredCharacter> ListCharacters(const std::filesystem::path &saveDir,
                                            std::string *errorMessage);

} // namespace game
