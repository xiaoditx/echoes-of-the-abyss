// renderer.cpp
#include "renderer.hpp"
#include "../core/logger.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

namespace render {

namespace {
D2D1_COLOR_F toD2D(Color c) { return D2D1::ColorF(c.r, c.g, c.b, c.a); }
}

bool Renderer::init(HWND hwnd) {
    hwnd_ = hwnd;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_.addressOf());
    if (FAILED(hr)) { LOG_ERROR(L"D2D1CreateFactory failed hr=0x%08X", hr); return false; }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(dwrite_.addressOf()));
    if (FAILED(hr)) { LOG_ERROR(L"DWriteCreateFactory failed hr=0x%08X", hr); return false; }

    // WIC factory (MinGW provides CLSID_WICImagingFactory without INITGUID).
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWICImagingFactory, reinterpret_cast<void**>(wic_.addressOf()));
    if (FAILED(hr)) { LOG_ERROR(L"CoCreateInstance(WIC) failed hr=0x%08X", hr); return false; }

    computeViewTransform();
    if (!createDeviceResources()) return false;

    LOG_INFO(L"Renderer initialized");
    return true;
}

void Renderer::computeViewTransform() {
    if (pixelW_ == 0 || pixelH_ == 0) { view_ = D2D1::Matrix3x2F::Identity(); return; }
    const float sx = static_cast<float>(pixelW_) / kLogicalW;
    const float sy = static_cast<float>(pixelH_) / kLogicalH;
    const float s = (sx < sy) ? sx : sy;
    const float ox = (pixelW_ - kLogicalW * s) * 0.5f;
    const float oy = (pixelH_ - kLogicalH * s) * 0.5f;
    view_ = D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(ox, oy);
}

bool Renderer::createDeviceResources() {
    if (rt_) return true; // already alive
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    pixelW_ = static_cast<UINT32>(rc.right - rc.left);
    pixelH_ = static_cast<UINT32>(rc.bottom - rc.top);
    if (pixelW_ == 0 || pixelH_ == 0) return true; // minimized; retry later

    HRESULT hr = d2d_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                     D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(pixelW_, pixelH_),
                                         D2D1_PRESENT_OPTIONS_NONE),
        rt_.addressOf());
    if (FAILED(hr)) { LOG_ERROR(L"CreateHwndRenderTarget failed hr=0x%08X", hr); return false; }
    rt_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    hr = rt_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), brush_.addressOf());
    if (FAILED(hr)) { LOG_ERROR(L"CreateSolidColorBrush failed hr=0x%08X", hr); return false; }

    computeViewTransform();
    return true;
}

void Renderer::releaseDeviceResources() {
    brush_.reset();
    rt_.reset();
}

void Renderer::onResize() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const UINT32 w = static_cast<UINT32>(rc.right - rc.left);
    const UINT32 h = static_cast<UINT32>(rc.bottom - rc.top);
    if (w == 0 || h == 0) return; // minimized
    if (rt_) rt_->Resize(D2D1::SizeU(w, h));
    pixelW_ = w;
    pixelH_ = h;
    computeViewTransform();
}

void Renderer::shutdown() {
    releaseDeviceResources();
    fmts_.clear();
    wic_.reset(); dwrite_.reset(); d2d_.reset();
}

bool Renderer::begin() {
    if (!rt_ && !createDeviceResources()) return false;
    if (!rt_) return false;
    rt_->BeginDraw();
    rt_->SetTransform(D2D1::Matrix3x2F::Identity());
    rt_->Clear(D2D1::ColorF(0, 0, 0, 1));
    rt_->SetTransform(view_);
    return true;
}

bool Renderer::end() {
    if (!rt_) return false;
    HRESULT hr = rt_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        releaseDeviceResources();
        return false;
    }
    return SUCCEEDED(hr);
}

void Renderer::setCamera(float x, float y) {
    cam_ = D2D1::Matrix3x2F::Translation(-x, -y);
    camActive_ = true;
    if (rt_) rt_->SetTransform(cam_ * view_);
}

