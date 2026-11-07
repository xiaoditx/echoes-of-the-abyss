// com_ptr.hpp - minimal COM smart pointer (avoids dependency on WRL header quirks).
#pragma once

namespace core {

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    ComPtr& operator=(ComPtr&& o) noexcept {
        if (this != &o) { reset(); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }

    T* get() const { return p_; }
    T* operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }

    T** addressOf() { reset(); return &p_; }
    void reset() {
        if (p_) { p_->Release(); p_ = nullptr; }
    }
    // Attach without AddRef.
    void attach(T* p) { reset(); p_ = p; }

private:
    T* p_ = nullptr;
};

} // namespace core
