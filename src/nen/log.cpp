#include "nen/log.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace nen {

namespace {

// ANSI colour codes (no-op on platforms that don't support them; terminals on
// macOS/Linux handle them fine)
constexpr const char *kReset  = "\033[0m";
constexpr const char *kGrey   = "\033[90m";
constexpr const char *kCyan   = "\033[36m";
constexpr const char *kYellow = "\033[33m";
constexpr const char *kRed    = "\033[31m";

LogLevel sMinLevel = LogLevel::Info;

const char *LevelTag(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?    ";
}

const char *LevelColor(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return kGrey;
    case LogLevel::Info:  return kCyan;
    case LogLevel::Warn:  return kYellow;
    case LogLevel::Error: return kRed;
    }
    return kReset;
}

// Returns elapsed seconds since first call (process uptime)
double ElapsedSeconds() {
    static const auto kStart = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - kStart).count();
}

} // namespace

void SetMinLogLevel(LogLevel level) { sMinLevel = level; }

void LogMsg(LogLevel level, const char *fmt, ...) {
    if (level < sMinLevel) { return; }

    const double t = ElapsedSeconds();
    const int    minutes = static_cast<int>(t) / 60;
    const double seconds = t - static_cast<double>(minutes * 60);

    FILE *out = (level == LogLevel::Error || level == LogLevel::Warn) ? stderr : stdout;

    std::fprintf(out, "%s[%02d:%05.2f]%s %s%s%s  ",
                 kGrey, minutes, seconds, kReset,
                 LevelColor(level), LevelTag(level), kReset);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(out, fmt, args);
    va_end(args);

    std::fputc('\n', out);
}

} // namespace nen
