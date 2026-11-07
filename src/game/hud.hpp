// hud.hpp - in-run heads-up display: health, potions, currencies, boss bar, level banner.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

namespace render { class Renderer; class TextureStore; }

namespace game {

struct World;

// Draws the HUD in logical screen space (call with camera cleared).
void drawHud(World& w, render::Renderer& r, render::TextureStore& ts);

} // namespace game
