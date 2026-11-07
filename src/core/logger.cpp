// logger.cpp - implementation. Writes UTF-8 log lines: [time][level][file:line] message
#include "logger.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <cstdarg>
#include <ctime>
#include <vector>

namespace core {

namespace {

std::mutex g_mutex;
FILE* g_file = nullptr;
LogLevel g_minLevel = LogLevel::Debug;
std::wstring g_logPath;
std::wstring g_crashCtx;
constexpr long long kMaxLogBytes = 4LL * 1024 * 1024; // rotate past 4 MB
constexpr int kKeepGenerations = 3;

const wchar_t* levelName(LogLevel lv) {
    switch (lv) {
        case LogLevel::Trace: return L"TRACE";
        case LogLevel::Debug: return L"DEBUG";
        case LogLevel::Info:  return L"INFO ";
        case LogLevel::Warn:  return L"WARN ";
        case LogLevel::Error: return L"ERROR";
        case LogLevel::Fatal: return L"FATAL";
    }
    return L"?";
}

std::string wideToUtf8(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}

void writeRaw(const std::string& s) {
    if (g_file) {
        fwrite(s.data(), 1, s.size(), g_file);
        fflush(g_file);
    }
}

} // namespace

void Logger::rotateIfNeeded() {
    // If existing log exceeds cap, rotate: app.log -> app.1.log, app.1 -> app.2, drop oldest.
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(g_logPath.c_str(), GetFileExInfoStandard, &fad)) return;
    const long long size = (static_cast<long long>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
    if (size < kMaxLogBytes) return;

    const size_t dot = g_logPath.find_last_of(L'.');
    const std::wstring stem = (dot == std::wstring::npos) ? g_logPath : g_logPath.substr(0, dot);
    for (int i = kKeepGenerations - 1; i >= 1; --i) {
        std::wstring older = stem + L"." + std::to_wstring(i) + L".log";
        std::wstring newer = stem + L"." + std::to_wstring(i + 1) + L".log";
        DeleteFileW(newer.c_str());
        MoveFileW(older.c_str(), newer.c_str());
    }
    MoveFileW(g_logPath.c_str(), (stem + L".1.log").c_str());
}

bool Logger::init(const std::wstring& logDir, const std::wstring& appName, LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_minLevel = minLevel;
    g_logPath = logDir + L"\\" + appName + L".log";
    rotateIfNeeded();
    g_file = _wfopen(g_logPath.c_str(), L"ab");
    if (!g_file) return false;
    // No BOM: keep appending friendly. Write session header.
    // NOTE: g_mutex already held here; use vwrite (non-locking path).
    time_t now = time(nullptr);
    wchar_t tbuf[64]{};
    tm tmv{};
    localtime_s(&tmv, &now);
    wcsftime(tbuf, 64, L"%Y-%m-%d %H:%M:%S", &tmv);
    wchar_t msg[128]{};
    swprintf(msg, 128, L"---- session start %ls ----", tbuf);
    wchar_t lineBuf[256]{};
    swprintf(lineBuf, 256, L"[%ls][%ls][%hs:%d] %ls\n", tbuf + 11, L"INFO ", "logger.cpp", __LINE__, msg);
    writeRaw(wideToUtf8(lineBuf));
    return true;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
}

void Logger::setMinLevel(LogLevel lv) { g_minLevel = lv; }

void Logger::vwrite(LogLevel lv, const char* file, int line, const wchar_t* fmt, va_list ap) {
    if (lv < g_minLevel || !g_file) return;
    // UCRT vswprintf treats %s as narrow char*; our convention is wide strings.
    // Rewrite unescaped %s -> %ls so call sites can stay portable.
    wchar_t wfmt[512]{};
    {
        size_t j = 0;
        for (size_t i = 0; fmt[i] && j < 510; ++i) {
            if (fmt[i] == L'%' && fmt[i + 1] == L's') {
                wfmt[j++] = L'%'; wfmt[j++] = L'l'; wfmt[j++] = L's'; ++i;
            } else if (fmt[i] == L'%' && fmt[i + 1] == L'%') {
                wfmt[j++] = L'%'; wfmt[j++] = L'%'; ++i;
            } else {
                wfmt[j++] = fmt[i];
            }
        }
        wfmt[j] = 0;
    }
    wchar_t msg[2048]{};
    vswprintf(msg, 2048, wfmt, ap);

    wchar_t tbuf[32]{};
    time_t now = time(nullptr);
    tm tmv{};
    localtime_s(&tmv, &now);
    wcsftime(tbuf, 32, L"%H:%M:%S", &tmv);

    // Keep only file basename for brevity.
    const char* base = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    wchar_t lineBuf[2400]{};
    swprintf(lineBuf, 2400, L"[%ls][%ls][%hs:%d] %ls\n", tbuf, levelName(lv), base, line, msg);
    writeRaw(wideToUtf8(lineBuf));
}

void Logger::write(LogLevel lv, const char* file, int line, const wchar_t* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);
    va_list ap;
    va_start(ap, fmt);
    vwrite(lv, file, line, fmt, ap);
    va_end(ap);
}

void Logger::writeA(LogLevel lv, const char* file, int line, const char* fmt, ...) {
    if (lv < g_minLevel) return;
    char msg[2048]{};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, 2048, fmt, ap);
    va_end(ap);
    std::lock_guard<std::mutex> lock(g_mutex);
    wchar_t wmsg[2048]{};
    MultiByteToWideChar(CP_UTF8, 0, msg, -1, wmsg, 2048);
    vwrite(lv, file, line, wmsg, nullptr);
}

void Logger::setCrashContext(const wchar_t* ctx) { g_crashCtx = ctx ? ctx : L""; }
const std::wstring& Logger::crashContext() { return g_crashCtx; }

void assertFail(const char* expr, const char* file, int line, const wchar_t* detail) {
    // Log as Fatal and show a message box in debug; abort in all builds.
    Logger::write(LogLevel::Fatal, file, line,
                  L"ASSERT FAILED: %hs%ls%ls", expr,
                  detail ? L" | " : L"", detail ? detail : L"");
#ifndef NDEBUG
    wchar_t buf[1024]{};
    swprintf(buf, 1024, L"Assertion failed:\n%hs\n\n%hs:%d%ls%ls",
             expr, file, line, detail ? L"\n\n" : L"", detail ? detail : L"");
    MessageBoxW(nullptr, buf, L"Assertion Failed", MB_ICONERROR | MB_OK);
#endif
    Logger::shutdown();
    abort();
}

} // namespace core
