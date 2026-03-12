#include "game/ui_renderer.hpp"

#include <array>
#include <string>

#include <raylib.h>

#include "game/app_state.hpp"
#include "game/game_constants.hpp"
#include "game/world_renderer.hpp"
#include "nen/hatsu.hpp"
#include "nen/hatsu_spec.hpp"
#include "nen/quiz.hpp"
#include "nen/types.hpp"

namespace game {

namespace {

const char *AnimStateName(AnimState state) {
    switch (state) {
    case AnimState::Idle:      return "Idle";
    case AnimState::Move:      return "Move";
    case AnimState::Charge:    return "Charge";
    case AnimState::CastBase:  return "CastBase";
    case AnimState::CastHatsu: return "CastHatsu";
    }
    return "Unknown";
}

} // namespace

bool IsInside(const Rectangle &rect, Vector2 point) {
    return CheckCollisionPointRec(point, rect);
}

bool IsButtonTriggered(const Rectangle &rect) {
    return IsInside(rect, GetMousePosition()) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void DrawButton(const Rectangle &rect, const std::string &label, bool selected) {
    const bool  hovered = IsInside(rect, GetMousePosition());
    const Color fill    = selected || hovered ? Color{57, 87, 143, 255} : Color{32, 50, 82, 255};
    DrawRectangleRounded(rect, 0.2F, 8, fill);
    DrawRectangleRoundedLinesEx(rect, 0.2F, 8, 2.0F,
                                selected || hovered ? WHITE : Fade(WHITE, 0.5F));
    DrawText(label.c_str(), static_cast<int>(rect.x + 18.0F),
             static_cast<int>(rect.y + rect.height * 0.28F), 28, RAYWHITE);
}

float DrawWrappedText(std::string_view text, Rectangle bounds, int fontSize, float spacing,
                      Color tint, int maxLines) {
    Font        font = GetFontDefault();
    std::string line;
    std::string word;
    float       y          = bounds.y;
    int         linesDrawn = 0;

    auto flushLine = [&](bool forceEllipsis) {
        if (line.empty()) {
            return true;
        }
        if (y + static_cast<float>(fontSize) > bounds.y + bounds.height) {
            return false;
        }
        std::string toDraw = line;
        if (forceEllipsis && toDraw.size() > 3) {
            toDraw = toDraw.substr(0, toDraw.size() - 3) + "...";
        }
        DrawTextEx(font, toDraw.c_str(), {bounds.x, y}, static_cast<float>(fontSize), spacing,
                   tint);
        line.clear();
        y += static_cast<float>(fontSize) + 5.0F;
        linesDrawn += 1;
        if (maxLines > 0 && linesDrawn >= maxLines) {
            return false;
        }
        return true;
    };

    auto appendWord = [&](std::string_view nextWord) {
        if (nextWord.empty()) {
            return true;
        }
        std::string candidate = line;
        if (!candidate.empty()) {
            candidate.push_back(' ');
        }
        candidate.append(nextWord);

        const float width =
            MeasureTextEx(font, candidate.c_str(), static_cast<float>(fontSize), spacing).x;
        if (width <= bounds.width) {
            line = std::move(candidate);
            return true;
        }
        if (!flushLine(false)) {
            return false;
        }

        line = std::string(nextWord);
        const float singleWidth =
            MeasureTextEx(font, line.c_str(), static_cast<float>(fontSize), spacing).x;
        if (singleWidth <= bounds.width) {
            return true;
        }

        std::string fragmented;
        for (char c : line) {
            std::string test = fragmented;
            test.push_back(c);
            const float testWidth =
                MeasureTextEx(font, test.c_str(), static_cast<float>(fontSize), spacing).x;
            if (testWidth > bounds.width && !fragmented.empty()) {
                line = fragmented;
                if (!flushLine(false)) {
                    return false;
                }
                fragmented.clear();
            }
            fragmented.push_back(c);
        }
        line = fragmented;
        return true;
    };

    for (char c : std::string(text)) {
        if (c == '\n') {
            if (!appendWord(word)) {
                return y;
            }
            word.clear();
            if (!flushLine(false)) {
                return y;
            }
            continue;
        }
        if (c == ' ' || c == '\t') {
            if (!appendWord(word)) {
                return y;
            }
            word.clear();
            continue;
        }
        word.push_back(c);
    }

    if (!appendWord(word)) {
        return y;
    }
    flushLine(false);
    return y;
}

bool IsCategoryNatural(nen::Type type, nen::HatsuCategory cat) {
    using C = nen::HatsuCategory;
    using T = nen::Type;
    switch (type) {
    case T::Enhancer:    return cat == C::Projectile || cat == C::Passive;
    case T::Transmuter:  return cat == C::Counter    || cat == C::Control;
    case T::Emitter:     return cat == C::Projectile || cat == C::Zone;
    case T::Conjurer:    return cat == C::Summon     || cat == C::Zone;
    case T::Manipulator: return cat == C::Control    || cat == C::Summon;
    case T::Specialist:  return true; // all natural for Specialist
    }
    return false;
}

void DrawMainMenu(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {11, 18, 34, 255});
    DrawText("Nen World", 86, 70, 64, RAYWHITE);
    DrawText("Create or load a hunter profile", 86, 148, 30, LIGHTGRAY);

    const Rectangle newButton{86.0F, 262.0F, 420.0F, 74.0F};
    const Rectangle loadButton{86.0F, 354.0F, 420.0F, 74.0F};
    DrawButton(newButton,  "New Character",  app.menuSelection == 0);
    DrawButton(loadButton, "Load Character", app.menuSelection == 1);

    DrawText("Keyboard or mouse both supported", 86, 456, 22, Fade(WHITE, 0.82F));
    DrawText("ESC to quit", 86, 492, 20, Fade(WHITE, 0.7F));
}

void DrawNameEntry(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});
    DrawText("Create New Hunter", 86, 60, 52, RAYWHITE);
    DrawText("Your name determines your hatsu's identity.", 86, 126, 26, LIGHTGRAY);

    const Rectangle inputBox{86.0F, 188.0F, 540.0F, 66.0F};
    DrawRectangleRounded(inputBox, 0.18F, 8, {24, 38, 64, 255});
    DrawRectangleRoundedLinesEx(inputBox, 0.18F, 8, 2.0F, WHITE);
    const bool        cursorVisible = static_cast<int>(GetTime() * 2.0) % 2 == 0;
    const std::string displayName   = app.draftName + (cursorVisible ? "_" : " ");
    DrawText(displayName.c_str(), 106, 208, 34, RAYWHITE);

    DrawText("After the quiz, you will name your own hatsu.", 86, 278, 22, Fade(WHITE, 0.38F));

    DrawButton({86.0F,  390.0F, 300.0F, 62.0F}, "Continue", false);
    DrawButton({402.0F, 390.0F, 184.0F, 62.0F}, "Back",     false);

    DrawText("Up to 20 characters. Press Enter or click Continue.", 86, 470, 20,
             Fade(WHITE, 0.40F));
}

