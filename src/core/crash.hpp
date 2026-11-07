// crash.hpp - unhandled exception filter writing a crash log. Active in release builds too.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>

namespace core {

// Installs the unhandled-exception filter. logDir must exist.
void installCrashHandler(const std::wstring& logDir, const std::wstring& version);

} // namespace core
