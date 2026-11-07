// input.cpp
#include "input.hpp"

namespace input {

void Input::onKey(int vk, bool down) {
    vk &= 0xFF;
    if (down) {
        if (!pending_[vk]) pendingPress_[vk] = true; // filter auto-repeat
        pending_[vk] = true;
    } else {
        if (pending_[vk]) pendingRelease_[vk] = true;
        pending_[vk] = false;
    }
}

void Input::onMouseButton(MouseBtn b, bool down) {
    const size_t i = static_cast<size_t>(b);
    if (down) {
        if (!mPending_[i]) mPendingPress_[i] = true;
        mPending_[i] = true;
    } else {
        if (mPending_[i]) mPendingRelease_[i] = true;
        mPending_[i] = false;
    }
}

void Input::beginFrame() {
    cur_ = pending_;
    pressEdge_ = pendingPress_;
    releaseEdge_ = pendingRelease_;
    pendingPress_.fill(false);
    pendingRelease_.fill(false);

    mCur_ = mPending_;
    mPressEdge_ = mPendingPress_;
    mReleaseEdge_ = mPendingRelease_;
    mPendingPress_.fill(false);
    mPendingRelease_.fill(false);

    mMoved_ = false;
}

void Input::endFrame() {
    wheel_ = 0;
}

void Input::clearEdges() {
    pressEdge_.fill(false);
    releaseEdge_.fill(false);
    mPressEdge_.fill(false);
    mReleaseEdge_.fill(false);
    wheel_ = 0;
}

void Input::releaseAll() {
    pending_.fill(false);
    mPending_.fill(false);
    pendingPress_.fill(false);
    pendingRelease_.fill(false);
    cur_.fill(false);
    mCur_.fill(false);
}

} // namespace input
