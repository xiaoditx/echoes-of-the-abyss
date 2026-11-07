// sprites.hpp - grid-based sprite sheet animation over WIC-loaded textures.
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
#include <vector>

namespace render {

class Renderer;
class TextureStore;

// One animation clip: a row (or grid) of equally-sized frames inside one texture.
struct AnimClip {
    std::wstring texturePath; // absolute path key into TextureStore
    int frameW = 0, frameH = 0;
    int startIndex = 0;   // first frame index (row-major)
    int frameCount = 0;
    float fps = 10.0f;
    bool loop = true;
};

// Named set of clips for one character/prop.
class SpriteSet {
public:
    void addClip(const std::wstring& name, const AnimClip& clip);
    const AnimClip* find(const std::wstring& name) const;
    bool empty() const { return clips_.empty(); }
private:
    std::unordered_map<std::wstring, AnimClip> clips_;
};

// Playback state for one entity instance.
class Animator {
public:
    void play(const AnimClip* clip, bool restart = true);
    // Re-point at a clip's new address without resetting playback
    // (used after a container relocates/moves its elements).
    void retarget(const AnimClip* clip) { clip_ = clip; }
    void update(float dt); // advance frame timer
    // Draw current frame. size = destination size (scaled pixels), flipX mirrors.
    void draw(Renderer& r, TextureStore& store, float x, float y, float w, float h, bool flipX, float opacity = 1.0f) const;

    bool finished() const { return finished_; }
    int frameIndex() const { return curFrame_; }
    const AnimClip* clip() const { return clip_; }

private:
    const AnimClip* clip_ = nullptr;
    int curFrame_ = 0;
    float t_ = 0.0f;
    bool finished_ = false;
};

} // namespace render