void DrawLoadCharacter(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});
    DrawText("Load Character", 80, 70, 56, RAYWHITE);

    if (app.storedCharacters.empty()) {
        DrawText("No saved characters found.", 80, 210, 30, LIGHTGRAY);
        DrawText("Press ESC to return.",       80, 252, 24, LIGHTGRAY);
        return;
    }

    int y = 210;
    for (std::size_t i = 0; i < app.storedCharacters.size(); ++i) {
        const bool      selected = static_cast<int>(i) == app.loadSelection;
        const Rectangle row{80.0F, static_cast<float>(y), 640.0F, 40.0F};
        DrawRectangleRounded(row, 0.2F, 6,
                             selected ? Color{48, 74, 124, 255} : Color{27, 43, 72, 255});
        DrawRectangleRoundedLinesEx(row, 0.2F, 6, 2.0F, selected ? WHITE : Fade(WHITE, 0.45F));

        const auto       &ch   = app.storedCharacters[i].character;
        const std::string line = ch.name + " | " +
                                 std::string(nen::ToString(ch.naturalType)) +
                                 " | aura " + std::to_string(static_cast<int>(ch.auraPool.current));
        DrawText(line.c_str(), 96, y + 8, 24, RAYWHITE);
        y += 48;
    }
}

void DrawQuiz(const AppState &app) {
    const auto &questions = nen::PersonalityQuestions();
    const auto &question  = questions[app.quizQuestionIndex];
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});

    DrawText("Nen Personality Assessment", 86, 54, 48, RAYWHITE);

    {
        const int   total   = static_cast<int>(questions.size());
        const int   current = static_cast<int>(app.quizQuestionIndex);
        constexpr float barX = 86.0F;
        constexpr float barY = 112.0F;
        constexpr float barW = 700.0F;
        constexpr float barH = 8.0F;
        DrawRectangle(static_cast<int>(barX), static_cast<int>(barY),
                      static_cast<int>(barW),  static_cast<int>(barH), {30, 46, 76, 255});
        const float filled = barW * (static_cast<float>(current) / static_cast<float>(total));
        DrawRectangle(static_cast<int>(barX), static_cast<int>(barY),
                      static_cast<int>(filled), static_cast<int>(barH), {90, 160, 255, 200});
        for (int i = 1; i < total; ++i) {
            const float tx = barX + barW * (static_cast<float>(i) / static_cast<float>(total));
            DrawLine(static_cast<int>(tx), static_cast<int>(barY),
                     static_cast<int>(tx), static_cast<int>(barY + barH), Fade(WHITE, 0.25F));
        }
        DrawText(TextFormat("%d / %d", current + 1, total),
                 static_cast<int>(barX + barW + 12), static_cast<int>(barY - 2), 20, LIGHTGRAY);
    }

    DrawRectangleRounded({86.0F, 138.0F, 960.0F, 96.0F}, 0.12F, 6, {24, 38, 64, 255});
    DrawRectangleRoundedLinesEx({86.0F, 138.0F, 960.0F, 96.0F}, 0.12F, 6, 1.5F,
                                Fade(WHITE, 0.4F));
    DrawWrappedText(question.prompt, {108.0F, 158.0F, 920.0F, 76.0F}, 28, 1.0F, RAYWHITE, 2);

    constexpr float optX   = 86.0F;
    constexpr float optW   = 960.0F;
    constexpr float optH   = 72.0F;
    for (int i = 0; i < 3; ++i) {
        const Rectangle rect{optX, 258.0F + static_cast<float>(i) * 82.0F, optW, optH};
        const bool hovered = IsInside(rect, GetMousePosition());
        DrawRectangleRounded(rect, 0.12F, 8,
                             hovered ? Color{57, 87, 143, 255} : Color{28, 44, 74, 255});
        DrawRectangleRoundedLinesEx(rect, 0.12F, 8, hovered ? 2.0F : 1.5F,
                                    hovered ? WHITE : Fade(WHITE, 0.45F));
        const std::string label =
            std::to_string(i + 1) + ".  " +
            std::string(question.options[static_cast<std::size_t>(i)].text);
        DrawWrappedText(label, {rect.x + 18.0F, rect.y + 14.0F, rect.width - 36.0F, 44.0F},
                        24, 1.0F, RAYWHITE, 1);
    }

    DrawText("Press 1/2/3 or click an option", 86, 512, 20, Fade(WHITE, 0.40F));
}

