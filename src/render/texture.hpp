// texture.hpp - WIC PNG loading into ID2D1Bitmap. Device-dependent: reload on target loss.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../core/com_ptr.hpp"
#include <d2d1.h>
#include <string>
#include <unordered_map>

namespace render {

class Renderer;

class Texture {
public:
    // path: absolute Unicode path. Returns false on failure (logged).
    bool load(Renderer& r, const std::wstring& path);
    void unload(); // drop device-dependent bitmap (keeps path for reload)

    // Attaches an already-created bitmap (used for the built-in placeholder).
    void attachPlaceholder(core::ComPtr<ID2D1Bitmap> bmp);

    ID2D1Bitmap* bmp() const { return bmp_.get(); }
    float width() const { return w_; }
    float height() const { return h_; }
    const std::wstring& path() const { return path_; }

private:
    core::ComPtr<ID2D1Bitmap> bmp_;
    std::wstring path_;
    float w_ = 0, h_ = 0;
};

// Central texture store with lazy load and automatic reload after device loss.
class TextureStore {
public:
    // Get or load. Never null-returned silently: on failure returns the built-in
    // magenta checker placeholder so missing assets are visible in-game.
    Texture* get(Renderer& r, const std::wstring& path);
    void releaseAll();      // call on device loss
    void clear();

private:
    std::unordered_map<std::wstring, Texture> map_;
};

} // namespace render
