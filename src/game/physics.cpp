// physics.cpp - axis-separated AABB tile collision.
#include "physics.hpp"
#include "level.hpp"

namespace game {

static bool solidBlocking(uint8_t s) { return s == SSolid; }

MoveResult moveAabb(Rect& r, float dx, float dy, const TileMap& map, bool dropThrough) {
    MoveResult res;
    // --- X axis ---
    r.x += dx;
    if (dx > 0) {
        int tx = (int)((r.x + r.w) / kTilePx);
        int y0 = (int)(r.y / kTilePx), y1 = (int)((r.y + r.h - 0.001f) / kTilePx);
        for (int ty = y0; ty <= y1; ++ty)
            if (solidBlocking(map.solidAt(tx, ty))) {
                r.x = tx * kTilePx - r.w - 0.001f;
                res.hitRight = true;
                break;
            }
    } else if (dx < 0) {
        int tx = (int)(r.x / kTilePx);
        int y0 = (int)(r.y / kTilePx), y1 = (int)((r.y + r.h - 0.001f) / kTilePx);
        for (int ty = y0; ty <= y1; ++ty)
            if (solidBlocking(map.solidAt(tx, ty))) {
                r.x = (tx + 1) * kTilePx + 0.001f;
                res.hitLeft = true;
                break;
            }
    }
    // --- Y axis ---
    float prevBottom = r.y + r.h;
    r.y += dy;
    if (dy > 0) {
        int ty = (int)((r.y + r.h) / kTilePx);
        int x0 = (int)(r.x / kTilePx), x1 = (int)((r.x + r.w - 0.001f) / kTilePx);
        for (int tx = x0; tx <= x1; ++tx) {
            uint8_t s = map.solidAt(tx, ty);
            bool block = (s == SSolid);
            if (s == SPlatform && !dropThrough) {
                float platTop = ty * kTilePx;
                if (prevBottom <= platTop + 0.01f) { block = true; res.standingOnPlatform = true; }
            }
            if (block) {
                r.y = ty * kTilePx - r.h - 0.001f;
                res.onGround = true;
                break;
            }
        }
    } else if (dy < 0) {
        int ty = (int)(r.y / kTilePx);
        int x0 = (int)(r.x / kTilePx), x1 = (int)((r.x + r.w - 0.001f) / kTilePx);
        for (int tx = x0; tx <= x1; ++tx)
            if (solidBlocking(map.solidAt(tx, ty))) {
                r.y = (ty + 1) * kTilePx + 0.001f;
                res.hitHead = true;
                break;
            }
    }
    return res;
}

bool rectVsSolid(const Rect& r, const TileMap& map) {
    int x0 = (int)(r.x / kTilePx), x1 = (int)((r.x + r.w - 0.001f) / kTilePx);
    int y0 = (int)(r.y / kTilePx), y1 = (int)((r.y + r.h - 0.001f) / kTilePx);
    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
            if (map.solidAt(tx, ty) == SSolid) return true;
    return false;
}

} // namespace game
