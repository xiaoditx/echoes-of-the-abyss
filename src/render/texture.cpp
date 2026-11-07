// texture.cpp - WIC decode (32bpp PBGRA) -> ID2D1Bitmap.
#include "texture.hpp"
#include "renderer.hpp"
#include "../core/logger.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <wincodec.h>

namespace render {

bool Texture::load(Renderer& r, const std::wstring& path) {
    if (bmp_) return true;
    path_ = path;

    core::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = r.wic()->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, decoder.addressOf());
    if (FAILED(hr)) { LOG_ERROR(L"WIC decoder open failed: %ls hr=0x%08X", path.c_str(), hr); return false; }

    core::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.addressOf());
    if (FAILED(hr)) { LOG_ERROR(L"WIC GetFrame failed: %ls", path.c_str()); return false; }

    core::ComPtr<IWICFormatConverter> conv;
    hr = r.wic()->CreateFormatConverter(conv.addressOf());
    if (FAILED(hr)) return false;
    hr = conv->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA,
                          WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { LOG_ERROR(L"WIC convert failed: %ls", path.c_str()); return false; }

    if (!r.rt()) { LOG_ERROR(L"no render target while loading %ls", path.c_str()); return false; }
    hr = r.rt()->CreateBitmapFromWicBitmap(conv.get(), nullptr, bmp_.addressOf());
    if (FAILED(hr)) { LOG_ERROR(L"CreateBitmapFromWicBitmap failed: %ls hr=0x%08X", path.c_str(), hr); return false; }

    D2D1_SIZE_F sz = bmp_->GetSize();
    w_ = sz.width;
    h_ = sz.height;
    LOG_DEBUG(L"texture loaded: %ls (%.0fx%.0f)", path.c_str(), w_, h_);
    return true;
}

void Texture::unload() { bmp_.reset(); }

void Texture::attachPlaceholder(core::ComPtr<ID2D1Bitmap> bmp) {
    bmp_ = std::move(bmp);
    w_ = h_ = 8.0f;
    path_ = L"__placeholder__";
}

namespace {

// 8x8 magenta/black checker to make missing assets obvious.
core::ComPtr<ID2D1Bitmap> makePlaceholder(Renderer& r) {
    if (!r.rt()) return {};
    const int n = 8;
    UINT32 px[8 * 8];
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            px[y * n + x] = ((x + y) & 1) ? 0xFFFF00FFu : 0xFF000000u; // AARRGGBB (PBGRA uint)
    core::ComPtr<ID2D1Bitmap> bmp;
    HRESULT hr = r.rt()->CreateBitmap(D2D1::SizeU(n, n), px, n * sizeof(UINT32),
                                      D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                                                               D2D1_ALPHA_MODE_PREMULTIPLIED)),
                                      bmp.addressOf());
    if (FAILED(hr)) return {};
    return bmp;
}

} // namespace

Texture* TextureStore::get(Renderer& r, const std::wstring& path) {
    auto it = map_.find(path);
    if (it == map_.end()) {
        auto res = map_.emplace(path, Texture{});
        it = res.first;
        it->second.load(r, path); // failure recorded; placeholder handled below
    }
    Texture& t = it->second;
    if (!t.bmp()) {
        // (Re)load attempt: either first failure or after device loss.
        if (!t.load(r, path)) {
            // Give back a placeholder texture instead of failing.
            auto pit = map_.find(L"__placeholder__");
            if (pit == map_.end()) {
                Texture ph;
                ph.attachPlaceholder(makePlaceholder(r));
                auto rres = map_.emplace(L"__placeholder__", std::move(ph));
                pit = rres.first;
            }
            return &pit->second;
        }
    }
    return &t;
}

void TextureStore::releaseAll() {
    for (auto& kv : map_) kv.second.unload();
}

void TextureStore::clear() { map_.clear(); }

} // namespace render
