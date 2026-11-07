// physics.hpp - AABB vs tile-grid movement. World units are logical pixels (tile = 54).
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

namespace game {

// World-space constants. Tiles are 18px source art drawn at x3.
inline constexpr float kTilePx = 54.0f;

struct Rect { float x = 0, y = 0, w = 0, h = 0; };
struct Vec2 { float x = 0, y = 0; };

class TileMap; // fwd (level.hpp)

struct MoveResult {
    bool hitLeft = false, hitRight = false, hitHead = false, onGround = false;
    bool standingOnPlatform = false;
};

// Moves rect by (dx,dy) with axis-separated tile collision.
// dropThrough: ignore one-way platforms this step (used for jump-down).
MoveResult moveAabb(Rect& r, float dx, float dy, const TileMap& map, bool dropThrough);

// Overlap test helpers.
bool rectVsSolid(const Rect& r, const TileMap& map);
inline bool rectsOverlap(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

} // namespace game