void DrawReveal(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {20, 29, 50, 255}, {12, 19, 36, 255});
    DrawText("Water Divination", 86, 52, 50, RAYWHITE);
    DrawText(TextFormat("%s is a %s!", app.player.name.c_str(),
                        nen::ToString(app.player.naturalType).data()),
             86, 116, 44, TypeColor(app.player.naturalType));
    DrawText(nen::WaterDivinationEffect(app.player.naturalType).data(), 86, 174, 26, LIGHTGRAY);

    DrawText(nen::HatsuAbilityName(app.player.naturalType).data(), 86, 216, 26, LIGHTGRAY);
    DrawWrappedText(nen::HatsuAbilityDescription(app.player.naturalType),
                    {86.0F, 250.0F, 820.0F, 52.0F}, 20, 1.0F, Fade(WHITE, 0.60F), 2);

    DrawText("You will name and shape your hatsu on the next screen.",
             86, 318, 22, Fade(WHITE, 0.42F));

    const Rectangle glass{980.0F, 166.0F, 280.0F, 430.0F};
    DrawRectangleRounded(glass, 0.06F, 8, Fade({51, 80, 132, 255}, 0.44F));
    DrawRectangleRoundedLinesEx(glass, 0.06F, 8, 4.0F, WHITE);

    float waterLevel = 190.0F;
    Color waterBase  = {73, 136, 236, 255};
    Color glow       = TypeColor(app.player.naturalType);

    switch (app.player.naturalType) {
    case nen::Type::Enhancer:
        waterLevel = 190.0F + std::min(185.0F, app.revealTimer * 78.0F);
        break;
    case nen::Type::Transmuter:
        waterBase = {98, 216, 255, 255};
        break;
    case nen::Type::Emitter:
        waterBase = {150, 134, 252, 255};
        break;
    case nen::Type::Conjurer:
        waterBase = {111, 149, 238, 255};
        break;
    case nen::Type::Manipulator:
        waterBase = {101, 170, 228, 255};
        break;
    case nen::Type::Specialist:
        waterBase = {166, 104, 234, 255};
        break;
    }

    const Rectangle water{
        glass.x + 16.0F,
        glass.y + glass.height - waterLevel - 16.0F,
        glass.width - 32.0F,
        waterLevel,
    };
    DrawRectangleRounded(water, 0.08F, 8, waterBase);
    DrawRectangleRounded(water, 0.08F, 8, Fade(glow, 0.2F));

    for (int i = 0; i < 26; ++i) {
        const float t = static_cast<float>(i) / 25.0F;
        const float x = water.x + t * water.width;
        const float y = water.y + std::sin(t * 9.0F + app.revealTimer * 5.0F) * 5.5F;
        DrawCircle(static_cast<int>(x), static_cast<int>(y), 2.0F, Fade(WHITE, 0.85F));
    }

    DrawRectangle(
        static_cast<int>(glass.x + glass.width * 0.5F +
                         std::sin(app.revealTimer * 2.6F) * 36.0F),
        static_cast<int>(glass.y + 106.0F), 36, 12, {198, 230, 169, 255});

    DrawButton({86.0F, 716.0F, 340.0F, 66.0F}, "Continue", false);
}

