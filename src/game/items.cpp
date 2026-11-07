// items.cpp - pickups (gravity + magnet), saws, racks, portals, interaction.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "items.hpp"
#include "world.hpp"
#include "tables.hpp"
#include "strings.hpp"
#include "../render/renderer.hpp"
#include "../render/texture.hpp"
#include "../audio/audio.hpp"
#include <cmath>
#include <algorithm>

namespace game {

float Saw::cx() const {
    // triangle ping-pong 0..1..0
    float ph = std::fmod(t / period, 2.0f);
    float k = ph < 1.0f ? ph : 2.0f - ph;
    return x0 + (x1 - x0) * k;
}
Rect Saw::rect() const { return {cx() - 24, y - 24, 48, 48}; }

void spawnPickup(World& w, int kind, float x, float y, int amount) {
    Pickup p;
    p.kind = kind; p.x = x; p.y = y - 14;
    float a = (float)(w.rng() % 628) / 100.0f;
    float sp = 60.0f + (float)(w.rng() % 90);
    p.vx = std::cos(a) * sp; p.vy = -140.0f - (float)(w.rng() % 80);
    p.amount = amount;
    w.pickups.push_back(p);
}

static void collectPickup(World& w, Pickup& p) {
    switch (p.kind) {
    case PK_COIN: {
        int n = (int)std::lround(p.amount * w.coinMult);
        w.coins += n;
        w.audio->playSfx(L"coin", 0.55f);
        w.fx.floatText(p.x, p.y - 14, L"+" + std::to_wstring(n), render::Color::rgb(0.98f, 0.85f, 0.3f));
        break; }
    case PK_SOUL:
        w.soulRun += p.amount;
        w.audio->playSfx(L"soul", 0.6f);
        w.fx.floatText(p.x, p.y - 14, L"+" + std::to_wstring(p.amount), render::Color::rgb(0.4f, 0.8f, 1.0f));
        break;
    case PK_POTION:
        w.player.potions += 1;
        w.audio->playSfx(L"potion", 0.7f);
        w.fx.floatText(p.x, p.y - 14, L"+1 " + w.strings->get(L"hud_potion"), render::Color::rgb(0.5f, 0.95f, 0.6f));
        break;
    }
    p.removeMe = true;
}

static void updatePickups(World& w, float dt) {
    const Player& pl = w.player;
    for (auto& p : w.pickups) {
        p.t += dt;
        // gravity with terminal velocity
        p.vy = std::min(p.vy + 1200.0f * dt, 500.0f);
        Rect r = {p.x - 9, p.y - 9, 18, 18};
        MoveResult mr = moveAabb(r, p.vx * dt, p.vy * dt, w.level.map, false);
        if (mr.hitLeft || mr.hitRight) p.vx = -p.vx * 0.4f;
        if (mr.onGround) { p.vy = 0; p.vx *= 0.90f; }
        p.x = r.x + 9; p.y = r.y + 9;
        // magnet after 0.35s
        if (p.t > 0.35f && pl.alive()) {
            float dx = pl.cx() - p.x, dy = pl.cy() - p.y;
            float d2 = dx * dx + dy * dy;
            float range = (p.kind == PK_POTION) ? 60.0f : 150.0f;
            if (d2 < range * range) {
                float d = std::sqrt(std::max(d2, 1.0f));
                float pull = 700.0f;
                p.vx = dx / d * pull; p.vy = dy / d * pull;
            }
            if (d2 < 30.0f * 30.0f) collectPickup(w, p);
        }
    }
    w.pickups.erase(std::remove_if(w.pickups.begin(), w.pickups.end(),
        [](const Pickup& p) { return p.removeMe; }), w.pickups.end());
}

static void updateSaws(World& w, float dt) {
    for (auto& s : w.saws) {
        s.t += dt;
        if (s.dmgCd > 0) s.dmgCd -= dt;
        if (s.dmgCd <= 0 && w.player.alive() && !w.player.invulnerable() &&
            rectsOverlap(s.rect(), w.player.rect())) {
            w.player.damage(w, 15.0f, s.cx());
            s.dmgCd = 0.5f;
        }
    }
}

void updateItems(World& w, float dt) {
    updatePickups(w, dt);
    updateSaws(w, dt);
}

// Draw one frame from a grid atlas (no gap) at dest rect.
static void drawFrame(render::Renderer& r, render::Texture* tex, int frame, int cols, int srcPx,
                      float dx, float dy, float dw, float dh, bool flipX = false) {
    if (!tex || !tex->bmp()) return;
    float sx = (float)((frame % cols) * srcPx);
    float sy = (float)((frame / cols) * srcPx);
    r.drawBitmap(tex->bmp(), dx, dy, dw, dh, sx, sy, (float)srcPx, (float)srcPx, 1.0f, flipX);
}

static void drawPickups(World& w, render::Renderer& r, render::TextureStore& ts) {
    render::Texture* tiles = ts.get(r, w.tilesPath);
    for (const auto& p : w.pickups) {
        float bob = std::sin(p.t * 6.0f) * 2.0f;
        switch (p.kind) {
        case PK_COIN: {
            // gold square with dark outline
            r.fillRect(p.x - 8, p.y - 8 + bob, 16, 16, render::Color::rgb(0.25f, 0.18f, 0.05f));
            r.fillRect(p.x - 6, p.y - 6 + bob, 12, 12, render::Color::rgb(0.98f, 0.8f, 0.25f));
            r.fillRect(p.x - 2, p.y - 4 + bob, 4, 8, render::Color::rgb(1.0f, 0.95f, 0.6f));
            break; }
        case PK_SOUL:
            // blue gem frame 66 from the tinted 20x9 18px tile atlas
            drawFrame(r, tiles, 66, 20, 18, p.x - 14, p.y - 14 + bob, 28, 28);
            break;
        case PK_POTION:
            // potion bottle frame 64
            drawFrame(r, tiles, 64, 20, 18, p.x - 13, p.y - 13 + bob, 26, 26);
            break;
        }
    }
}

static void drawSaws(World& w, render::Renderer& r, render::TextureStore& ts) {
    render::Texture* chars = ts.get(r, w.assetsRoot + L"/gfx/chars.png");
    for (const auto& s : w.saws) {
        float c = s.cx();
        // track line
        r.fillRect(std::min(s.x0, s.x1) - 2, s.y - 2, std::fabs(s.x1 - s.x0) + 4, 4,
                   render::Color::rgb(0.3f, 0.3f, 0.34f, 0.7f));
        drawFrame(r, chars, 8, 9, 24, c - 27, s.y - 27, 54, 54);
    }
}

// Floating "[E] ..." prompt when the player stands inside an interact zone.
// Without it there was no on-screen clue that E does anything.
static void drawHint(World& w, render::Renderer& r, const wchar_t* key,
                     float cx, float topY, const Rect& zone) {
    if (!w.player.alive() || !rectsOverlap(zone, w.player.rect())) return;
    float bob = std::sin(w.runTime * 5.0f) * 3.0f;
    r.drawText(w.strings->get(key), cx - 80, topY - 26 + bob, 160, 22, 15,
               render::Color::rgb(1.0f, 0.95f, 0.6f), 1, true);
}

static void drawRacks(World& w, render::Renderer& r, render::TextureStore&) {
    for (const auto& rk : w.racks) {
        render::Color base = rk.used ? render::Color::rgb(0.3f, 0.28f, 0.3f, 0.6f)
                                     : render::Color::rgb(0.45f, 0.32f, 0.2f);
        r.fillRect(rk.x - 22, rk.y - 66, 44, 66, render::Color::rgb(0.16f, 0.13f, 0.15f));
        r.fillRect(rk.x - 18, rk.y - 62, 36, 58, base);
        // sword silhouette
        render::Color blade = rk.used ? render::Color::rgb(0.35f, 0.35f, 0.38f, 0.5f)
                                      : render::Color::rgb(0.85f, 0.85f, 0.9f);
        r.fillRect(rk.x - 3, rk.y - 56, 6, 36, blade);
        r.fillRect(rk.x - 10, rk.y - 22, 20, 5, blade);
        r.fillRect(rk.x - 2, rk.y - 17, 4, 10, render::Color::rgb(0.4f, 0.28f, 0.16f));
        if (!rk.used)
            drawHint(w, r, L"hint_rack", rk.x, rk.y - 66, {rk.x - 60, rk.y - 90, 120, 100});
    }
}

static void drawPortals(World& w, render::Renderer& r, render::TextureStore&) {
    for (const auto& pt : w.portals) {
        float pulse = 0.5f + 0.5f * std::sin(w.runTime * 4.0f);
        float h = 92.0f + pulse * 8.0f;
        r.fillRect(pt.x - 26, pt.y - h, 52, h, render::Color::rgb(0.12f, 0.08f, 0.16f, 0.9f));
        r.fillRect(pt.x - 20, pt.y - h + 6, 40, h - 12,
                   render::Color::rgb(0.95f, 0.55f, 0.15f, 0.55f + 0.35f * pulse));
        r.fillRect(pt.x - 10, pt.y - h + 14, 20, h - 28,
                   render::Color::rgb(1.0f, 0.8f, 0.35f, 0.5f + 0.4f * pulse));
        if (pt.active)
            drawHint(w, r, L"hint_portal", pt.x, pt.y - h, {pt.x - 56, pt.y - 110, 112, 120});
    }
}

void drawItems(World& w, render::Renderer& r, render::TextureStore& ts) {
    drawPickups(w, r, ts);
    drawSaws(w, r, ts);
    drawRacks(w, r, ts);
    drawPortals(w, r, ts);
}

// ---------- interaction ----------

static const WeaponRow* rollOtherWeapon(World& w) {
    const auto& ws = w.tables->weapons();
    if (ws.size() < 2) return nullptr;
    for (int tries = 0; tries < 8; ++tries) {
        const WeaponRow& cand = ws[w.rng() % ws.size()];
        if (cand.id != w.player.weaponId) return &cand;
    }
    return nullptr;
}

bool tryInteract(World& w) {
    if (!w.player.alive()) return false;
    Rect pr = w.player.rect();
    // racks first
    for (auto& rk : w.racks) {
        if (rk.used) continue;
        Rect zone = {rk.x - 60, rk.y - 90, 120, 100};
        if (rectsOverlap(zone, pr)) {
            const WeaponRow* wr = rollOtherWeapon(w);
            if (!wr) return false;
            w.player.weapon = wr;
            w.player.weaponId = wr->id;
            rk.used = true;
            w.audio->playSfx(L"buy", 0.7f);
            w.fx.floatText(rk.x, rk.y - 84, w.strings->get(wr->nameKey), render::Color::rgb(0.9f, 0.9f, 1.0f));
            w.fx.burst(rk.x, rk.y - 40, render::Color::rgb(0.9f, 0.9f, 1.0f), 8, 150.0f);
            return true;
        }
    }
    for (auto& pt : w.portals) {
        if (!pt.active) continue;
        Rect zone = {pt.x - 56, pt.y - 110, 112, 120};
        if (rectsOverlap(zone, pr)) {
            w.pendingExit = true;
            w.audio->playSfx(L"portal", 0.8f);
            w.fx.burst(pt.x, pt.y - 48, render::Color::rgb(1.0f, 0.7f, 0.25f), 16, 240.0f);
            return true;
        }
    }
    return false;
}

} // namespace game
