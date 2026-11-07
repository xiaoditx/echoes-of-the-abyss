// paths.hpp - Unicode-safe filesystem paths: exe dir, AppData dirs, asset resolution.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>

namespace core {

// Directory of the running executable (no trailing backslash).
const std::wstring& exeDir();

// %LOCALAPPDATA%\<appName> created on demand. Fallback: exeDir if AppData unavailable.
const std::wstring& appDataDir(const std::wstring& appName);

// Ensures subdirectory under appData (e.g. L"logs", L"saves"). Returns full path.
std::wstring ensureAppDataSubdir(const std::wstring& appName, const std::wstring& sub);

// Joins path segments with backslash.
std::wstring joinPath(const std::wstring& a, const std::wstring& b);

// Resolves an asset path relative to the exe directory (assets are deployed next to exe).
std::wstring assetPath(const std::wstring& relative);

bool fileExists(const std::wstring& path);

} // namespace core
