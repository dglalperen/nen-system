#pragma once

namespace nen {

enum class LogLevel { Debug, Info, Warn, Error };

// Set the minimum level that actually gets printed (default: Info)
void SetMinLogLevel(LogLevel level);

// Core log function — accepts printf-style format string
void LogMsg(LogLevel level, const char *fmt, ...);

} // namespace nen

// Convenience macros
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define NEN_DEBUG(...) nen::LogMsg(nen::LogLevel::Debug, __VA_ARGS__)
#define NEN_INFO(...)  nen::LogMsg(nen::LogLevel::Info,  __VA_ARGS__)
#define NEN_WARN(...)  nen::LogMsg(nen::LogLevel::Warn,  __VA_ARGS__)
#define NEN_ERROR(...) nen::LogMsg(nen::LogLevel::Error, __VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)
