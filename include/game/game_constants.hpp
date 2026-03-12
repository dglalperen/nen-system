#pragma once
#include <raylib.h>

namespace game {

constexpr int   kWidth               = 1760;
constexpr int   kHeight              = 980;
constexpr float kWorldScale          = 0.035F;
constexpr float kSidebarMargin       = 28.0F;
constexpr float kSidebarTop          = 88.0F;
constexpr float kSidebarBottomMargin = 94.0F;
inline const Rectangle kArenaBounds{40.0F, 110.0F, 1180.0F, 760.0F};

} // namespace game
