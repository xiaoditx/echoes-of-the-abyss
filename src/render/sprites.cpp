// sprites.cpp
#include "sprites.hpp"
#include "renderer.hpp"
#include "texture.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

namespace render {

void SpriteSet::addClip(const std::wstring& name, const AnimClip& clip) { clips_[name] = clip; }

const AnimClip* SpriteSet::find(const std::wstring& name) const {
    auto it = clips_.find(name);
    return it == clips_.end() ? nullptr : &it->second;
}

void Animator::play(const AnimClip* clip, bool restart) {
    if (!clip) return;
    if (clip_ == clip && !restart) return;
    clip_ = clip;
    curFrame_ = 0;
    t_ = 0.0f;
    finished_ = false;
}

void Animator::update(float dt) {
    if (!clip_ || finished_ || clip_->fps <= 0.0f || clip_->frameCount <= 1) return;
    t_ += dt;
    const float spf = 1.0f / clip_->fps;
    while (t_ >= spf) {
        t_ -= spf;
        ++curFrame_;
        if (curFrame_ >= clip_->frameCount) {
            if (clip_->loop) {
                curFrame_ = 0;
            } else {
                curFrame_ = clip_->frameCount - 1;
                finished_ = true;
                break;
            }
        }
    }
}

void Animator::draw(Renderer& r, TextureStore& store, float x, float y, float w, float h,
                    bool flipX, float opacity) const {
    if (!clip_) return;
    // clip_ may be stale (containers that move their elements don't move the clip
    // with it); a garbage path/frame size here must never reach the math below.
    if (clip_->frameW <= 0 || clip_->frameH <= 0 || clip_->frameCount <= 0) return;
    Texture* tex = store.get(r, clip_->texturePath);
    if (!tex || !tex->bmp()) return;
    const int cols = static_cast<int>(tex->width()) / clip_->frameW;
    if (cols <= 0 || static_cast<int>(tex->height()) < clip_->frameH) return;
    const int idx = clip_->startIndex + curFrame_;
    const float sx = static_cast<float>((idx % cols) * clip_->frameW);
    const float sy = static_cast<float>((idx / cols) * clip_->frameH);
    r.drawBitmap(tex->bmp(), x, y, w, h, sx, sy,
                 static_cast<float>(clip_->frameW), static_cast<float>(clip_->frameH), opacity, flipX);
}

} // namespace render
