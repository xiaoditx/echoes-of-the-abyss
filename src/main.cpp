// main.cpp - Win32 entry point: window, message loop, fixed-timestep game loop.
// Target: Windows x64, Unicode only.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <objbase.h>

#include "core/logger.hpp"
#include "core/paths.hpp"
#include "core/crash.hpp"
#include "input/input.hpp"
#include "app/app.hpp"

namespace {

constexpr wchar_t kAppName[] = L"EchoesOfTheAbyss";
constexpr wchar_t kVersion[] = L"0.1.0";
constexpr wchar_t kClassName[] = L"EchoesOfTheAbyssWndClass";
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr int kMaxSteps = 3;

app::App g_app;
input::Input g_input;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        g_app.onResize();
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        g_input.onKey((int)wp, true);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        g_input.onKey((int)wp, false);
        return 0;
    case WM_KILLFOCUS:
        // The matching key-up goes to whichever window takes focus, so held keys
        // would stay latched forever (menus then ignore the next press).
        g_input.releaseAll();
        return 0;
    case WM_LBUTTONDOWN: g_input.onMouseButton(input::MouseBtn::Left, true); return 0;
    case WM_LBUTTONUP:   g_input.onMouseButton(input::MouseBtn::Left, false); return 0;
    case WM_RBUTTONDOWN: g_input.onMouseButton(input::MouseBtn::Right, true); return 0;
    case WM_RBUTTONUP:   g_input.onMouseButton(input::MouseBtn::Right, false); return 0;
    case WM_MBUTTONDOWN: g_input.onMouseButton(input::MouseBtn::Middle, true); return 0;
    case WM_MBUTTONUP:   g_input.onMouseButton(input::MouseBtn::Middle, false); return 0;
    case WM_MOUSEMOVE:
        g_input.onMouseMove((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;
    case WM_MOUSEWHEEL:
        g_input.onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_ERASEBKGND:
        return 1; // we draw everything
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int showCmd) {
    // --- logging & crash handler first ---
    const std::wstring logDir = core::ensureAppDataSubdir(kAppName, L"logs");
    core::Logger::init(logDir, kAppName, core::LogLevel::Trace);
    core::installCrashHandler(logDir, kVersion);
    LOG_INFO(L"=== %s %s starting ===", kAppName, kVersion);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) { LOG_FATAL(L"CoInitializeEx failed: 0x%08X", hr); return 1; }

    // --- window ---
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) { LOG_FATAL(L"RegisterClassExW failed"); return 1; }

    RECT rc{0, 0, 1280, 720};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, kClassName, kAppName, WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top,
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) { LOG_FATAL(L"CreateWindowExW failed"); return 1; }
    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    if (!g_app.init(hwnd)) { LOG_FATAL(L"app init failed"); return 1; }

    // --- main loop: fixed timestep with edge-safe input ---
    LARGE_INTEGER freq{}, prev{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    double acc = 0.0;

    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running || g_app.quitRequested()) break;

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        double frameSec = (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        prev = now;
        if (frameSec > 0.25) frameSec = 0.25; // clamp huge hitches
        acc += frameSec;

        g_input.beginFrame();
        int steps = 0;
        while (acc >= kFixedDt && steps < kMaxSteps) {
            if (steps == 0) {
                g_app.frame(g_input, kFixedDt);
            } else {
                input::Input noEdge = g_input; // copy held state, drop edges
                noEdge.clearEdges();
                g_app.frame(noEdge, kFixedDt);
            }
            acc -= kFixedDt;
            ++steps;
        }
        if (steps == kMaxSteps) acc = 0.0; // drop backlog, slow-mo instead of spiral
        if (steps == 0) {
            // nothing to simulate this tick; still render once
            g_app.frame(g_input, 0.0f);
        }
        g_input.endFrame();

        Sleep(1); // don't busy-spin
    }

    g_app.shutdown();
    CoUninitialize();
    LOG_INFO(L"=== %s exiting cleanly ===", kAppName);
    core::Logger::shutdown();
    return 0;
}
