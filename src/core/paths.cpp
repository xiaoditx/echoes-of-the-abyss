// paths.cpp - Unicode-safe path helpers.
#include "paths.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <vector>

namespace core {

namespace {
std::wstring g_exeDir;
std::wstring g_appData;
}

const std::wstring& exeDir() {
    if (g_exeDir.empty()) {
        std::vector<wchar_t> buf(MAX_PATH);
        DWORD len = 0;
        while (true) {
            len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (len == 0) { g_exeDir = L"."; break; }
            if (len < buf.size() - 1) break; // fits
            buf.resize(buf.size() * 2);
        }
        g_exeDir.assign(buf.data(), len);
        const size_t slash = g_exeDir.find_last_of(L"\\/");
        g_exeDir = (slash == std::wstring::npos) ? L"." : g_exeDir.substr(0, slash);
    }
    return g_exeDir;
}

const std::wstring& appDataDir(const std::wstring& appName) {
    if (!g_appData.empty()) return g_appData;
    PWSTR known = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known)) && known) {
        g_appData = known;
        CoTaskMemFree(known);
    } else {
        // Fallback: exe dir (portable mode).
        g_appData = exeDir();
    }
    g_appData += L"\\" + appName;
    CreateDirectoryW(g_appData.c_str(), nullptr);
    return g_appData;
}

std::wstring ensureAppDataSubdir(const std::wstring& appName, const std::wstring& sub) {
    std::wstring dir = appDataDir(appName) + L"\\" + sub;
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring joinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring assetPath(const std::wstring& relative) {
    return joinPath(exeDir(), relative);
}

bool fileExists(const std::wstring& path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

} // namespace core
