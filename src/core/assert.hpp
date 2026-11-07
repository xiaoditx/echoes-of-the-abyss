// assert.hpp - development assertions with file/line/expression logging.
// Release builds (NDEBUG) compile ASSERT/CHECK away except VERIFY (kept, logs Error).
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "logger.hpp"

namespace core {
[[noreturn]] void assertFail(const char* expr, const char* file, int line, const wchar_t* detail);
}

#ifdef NDEBUG
#   define ASSERT(expr) ((void)0)
#   define ASSERT_MSG(expr, ...) ((void)0)
#else
#   define ASSERT(expr) \
        do { if (!(expr)) ::core::assertFail(#expr, __FILE__, __LINE__, nullptr); } while (0)
#   define ASSERT_MSG(expr, ...) \
        do { if (!(expr)) ::core::assertFail(#expr, __FILE__, __LINE__, __VA_ARGS__); } while (0)
#endif

// CHECK stays in release builds too but only logs Error and aborts; use for unrecoverable runtime states.
#define CHECK(expr) \
    do { if (!(expr)) ::core::assertFail(#expr, __FILE__, __LINE__, nullptr); } while (0)
#define CHECK_MSG(expr, ...) \
    do { if (!(expr)) ::core::assertFail(#expr, __FILE__, __LINE__, __VA_ARGS__); } while (0)