void Renderer::clearCamera() {
    camActive_ = false;
    if (rt_) rt_->SetTransform(view_);
}

void Renderer::fillRect(float x, float y, float w, float h, Color c) {
    if (!rt_ || !brush_) return;
    brush_->SetColor(toD2D(c));
    rt_->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush_.get());
}

void Renderer::drawRect(float x, float y, float w, float h, Color c, float stroke) {
    if (!rt_ || !brush_) return;
    brush_->SetColor(toD2D(c));
    rt_->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), brush_.get(), stroke);
}

void Renderer::drawBitmap(ID2D1Bitmap* bmp, float x, float y, float w, float h,
                          float srcX, float srcY, float srcW, float srcH,
                          float opacity, bool flipX) {
    if (!rt_ || !bmp) return;
    D2D1_RECT_F dst = D2D1::RectF(x, y, x + w, y + h);
    D2D1_RECT_F src = D2D1::RectF(srcX, srcY, srcX + srcW, srcY + srcH);
    if (flipX) {
        // Mirror around destination center via transform.
        D2D1_MATRIX_3X2_F prev;
        rt_->GetTransform(&prev);
        D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Scale(-1.0f, 1.0f, D2D1::Point2F(x + w * 0.5f, 0)) * prev;
        rt_->SetTransform(m);
        rt_->DrawBitmap(bmp, dst, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, src);
        rt_->SetTransform(prev);
    } else {
        rt_->DrawBitmap(bmp, dst, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, src);
    }
}

void Renderer::drawBitmapWhole(ID2D1Bitmap* bmp, float x, float y, float opacity) {
    if (!bmp) return;
    D2D1_SIZE_F sz = bmp->GetSize();
    drawBitmap(bmp, x, y, sz.width, sz.height, 0, 0, sz.width, sz.height, opacity);
}

void Renderer::screenToLogical(int mx, int my, float& lx, float& ly) const {
    if (pixelW_ == 0 || pixelH_ == 0) { lx = 0; ly = 0; return; }
    const float sx = (float)pixelW_ / kLogicalW;
    const float sy = (float)pixelH_ / kLogicalH;
    const float s = (sx < sy) ? sx : sy;
    const float ox = (pixelW_ - kLogicalW * s) * 0.5f;
    const float oy = (pixelH_ - kLogicalH * s) * 0.5f;
    lx = ((float)mx - ox) / s;
    ly = ((float)my - oy) / s;
}

IDWriteTextFormat* Renderer::textFormat(float fontSize, int align, bool bold) {
    int sizeKey = (int)(fontSize * 4.0f + 0.5f);
    int key = align * 1000000 + (bold ? 1 : 0) * 100000 + sizeKey;
    auto it = fmts_.find(key);
    if (it != fmts_.end()) return it->second.get();
    core::ComPtr<IDWriteTextFormat> fmt;
    HRESULT hr = dwrite_->CreateTextFormat(
        L"Microsoft YaHei", nullptr,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        (float)sizeKey / 4.0f, L"zh-cn", fmt.addressOf());
    if (FAILED(hr) || !fmt) { LOG_WARN(L"CreateTextFormat failed hr=0x%08X", hr); return nullptr; }
    if (align == 1) fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    else if (align == 2) fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* raw = fmt.get();
    fmts_[key] = std::move(fmt);
    return raw;
}

void Renderer::drawText(const std::wstring& text, float x, float y, float w, float h,
                        float fontSize, Color c, int align, bool bold) {
    if (!rt_ || !brush_ || !dwrite_) return;
    IDWriteTextFormat* fmt = textFormat(fontSize, align, bold);
    if (!fmt) return;
    brush_->SetColor(toD2D(c));
    rt_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), fmt,
                   D2D1::RectF(x, y, x + w, y + h), brush_.get());
}

} // namespace render
