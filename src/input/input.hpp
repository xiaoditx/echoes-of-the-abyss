// input.hpp - keyboard + mouse state with edge detection. Message-driven (WM_KEY* / WM_*BUTTON*).
// Edge presses are latched so a quick tap within one frame is never lost;
// key auto-repeat does not generate new presses.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <array>

namespace input {

enum class MouseBtn { Left = 0, Right = 1, Middle = 2, Count };

class Input {
public:
    // Feed window messages here.
    void onKey(int vk, bool down);
    void onMouseButton(MouseBtn b, bool down);
    void onMouseMove(int x, int y) { mx_ = x; my_ = y; mMoved_ = true; }
    void onMouseWheel(int delta) { wheel_ += delta; }

    // Call once per frame BEFORE game logic reads state.
    void beginFrame();
    // Call at end of frame.
    void endFrame();

    // Drops all latched press/release edges (used for extra fixed-steps within one frame
    // so one physical tap cannot trigger multiple gameplay actions).
    void clearEdges();
    // Drops all held keys/buttons (focus loss: the matching key-up lands elsewhere).
    void releaseAll();

    bool down(int vk) const { return cur_[vk & 0xFF]; }
    bool pressed(int vk) const { return pressEdge_[vk & 0xFF]; }  // latched this frame
    bool released(int vk) const { return releaseEdge_[vk & 0xFF]; }

    bool mouseDown(MouseBtn b) const { return mCur_[static_cast<int>(b)]; }
    bool mousePressed(MouseBtn b) const { return mPressEdge_[static_cast<int>(b)]; }
    bool mouseReleased(MouseBtn b) const { return mReleaseEdge_[static_cast<int>(b)]; }

    int mouseX() const { return mx_; }
    int mouseY() const { return my_; }
    // True if the cursor actually moved since the last beginFrame().
    bool mouseMoved() const { return mMoved_; }
    int wheelDelta() const { return wheel_; }

private:
    static constexpr size_t kM = static_cast<size_t>(MouseBtn::Count);

    std::array<bool, 256> cur_{};
    std::array<bool, 256> pending_{};
    std::array<bool, 256> pressEdge_{};
    std::array<bool, 256> releaseEdge_{};
    std::array<bool, 256> pendingPress_{};
    std::array<bool, 256> pendingRelease_{};

    std::array<bool, kM> mCur_{};
    std::array<bool, kM> mPending_{};
    std::array<bool, kM> mPressEdge_{};
    std::array<bool, kM> mReleaseEdge_{};
    std::array<bool, kM> mPendingPress_{};
    std::array<bool, kM> mPendingRelease_{};

    int mx_ = 0, my_ = 0;
    int wheel_ = 0;
    bool mMoved_ = false;
};

} // namespace input
