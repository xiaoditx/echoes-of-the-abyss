// menu.cpp - reusable vertical menu implementation.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "menu.hpp"
#include "../input/input.hpp"
#include "../render/renderer.hpp"
#include <windows.h>

namespace ui {

void Menu::setItems(std::vector<MenuItem> items) {
    items_ = std::move(items);
    if (sel_ >= (int)items_.size()) sel_ = 0;
    // skip disabled entries
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].enabled) { sel_ = (int)i; break; }
    }
}

int Menu::update(const input::Input& in, render::Renderer& r, float menuY) {
    if (items_.empty()) return -1;

    auto step = [&](int dir) {
        for (int i = 0; i < (int)items_.size(); ++i) {
            sel_ = (sel_ + dir + (int)items_.size()) % (int)items_.size();
            if (items_[sel_].enabled) break;
        }
    };

    if (in.pressed(VK_UP)) step(-1);
    if (in.pressed(VK_DOWN)) step(1);

    // mouse hover + click (convert to logical coords)
    float lx = 0, ly = 0;
    r.screenToLogical(in.mouseX(), in.mouseY(), lx, ly);
    float x0 = (render::kLogicalW - kItemW) * 0.5f;
    int hitIdx = -1;
    for (size_t i = 0; i < items_.size(); ++i) {
        float iy = menuY + (float)i * kItemH;
        if (lx >= x0 && lx <= x0 + kItemW && ly >= iy && ly <= iy + kItemH) {
            hitIdx = (int)i;
            break;
        }
    }
    // Hover may only take over the selection when the cursor actually moves:
    // a resting cursor would otherwise pin the highlight and make Up/Down look dead.
    if (hitIdx >= 0 && items_[hitIdx].enabled && in.mouseMoved()) sel_ = hitIdx;
    if (hitIdx >= 0 && in.mousePressed(input::MouseBtn::Left)) {
        if (items_[hitIdx].enabled) { sel_ = hitIdx; return items_[hitIdx].id; }
    }
    if (in.pressed(VK_RETURN) || in.pressed(VK_SPACE))
        return items_[sel_].enabled ? items_[sel_].id : -1;
    return -1;
}

void Menu::draw(render::Renderer& r, float y) const {
    float x0 = (render::kLogicalW - kItemW) * 0.5f;
    float y0 = y;
    for (size_t i = 0; i < items_.size(); ++i) {
        const MenuItem& it = items_[i];
        float iy = y0 + (float)i * kItemH;
        bool sel = (int)i == sel_ && it.enabled;
        if (sel)
            r.fillRect(x0, iy, kItemW, kItemH - 6, render::Color::rgb(0.22f, 0.2f, 0.3f, 0.85f));
        render::Color c = !it.enabled ? render::Color::rgb(0.4f, 0.4f, 0.45f)
                        : sel          ? render::Color::rgb(1.0f, 0.92f, 0.55f)
                                       : render::Color::rgb(0.82f, 0.82f, 0.88f);
        r.drawText(it.text, x0, iy + 6, kItemW, kItemH - 12, 22, c, 1, sel);
    }
}

} // namespace ui
