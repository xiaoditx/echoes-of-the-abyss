// fx.hpp - juice: particles, floating texts, screen shake, hitstop. Pure D2D rects/text.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../render/renderer.hpp"
#include <string>
#include <vector>

namespace game {

class Fx {
public:
    void update(float dt);
    void draw(render::Renderer& r); // world-space (call with camera active)

    // Burst of small colored rects.
    void burst(float x, float y, render::Color c, int count = 8, float speed = 180.0f);
    // Floating combat/pickup text (world space).
    void floatText(float x, float y, const std::wstring& text, render::Color c, float size = 18.0f);
    // Impact mark: a short bright streak that expands and fades. dir=+1 points
    // right, -1 left; used as the visible "hit trace" of every attack.
    void slash(float x, float y, float w, float h, render::Color c, float dir = 1.0f);

    void shake(float amp, float dur);
    float shakeX() const { return sx_; }
    float shakeY() const { return sy_; }

    // Hitstop: freezes gameplay briefly. Query with timeScale().
    void hitstop(float sec);
    float timeScale() const { return hitstopT_ > 0 ? 0.0f : 1.0f; }

    void clear();

private:
    struct Particle { float x, y, vx, vy, life, maxLife, size; render::Color c; };
    struct FloatText { float x, y, life; std::wstring text; render::Color c; float size; };
    struct Slash { float x, y, w, h, dir, life, maxLife; render::Color c; };
    std::vector<Particle> parts_;
    std::vector<FloatText> texts_;
    std::vector<Slash> slashes_;
    float shakeT_ = 0, shakeDur_ = 0, shakeAmp_ = 0;
    float sx_ = 0, sy_ = 0;
    float hitstopT_ = 0;
};

} // namespace game
