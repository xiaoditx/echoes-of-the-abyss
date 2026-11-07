// hud.cpp - in-run HUD drawing (screen space, logical 1280x720).
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "hud.hpp"
#include "world.hpp"
#include "tables.hpp"
#include "strings.hpp"
#include "enemies.hpp"
#include "../render/renderer.hpp"
#include "../render/texture.hpp"
#include <algorithm>

namespace game {

namespace {
using render::Color;

void bar(render::Renderer& r, float x, float y, float w, float h, float frac,
         Color bg, Color fg) {
    frac = std::max(0.0f, std::min(1.0f, frac));
    r.fillRect(x - 2, y - 2, w + 4, h + 4, Color::rgb(0.05f, 0.05f, 0.07f, 0.85f));
    r.fillRect(x, y, w, h, bg);
    if (frac > 0.0f) r.fillRect(x, y, w * frac, h, fg);
}
} // namespace

void drawHud(World& w, render::Renderer& r, render::TextureStore& ts) {
    const Strings& S = *w.strings;

    // ---- top-left: HP + potions + weapon ----
    bar(r, 20, 18, 260, 20, w.player.hp / w.player.hpMax,
        Color::rgb(0.35f, 0.08f, 0.10f), Color::rgb(0.25f, 0.80f, 0.30f));
    std::wstring hpText = std::to_wstring((int)std::max(0.0f, w.player.hp)) + L" / " +
                          std::to_wstring((int)w.player.hpMax);
    r.drawText(hpText, 20, 19, 260, 18, 14, Color::rgb(1, 1, 1), 1);

    // potion icon (frame 64 of the tinted atlas) + count
    render::Texture* tiles = ts.get(r, w.tilesPath);
    if (tiles && tiles->bmp()) {
        float sx = (float)((64 % 20) * 18), sy = (float)((64 / 20) * 18);
        r.drawBitmap(tiles->bmp(), 20, 48, 26, 26, sx, sy, 18, 18, 1.0f, false);
    }
    r.drawText(L"x " + std::to_wstring(w.player.potions), 52, 50, 80, 22, 16,
               Color::rgb(0.6f, 0.95f, 0.65f), 0);
    r.drawText(L"Q", 30, 78, 40, 18, 12, Color::rgb(0.7f, 0.7f, 0.75f), 0);

    if (w.player.weapon)
        r.drawText(S.get(w.player.weapon->nameKey), 20, 100, 300, 22, 16,
                   Color::rgb(0.85f, 0.85f, 0.9f), 0);

    // ---- top-right: coins / souls / level name ----
    r.drawText(S.get(L"hud_coins") + L"  " + std::to_wstring(w.coins),
               980, 16, 280, 24, 18, Color::rgb(0.98f, 0.85f, 0.3f), 2);
    r.drawText(S.get(L"hud_soul") + L"  " + std::to_wstring(w.soulRun),
               980, 44, 280, 24, 18, Color::rgb(0.45f, 0.8f, 1.0f), 2);
    std::wstring lvKey = L"lv_" + std::to_wstring(w.levelIndex);
    r.drawText(S.get(lvKey), 980, 72, 280, 22, 15, Color::rgb(0.7f, 0.7f, 0.78f), 2);

    // ---- boss bar (bottom center) ----
    if (Enemy* boss = findBoss(w)) {
        if (!boss->dead) {
            r.drawText(S.get(L"boss_name"), 340, 636, 600, 24, 18,
                       Color::rgb(0.95f, 0.6f, 0.5f), 1, true);
            bar(r, 340, 664, 600, 16, boss->hp / boss->hpMax,
                Color::rgb(0.25f, 0.05f, 0.08f), Color::rgb(0.85f, 0.2f, 0.25f));
        }
    }

    // ---- level banner ----
    if (w.bannerT > 0.0f) {
        float a = std::min(1.0f, w.bannerT);
        r.fillRect(0, 280, 1280, 120, Color::rgb(0.03f, 0.03f, 0.05f, 0.55f * a));
        r.drawText(S.get(lvKey), 0, 300, 1280, 60, 44,
                   Color::rgb(0.95f, 0.9f, 0.75f, a), 1, true);
        r.drawText(S.get(L"hud_enter_level"), 0, 366, 1280, 26, 16,
                   Color::rgb(0.7f, 0.7f, 0.75f, a), 1);
    }
}

} // namespace game
