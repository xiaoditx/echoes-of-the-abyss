// renderer.hpp - Direct2D + DirectWrite + WIC wrapper. Single hwnd render target,
// nearest-neighbor pixel-art sampling, logical 1280x720 with letterbox scaling.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../core/com_ptr.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <map>
#include <string>

namespace render {

// Logical (design) resolution. All game code draws in these coordinates;
// the renderer scales to the real client area with letterboxing.
inline constexpr float kLogicalW = 1280.0f;
inline constexpr float kLogicalH = 720.0f;

struct Color {
    float r, g, b, a;
    static Color rgb(float r_, float g_, float b_, float a_ = 1.0f) { return {r_, g_, b_, a_}; }
};

class Renderer {
public:
    bool init(HWND hwnd);
    void shutdown();

    // Recreates the render target after WM_SIZE. Call from the message handler.
    void onResize();

    bool begin(); // BeginDraw + clear black; returns false while target is being recreated.
    bool end();   // EndDraw; handles D2DERR_RECREATE_TARGET internally.

    // --- camera transform: world -> logical screen ---
    void setCamera(float x, float y); // world-space top-left of the view
    void clearCamera();

    // --- drawing ---
    void fillRect(float x, float y, float w, float h, Color c);
    void drawRect(float x, float y, float w, float h, Color c, float stroke = 1.0f);
    void drawBitmap(ID2D1Bitmap* bmp, float x, float y, float w, float h,
                    float srcX, float srcY, float srcW, float srcH,
                    float opacity = 1.0f, bool flipX = false);
    void drawBitmapWhole(ID2D1Bitmap* bmp, float x, float y, float opacity = 1.0f);

    // Text with cached formats; fontSize in logical pixels. align: 0=left 1=center 2=right.
    void drawText(const std::wstring& text, float x, float y, float w, float h,
                  float fontSize, Color c, int align = 0, bool bold = false);

    // Converts physical client pixels to logical 1280x720 coordinates (for mouse UI).
    void screenToLogical(int mx, int my, float& lx, float& ly) const;

    ID2D1HwndRenderTarget* rt() const { return rt_.get(); }
    IWICImagingFactory* wic() const { return wic_.get(); }

    // Device-dependent resources must be dropped/recreated on target loss.
    void releaseDeviceResources();

private:
    bool createDeviceResources();
    void computeViewTransform();

    HWND hwnd_ = nullptr;
    core::ComPtr<ID2D1Factory> d2d_;
    core::ComPtr<ID2D1HwndRenderTarget> rt_;
    core::ComPtr<IDWriteFactory> dwrite_;
    core::ComPtr<IWICImagingFactory> wic_;
    core::ComPtr<ID2D1SolidColorBrush> brush_;
    // DirectWrite formats are immutable in size: cache one per (align,bold,size bucket).
    // key = align * 10000 + bold * 1000 + sizeKey (size in quarter pixels).
    std::map<int, core::ComPtr<IDWriteTextFormat>> fmts_;
    IDWriteTextFormat* textFormat(float fontSize, int align, bool bold);

    D2D1_MATRIX_3X2_F view_{};   // letterbox scale+offset
    D2D1_MATRIX_3X2_F cam_;      // camera translate
    bool camActive_ = false;
    UINT32 pixelW_ = 0, pixelH_ = 0;
};

} // namespace render
