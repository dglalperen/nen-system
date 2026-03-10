#include "game/persistence.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

#include "nen/hatsu.hpp"

namespace game {

namespace {

std::string Slugify(const std::string &source) {
    std::string out;
    out.reserve(source.size());
    bool previousWasUnderscore = false;

    for (const unsigned char c : source) {
        if (std::isalnum(c) != 0) {
            out.push_back(static_cast<char>(std::tolower(c)));
            previousWasUnderscore = false;
            continue;
        }
        if (!previousWasUnderscore) {
            out.push_back('_');
            previousWasUnderscore = true;
        }
    }

    if (out.empty()) {
        return "hunter";
    }
    return out;
}

bool ParseCharacter(std::ifstream *file, nen::Character *outCharacter) {
    if (file == nullptr || outCharacter == nullptr) {
        return false;
    }

    std::string line;
    std::string name;
    int typeValue = 1;
    int auraPool = 100;
    std::string hatsuName;
    int hatsuPotency = 100;

    while (std::getline(*file, line)) {
        const std::size_t split = line.find('=');
        if (split == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, split);
        const std::string value = line.substr(split + 1);

        if (key == "name") {
            name = value;
        } else if (key == "type") {
            try {
                typeValue = std::stoi(value);
            } catch (...) {
                return false;
            }
        } else if (key == "aura") {
            try {
                auraPool = std::stoi(value);
            } catch (...) {
                return false;
            }
        } else if (key == "hatsu_name") {
            hatsuName = value;
        } else if (key == "hatsu_potency") {
            try {
                hatsuPotency = std::stoi(value);
            } catch (...) {
                return false;
            }
        }
    }

    if (name.empty() || typeValue < 1 || typeValue > 6) {
        return false;
    }

    *outCharacter = nen::Character{
        .name = name,
        .naturalType = static_cast<nen::Type>(typeValue),
        .auraPool = auraPool,
        .hatsuName = hatsuName,
        .hatsuPotency = hatsuPotency,
    };
    if (outCharacter->hatsuName.empty()) {
        outCharacter->hatsuName =
            nen::GenerateHatsuName(outCharacter->name, outCharacter->naturalType);
    }
    return true;
}

} // namespace

std::filesystem::path DefaultSaveDirectory() {
    const char *home = std::getenv("HOME");
    if (home != nullptr) {
        return std::filesystem::path(home) / ".nen_world" / "characters";
    }
    return std::filesystem::current_path() / "saves" / "characters";
}

bool SaveCharacter(const nen::Character &character, const std::filesystem::path &saveDir,
                   std::string *errorMessage) {
    std::error_code ec;
    std::filesystem::create_directories(saveDir, ec);
    if (ec) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not create save directory: " + ec.message();
        }
        return false;
    }

    const std::filesystem::path filePath = saveDir / (Slugify(character.name) + ".nenchar");
    std::ofstream file(filePath);
    if (!file) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not write save file: " + filePath.string();
        }
        return false;
    }

    file << "name=" << character.name << '\n';
    file << "type=" << static_cast<int>(character.naturalType) << '\n';
    file << "aura=" << character.auraPool << '\n';
    file << "hatsu_name=" << character.hatsuName << '\n';
    file << "hatsu_potency=" << character.hatsuPotency << '\n';
    return true;
}

std::vector<StoredCharacter> ListCharacters(const std::filesystem::path &saveDir,
                                            std::string *errorMessage) {
    std::vector<StoredCharacter> characters;
    std::error_code ec;
    if (!std::filesystem::exists(saveDir, ec)) {
        return characters;
    }
    if (ec) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not access save directory: " + ec.message();
        }
        return characters;
    }

    for (const auto &entry : std::filesystem::directory_iterator(saveDir, ec)) {
        if (ec) {
            if (errorMessage != nullptr) {
                *errorMessage = "Could not list save files: " + ec.message();
            }
            return characters;
        }
        if (!entry.is_regular_file() || entry.path().extension() != ".nenchar") {
            continue;
        }

        std::ifstream file(entry.path());
        nen::Character character;
        if (!ParseCharacter(&file, &character)) {
            continue;
        }
        characters.push_back(StoredCharacter{.filePath = entry.path(), .character = character});
    }

    std::sort(characters.begin(), characters.end(),
              [](const StoredCharacter &a, const StoredCharacter &b) {
                  return a.character.name < b.character.name;
              });
    return characters;
}

} // namespace game
