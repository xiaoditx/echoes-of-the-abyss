// menu.hpp - reusable vertical menu: keyboard (Up/Down + Enter) and mouse (hover + click).
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>
#include <vector>

namespace input { class Input; }
namespace render { class Renderer; }

namespace ui {

struct MenuItem {
    std::wstring text;
    int id = 0;
    bool enabled = true;
};

class Menu {
public:
    void setItems(std::vector<MenuItem> items);
    void reset() { sel_ = 0; }

    // Returns clicked/confirmed item id, or -1. `menuY` must match the y passed to draw()
    // so mouse hover regions line up. Mouse position in physical pixels.
    int update(const input::Input& in, render::Renderer& r, float menuY);
    // Draws centered at vertical position y (logical px).
    void draw(render::Renderer& r, float y) const;

    int selected() const { return sel_; }
    int count() const { return (int)items_.size(); }

private:
    std::vector<MenuItem> items_;
    int sel_ = 0;

    static constexpr float kItemW = 520.0f;
    static constexpr float kItemH = 46.0f;
};

} // namespace ui
