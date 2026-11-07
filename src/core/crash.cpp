// crash.cpp - writes logs/crash-<timestamp>.log with version, time, exception code, last context.
#include "crash.hpp"
#include "logger.hpp"
#include "paths.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <cstdio>
#include <ctime>

namespace core {

namespace {
std::wstring g_logDir;
std::wstring g_version;

const wchar_t* exceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return L"ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return L"ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT: return L"BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return L"DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return L"FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return L"ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return L"INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW: return L"INT_OVERFLOW";
        case EXCEPTION_STACK_OVERFLOW: return L"STACK_OVERFLOW";
        default: return L"UNKNOWN";
    }
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* ep) {
    wchar_t tbuf[32]{};
    time_t now = time(nullptr);
    tm tmv{};
    localtime_s(&tmv, &now);
    wcsftime(tbuf, 32, L"%Y%m%d-%H%M%S", &tmv);

    std::wstring path = g_logDir + L"\\crash-" + tbuf + L".log";
    FILE* f = _wfopen(path.c_str(), L"w, ccs=UTF-8");
    if (f) {
        const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
        const void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
        fwprintf(f, L"version: %ls\n", g_version.c_str());
        wchar_t human[64]{};
        wcsftime(human, 64, L"%Y-%m-%d %H:%M:%S", &tmv);
        fwprintf(f, L"time: %ls\n", human);
        fwprintf(f, L"exception: 0x%08lX (%ls) at %p\n", code, exceptionName(code), addr);
        fwprintf(f, L"last_state: %ls\n", Logger::crashContext().c_str());
        if (ep && ep->ContextRecord) {
            const CONTEXT& c = *ep->ContextRecord;
            fwprintf(f, L"rip=%llx rsp=%llx rbp=%llx\n", c.Rip, c.Rsp, c.Rbp);
            fwprintf(f, L"rax=%llx rbx=%llx rcx=%llx rdx=%llx\n", c.Rax, c.Rbx, c.Rcx, c.Rdx);
        }
        fclose(f);
    }
    Logger::write(LogLevel::Fatal, __FILE__, __LINE__, L"Unhandled exception, crash log: %ls", path.c_str());
    Logger::shutdown();
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void installCrashHandler(const std::wstring& logDir, const std::wstring& version) {
    g_logDir = logDir;
    g_version = version;
    SetUnhandledExceptionFilter(unhandledFilter);
}

} // namespace core
