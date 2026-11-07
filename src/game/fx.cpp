// fx.cpp
#include "fx.hpp"
#include <cmath>
#include <cstdlib>

namespace game {

void Fx::update(float dt) {
    // hitstop runs on real dt (it freezes gameplay, not itself)
    if (hitstopT_ > 0) hitstopT_ -= dt;

    for (auto& p : parts_) {
        p.life -= dt;
        p.vy += 900.0f * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
    for (size_t i = 0; i < parts_.size();)
        if (parts_[i].life <= 0) { parts_[i] = parts_.back(); parts_.pop_back(); }
        else ++i;

    for (auto& t : texts_) { t.life -= dt; t.y -= 34.0f * dt; }
    for (size_t i = 0; i < texts_.size();)
        if (texts_[i].life <= 0) { texts_[i] = texts_.back(); texts_.pop_back(); }
        else ++i;

    for (auto& s : slashes_) s.life -= dt;
    for (size_t i = 0; i < slashes_.size();)
        if (slashes_[i].life <= 0) { slashes_[i] = slashes_.back(); slashes_.pop_back(); }
        else ++i;

    if (shakeT_ > 0) {
        shakeT_ -= dt;
        float k = shakeDur_ > 0 ? (shakeT_ / shakeDur_) : 0;
        float a = shakeAmp_ * k;
        sx_ = ((float)rand() / RAND_MAX * 2 - 1) * a;
        sy_ = ((float)rand() / RAND_MAX * 2 - 1) * a;
    } else { sx_ = sy_ = 0; }
}

void Fx::draw(render::Renderer& r) {
    for (auto& p : parts_) {
        float a = p.maxLife > 0 ? (p.life / p.maxLife) : 1;
        render::Color c = p.c; c.a = a;
        r.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size, c);
    }
    for (auto& t : texts_) {
        float a = t.life > 0.8f ? 1.0f : t.life / 0.8f;
        render::Color c = t.c; c.a = a;
        r.drawText(t.text, t.x - 80, t.y, 160, 24, t.size, c, 1, true);
    }
    for (auto& s : slashes_) {
        float k = s.maxLife > 0 ? (s.life / s.maxLife) : 0; // 1 -> 0
        float grow = 1.0f + (1.0f - k) * 0.6f;
        float w = s.w * grow;
        float x = s.dir > 0 ? s.x : s.x - (w - s.w);
        render::Color c = s.c; c.a = 0.85f * k;
        r.fillRect(x, s.y, w, s.h, c);
        render::Color edge = render::Color::rgb(1, 1, 1, 0.55f * k);
        r.drawRect(x, s.y, w, s.h, edge, 1.5f);
    }
}

void Fx::burst(float x, float y, render::Color c, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        float ang = (float)rand() / RAND_MAX * 6.2831853f;
        float sp = speed * (0.4f + 0.6f * ((float)rand() / RAND_MAX));
        float life = 0.25f + 0.35f * ((float)rand() / RAND_MAX);
        parts_.push_back({x, y, cosf(ang) * sp, sinf(ang) * sp - 60.0f, life, life,
                          4.0f + 4.0f * ((float)rand() / RAND_MAX), c});
    }
}

void Fx::floatText(float x, float y, const std::wstring& text, render::Color c, float size) {
    texts_.push_back({x, y, 1.2f, text, c, size});
}

void Fx::slash(float x, float y, float w, float h, render::Color c, float dir) {
    slashes_.push_back({x, y, w, h, dir, 0.20f, 0.20f, c});
}

void Fx::shake(float amp, float dur) {
    shakeAmp_ = amp; shakeDur_ = dur; shakeT_ = dur;
}

void Fx::hitstop(float sec) { hitstopT_ = sec; }

void Fx::clear() {
    parts_.clear(); texts_.clear(); slashes_.clear();
    shakeT_ = shakeDur_ = shakeAmp_ = sx_ = sy_ = hitstopT_ = 0;
}

} // namespace game
