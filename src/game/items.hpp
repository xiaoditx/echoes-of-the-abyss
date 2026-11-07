// items.hpp - pickups (coin/soul/potion), saw hazards, weapon racks, portals.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "physics.hpp"
#include <vector>

namespace render { class Renderer; class TextureStore; }

namespace game {

struct World;

enum PickupKind { PK_COIN = 0, PK_SOUL = 1, PK_POTION = 2 };

struct Pickup {
    int kind = PK_COIN;
    float x = 0, y = 0, vx = 0, vy = 0; // x,y = center
    int amount = 1;
    float t = 0;         // age (magnet delay)
    bool removeMe = false;
    Rect rect() const { return {x - 9, y - 9, 18, 18}; }
};

// Moving saw hazard between two points (horizontal).
struct Saw {
    float x0 = 0, x1 = 0, y = 0; // center positions
    float t = 0;                 // 0..1 phase
    float period = 2.4f;         // one-way seconds
    float dmgCd = 0;
    float cx() const;
    Rect rect() const;
};

// Weapon rack: interact (E) to swap weapon.
struct Rack {
    float x = 0, y = 0; // center-bottom
    bool used = false;
    Rect rect() const { return {x - 26, y - 70, 52, 70}; }
};

// Exit portal.
struct Portal {
    float x = 0, y = 0; // center-bottom
    bool active = true;
    Rect rect() const { return {x - 30, y - 96, 60, 96}; }
};

void spawnPickup(World& w, int kind, float x, float y, int amount);
void updateItems(World& w, float dt);
void drawItems(World& w, render::Renderer& r, render::TextureStore& ts);

// E-key interaction: racks first, then portals. Returns true if something was used.
bool tryInteract(World& w);

} // namespace game
