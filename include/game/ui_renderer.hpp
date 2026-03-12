#pragma once

#include <string>
#include <string_view>

#include <raylib.h>

#include "game/app_state.hpp"
#include "nen/hatsu_spec.hpp"
#include "nen/types.hpp"

namespace game {

bool  IsInside        (const Rectangle &rect, Vector2 point);
bool  IsButtonTriggered(const Rectangle &rect);
void  DrawButton      (const Rectangle &rect, const std::string &label, bool selected);
float DrawWrappedText (std::string_view text, Rectangle bounds, int fontSize, float spacing,
                       Color tint, int maxLines = 0);

// IsCategoryNatural is used in both UpdateHatsuCreation (game.cpp) and DrawHatsuCreation here.
bool IsCategoryNatural(nen::Type type, nen::HatsuCategory cat);

void DrawMainMenu      (const AppState &app);
void DrawNameEntry     (const AppState &app);
void DrawLoadCharacter (const AppState &app);
void DrawQuiz          (const AppState &app);
void DrawReveal        (const AppState &app);
void DrawHatsuCreation (const AppState &app);
void DrawWorldSidebar  (const AppState &app);

// Calls DrawRectangleGradientV + DrawWorld3D + DrawWorldSidebar
void DrawWorld(const AppState &app);

} // namespace game
