// logger.hpp - leveled file logger with rotation. Unicode-safe paths.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

namespace core {

enum class LogLevel : int { Trace = 0, Debug, Info, Warn, Error, Fatal };

class Logger {
public:
    // Opens log file at <logDir>/<appName>.log with rotation (keeps up to 3 generations).
    static bool init(const std::wstring& logDir, const std::wstring& appName, LogLevel minLevel);
    static void shutdown();

    static void setMinLevel(LogLevel lv);
    static void write(LogLevel lv, const char* file, int line, const wchar_t* fmt, ...);
    static void writeA(LogLevel lv, const char* file, int line, const char* fmt, ...);

    // Last state string recorded for crash dumps (e.g. "state=Run area=2 room=3").
    static void setCrashContext(const wchar_t* ctx);
    static const std::wstring& crashContext();

private:
    static void rotateIfNeeded();
    static void vwrite(LogLevel lv, const char* file, int line, const wchar_t* fmt, va_list ap);
};

} // namespace core

// Convenience macros capture file/line.
#define LOG_TRACE(...) ::core::Logger::write(::core::LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) ::core::Logger::write(::core::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ::core::Logger::write(::core::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ::core::Logger::write(::core::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ::core::Logger::write(::core::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) ::core::Logger::write(::core::LogLevel::Fatal, __FILE__, __LINE__, __VA_ARGS__)