void DrawHatsuCreation(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {18, 27, 46, 255}, {10, 18, 33, 255});

    const Color typeCol = TypeColor(app.player.naturalType);
    DrawText("Design Your Hatsu", 86, 46, 48, RAYWHITE);
    DrawText(TextFormat("You are a %s — choose the nature of your power.",
                        nen::ToString(app.player.naturalType).data()),
             86, 104, 24, typeCol);

    // Hatsu name input
    DrawText("Hatsu Name", 86, 152, 22, Fade(WHITE, 0.60F));
    const Rectangle inputBox{86.0F, 178.0F, 540.0F, 60.0F};
    DrawRectangleRounded(inputBox, 0.18F, 8, {24, 38, 64, 255});
    DrawRectangleRoundedLinesEx(inputBox, 0.18F, 8, 2.0F, WHITE);
    const bool        cursorBlink = static_cast<int>(GetTime() * 2.0) % 2 == 0;
    const std::string displayName = app.draftHatsuName + (cursorBlink ? "_" : " ");
    DrawText(displayName.c_str(), 106, 198, 30, RAYWHITE);
    if (app.draftHatsuName.empty()) {
        DrawText("e.g. \"Crimson Bind Thread\"", 110, 200, 26, Fade(WHITE, 0.28F));
    }

    // Category selector
    DrawText("Category  (1-6 keys or click)", 86, 302, 22, Fade(WHITE, 0.60F));
    constexpr float catY      = 330.0F;
    constexpr float catH      = 64.0F;
    constexpr float catW      = 166.0F;
    constexpr float catGap    = 10.0F;
    constexpr float catStartX = 86.0F;
    constexpr std::array<nen::HatsuCategory, 6> kCategories = {
        nen::HatsuCategory::Projectile, nen::HatsuCategory::Zone,
        nen::HatsuCategory::Control,    nen::HatsuCategory::Counter,
        nen::HatsuCategory::Summon,     nen::HatsuCategory::Passive,
    };
    for (int i = 0; i < 6; ++i) {
        const nen::HatsuCategory cat      = kCategories[static_cast<std::size_t>(i)];
        const Rectangle          r{catStartX + static_cast<float>(i) * (catW + catGap), catY,
                                   catW, catH};
        const bool selected = app.selectedHatsuCategoryIndex == i;
        const bool natural  = IsCategoryNatural(app.player.naturalType, cat);
        const bool hovered  = IsInside(r, GetMousePosition());

        Color fill = selected ? Color{57, 87, 143, 255}
                              : (hovered ? Color{40, 60, 100, 255} : Color{24, 38, 64, 255});
        DrawRectangleRounded(r, 0.18F, 8, fill);
        Color border = selected ? WHITE
                                : (natural ? Fade(typeCol, 0.75F) : Fade(WHITE, 0.30F));
        DrawRectangleRoundedLinesEx(r, 0.18F, 8, selected ? 2.5F : 1.5F, border);

        DrawText(nen::CategoryLabel(cat),
                 static_cast<int>(r.x + 10.0F), static_cast<int>(r.y + 8.0F), 20,
                 selected ? RAYWHITE : (natural ? typeCol : LIGHTGRAY));
        DrawText(nen::CategoryDescription(cat),
                 static_cast<int>(r.x + 10.0F), static_cast<int>(r.y + 34.0F), 13,
                 Fade(LIGHTGRAY, selected ? 0.9F : 0.55F));
        if (natural) {
            DrawText("*", static_cast<int>(r.x + catW - 16.0F),
                     static_cast<int>(r.y + 6.0F), 16, Fade(typeCol, 0.85F));
        }
    }
    DrawText("* = natural affinity for your type", 86, catY + catH + 6.0F, 16,
             Fade(typeCol, 0.55F));

    // Vow selector
    DrawText("Vows  (optional — pick 0-2 for a power bonus)", 86, 450.0F, 22,
             Fade(WHITE, 0.60F));

    constexpr float vowStartY = 476.0F;
    constexpr float vowH      = 28.0F;
    constexpr float vowGap    = 4.0F;
    constexpr float col2X     = 700.0F;

    constexpr std::array<std::pair<std::string_view, float>, 12> kVowDisplay = {{
        {"Cannot be used consecutively",           1.15F},
        {"Requires physical contact to activate",  1.40F},
        {"Only usable within 3m of target",        1.30F},
        {"Cannot be used against fleeing targets", 1.20F},
        {"Doubles aura cost in open terrain",      1.20F},
        {"Only one active target at a time",       1.25F},
        {"User cannot speak while active",         1.35F},
        {"Cannot be canceled once started",        1.20F},
        {"Activates only after being hit first",   1.50F},
        {"Usable only three times per encounter",  1.45F},
        {"Visible glow reveals user location",     1.30F},
        {"Costs double when not at full health",   1.25F},
    }};

    for (int i = 0; i < 12; ++i) {
        const bool    inCol2   = i >= 6;
        const float   vx       = inCol2 ? col2X : 86.0F;
        const float   vy       = vowStartY + static_cast<float>(inCol2 ? i - 6 : i) * (vowH + vowGap);
        const uint32_t bit     = 1u << static_cast<uint32_t>(i);
        const bool    checked  = (app.selectedVowMask & bit) != 0u;
        const Rectangle checkR{vx, vy + 4.0F, 16.0F, 16.0F};
        DrawRectangle(static_cast<int>(checkR.x), static_cast<int>(checkR.y),
                      static_cast<int>(checkR.width), static_cast<int>(checkR.height),
                      checked ? Color{220, 180, 80, 255} : Color{30, 46, 76, 255});
        DrawRectangleLines(static_cast<int>(checkR.x), static_cast<int>(checkR.y),
                           static_cast<int>(checkR.width), static_cast<int>(checkR.height),
                           Fade(WHITE, 0.40F));
        const Color textCol = checked ? Color{220, 180, 80, 255} : Fade(WHITE, 0.65F);
        DrawText(kVowDisplay[static_cast<std::size_t>(i)].first.data(),
                 static_cast<int>(vx + 24.0F), static_cast<int>(vy + 5.0F), 16, textCol);
        DrawText(TextFormat("x%.2f", kVowDisplay[static_cast<std::size_t>(i)].second),
                 static_cast<int>(vx + 24.0F + 336.0F), static_cast<int>(vy + 5.0F), 16,
                 Fade(textCol, 0.75F));
    }

    // Potency preview
    {
        const int   basePot = app.player.hatsuPotency;
        float       vowMult = 1.0F;
        for (int i = 0; i < 12; ++i) {
            if ((app.selectedVowMask & (1u << static_cast<uint32_t>(i))) != 0u) {
                vowMult *= kVowDisplay[static_cast<std::size_t>(i)].second;
            }
        }
        const int finalPot = static_cast<int>(static_cast<float>(basePot) * vowMult);
        DrawText(TextFormat("Potency: %d base  x%.2f vows  =  %d", basePot, vowMult, finalPot),
                 86, 720.0F, 22, RAYWHITE);
    }

    const bool canContinue = !app.draftHatsuName.empty();
    const Rectangle continueBtn{86.0F, 754.0F, 320.0F, 62.0F};
    DrawButton(continueBtn, "Confirm Hatsu", false);
    if (!canContinue) {
        DrawText("Enter a name to continue.", 420, 772, 20, Fade(WHITE, 0.40F));
    }

    DrawText("ESC to go back", 86, 828.0F, 18, Fade(WHITE, 0.35F));
}

void DrawWorldSidebar(const AppState &app) {
    const float panelX = kArenaBounds.x + kArenaBounds.width + kSidebarMargin;
    const float panelY = kSidebarTop;
    const float panelW = static_cast<float>(kWidth) - panelX - kSidebarMargin;
    const float panelH = static_cast<float>(kHeight) - panelY - kSidebarBottomMargin;
    const Rectangle panelRect{panelX, panelY, panelW, panelH};
    DrawRectangleRounded(panelRect, 0.03F, 6, {22, 34, 56, 255});
    DrawRectangleRoundedLinesEx(panelRect, 0.03F, 6, 2.0F, Fade(WHITE, 0.6F));

    const float textX  = panelX + 14.0F;
    const float textW  = panelW - 28.0F;
    const float bottom = panelY + panelH - 4.0F;
    float       y      = panelY + 14.0F;

    BeginScissorMode(static_cast<int>(panelX + 2), static_cast<int>(panelY + 2),
                     static_cast<int>(panelW - 4), static_cast<int>(panelH - 4));

    // Character
    DrawText(app.player.name.c_str(), static_cast<int>(textX), static_cast<int>(y), 22, RAYWHITE);
    y += 24.0F;
    DrawText(TextFormat("Type: %s", nen::ToString(app.player.naturalType).data()),
             static_cast<int>(textX), static_cast<int>(y), 20, TypeColor(app.player.naturalType));
    y += 22.0F;

    // Aura bar
    {
        const float barW   = textW;
        const float barH   = 10.0F;
        const float filled = barW * std::clamp(app.player.auraPool.current /
                                               app.player.auraPool.max, 0.0F, 1.0F);
        DrawRectangle(static_cast<int>(textX), static_cast<int>(y),
                      static_cast<int>(barW),  static_cast<int>(barH), {30, 46, 76, 255});
        DrawRectangle(static_cast<int>(textX), static_cast<int>(y),
                      static_cast<int>(filled), static_cast<int>(barH),
                      Fade(TypeColor(app.player.naturalType), 0.85F));
        DrawRectangleLines(static_cast<int>(textX), static_cast<int>(y),
                           static_cast<int>(barW), static_cast<int>(barH), Fade(WHITE, 0.3F));
        y += barH + 3.0F;
        DrawText(TextFormat("Aura  %.0f / %.0f", app.player.auraPool.current,
                            app.player.auraPool.max),
                 static_cast<int>(textX), static_cast<int>(y), 16, Fade(WHITE, 0.80F));
        y += 20.0F;
    }

    // Technique state
    DrawLine(static_cast<int>(textX), static_cast<int>(y),
             static_cast<int>(textX + textW), static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    const nen::TechniqueState &tech = app.player.techniques;
    const char *modeName = (tech.auraMode == nen::AuraMode::Zetsu) ? "Zetsu"
                         : (tech.auraMode == nen::AuraMode::Ren)   ? "Ren"
                                                                    : "Ten";
    const Color modeColor = (tech.auraMode == nen::AuraMode::Zetsu) ? Color{200, 200, 200, 255}
                          : (tech.auraMode == nen::AuraMode::Ren)   ? Color{255, 120, 80,  255}
                                                                     : Color{120, 200, 255, 255};
    DrawText(TextFormat("Mode: %s  Dmg x%.2f  Def x%.2f", modeName,
                        app.cachedNenStats.damageMultiplier, app.cachedNenStats.defenseMultiplier),
             static_cast<int>(textX), static_cast<int>(y), 16, modeColor);
    y += 20.0F;

    {
        std::string overlays;
        if (tech.gyoActive)  { overlays += "Gyo "; }
        if (tech.enActive)   { overlays += "En "; }
        if (tech.koPrepared) { overlays += TextFormat("Ko:%.1fs ", tech.koCharge); }
        if (!overlays.empty()) {
            DrawText(overlays.c_str(), static_cast<int>(textX), static_cast<int>(y), 16,
                     {220, 220, 100, 255});
            y += 20.0F;
        }
    }

    // Hatsu
    DrawLine(static_cast<int>(textX), static_cast<int>(y),
             static_cast<int>(textX + textW), static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    y = DrawWrappedText(app.player.hatsuName, {textX, y, textW, 44.0F}, 18, 1.0F, RAYWHITE, 2);
    y = DrawWrappedText(nen::HatsuAbilityName(app.player.naturalType),
                        {textX, y + 2.0F, textW, 22.0F}, 16, 1.0F, LIGHTGRAY, 1);
    DrawText(TextFormat("Potency %d  |  Base: %s", app.player.hatsuPotency,
                        nen::ToString(app.selectedBaseType).data()),
             static_cast<int>(textX), static_cast<int>(y + 2.0F), 16, Fade(WHITE, 0.65F));
    y += 22.0F;

    if (!app.cachedHatsuSpec.vows.empty()) {
        DrawText("Vows", static_cast<int>(textX), static_cast<int>(y), 16, {220, 180, 80, 255});
        y += 18.0F;
        for (const auto &vow : app.cachedHatsuSpec.vows) {
            y = DrawWrappedText(vow.description, {textX + 6.0F, y, textW - 6.0F, 20.0F}, 15, 1.0F,
                                Fade({220, 180, 80, 255}, 0.85F), 1);
        }
        DrawText(TextFormat("x%.2f potency", nen::ComputeVowMultiplier(app.cachedHatsuSpec)),
                 static_cast<int>(textX), static_cast<int>(y), 15,
                 Fade({220, 180, 80, 255}, 0.7F));
        y += 18.0F;
    }

    // Model/Anim
    DrawLine(static_cast<int>(textX), static_cast<int>(y),
             static_cast<int>(textX + textW), static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    const Color modelStatusColor =
        app.hasPlayerModel ? Color{150, 234, 170, 255} : Color{250, 190, 128, 255};
    DrawText(TextFormat("Model: %s  Scale: %.2f", app.hasPlayerModel ? "OK" : "fallback",
                        app.playerModelScale),
             static_cast<int>(textX), static_cast<int>(y), 15, modelStatusColor);
    y += 18.0F;
    {
        const char *animMode = app.playerAnimationCount > 0 ? "clip" : "proc";
        DrawText(TextFormat("Anim: %s (%s)", animMode, AnimStateName(app.animationState)),
                 static_cast<int>(textX), static_cast<int>(y), 15, Fade(WHITE, 0.55F));
        y += 18.0F;
    }

    // Combat
    DrawLine(static_cast<int>(textX), static_cast<int>(y),
             static_cast<int>(textX + textW), static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    DrawText(TextFormat("CD  base %.2fs  hatsu %.2fs",
                        app.baseAttackCooldown, app.hatsuCooldown),
             static_cast<int>(textX), static_cast<int>(y), 16, LIGHTGRAY);
    y += 20.0F;
    DrawText(TextFormat("Enemy HP: %d / %d", app.enemy.health, app.enemy.maxHealth),
             static_cast<int>(textX), static_cast<int>(y), 16, LIGHTGRAY);
    y += 20.0F;

    if (app.enemy.manipulatedTimer > 0.0F) {
        DrawText(TextFormat("Manipulated %.1fs", app.enemy.manipulatedTimer),
                 static_cast<int>(textX), static_cast<int>(y), 15, {255, 210, 120, 255});
        y += 18.0F;
    }
    if (app.enemy.vulnerabilityTimer > 0.0F) {
        DrawText(TextFormat("Vulnerable %.1fs", app.enemy.vulnerabilityTimer),
                 static_cast<int>(textX), static_cast<int>(y), 15, {255, 121, 210, 255});
        y += 18.0F;
    }
    if (app.enemy.elasticTimer > 0.0F) {
        DrawText(TextFormat("Elastic %.1fs", app.enemy.elasticTimer),
                 static_cast<int>(textX), static_cast<int>(y), 15, {133, 228, 255, 255});
        y += 18.0F;
    }

    // Hatsu description
    DrawLine(static_cast<int>(textX), static_cast<int>(y),
             static_cast<int>(textX + textW), static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    DrawText("Hatsu", static_cast<int>(textX), static_cast<int>(y), 16, RAYWHITE);
    y += 20.0F;
    y = DrawWrappedText(nen::HatsuAbilityDescription(app.player.naturalType),
                        {textX, y, textW, 48.0F}, 15, 1.0F, Fade(WHITE, 0.70F), 3);
    y += 4.0F;

    // Controls
    DrawLine(static_cast<int>(textX), static_cast<int>(y),
             static_cast<int>(textX + textW), static_cast<int>(y), Fade(WHITE, 0.15F));
    y += 6.0F;

    if (y < bottom - 20.0F) {
        DrawText("Controls", static_cast<int>(textX), static_cast<int>(y), 16, RAYWHITE);
        y += 20.0F;
        const char *controlLines[] = {
            "WASD move  |  LMB/SPC attack  |  RMB/Q hatsu",
            "1-6 base type  |  R recharge aura",
            "E Ren  Z Zetsu  G Gyo  N En  K Ko",
            "MMB orbit  |  wheel zoom  |  +/- scale",
            "F5 save  |  ESC back",
        };
        for (const char *line : controlLines) {
            if (y >= bottom - 16.0F) {
                break;
            }
            y = DrawWrappedText(line, {textX, y, textW, 18.0F}, 14, 1.0F,
                                Fade(WHITE, 0.50F), 1);
        }
    }

    EndScissorMode();
}

void DrawWorld(const AppState &app) {
    DrawRectangleGradientV(0, 0, kWidth, kHeight, {15, 24, 41, 255}, {8, 14, 27, 255});
    DrawWorld3D(app);
    DrawWorldSidebar(app);
}

} // namespace game
