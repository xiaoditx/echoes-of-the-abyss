// level.hpp - tile map, room templates, run-level generation.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "physics.hpp"
#include <string>
#include <vector>
#include <random>

namespace render { class Renderer; class TextureStore; }

namespace game {

// World-space constants. Tiles are 18px source art drawn at x3.

// Solidness codes stored in TileMap::solid.
enum Solid : uint8_t { SEmpty = 0, SSolid = 1, SPlatform = 2 };

class TileMap {
public:
    int w = 0, h = 0;                     // tiles
    std::vector<uint8_t> solid;           // SEmpty/SSolid/SPlatform
    std::vector<int> visual;              // atlas frame index, -1 = none (bg layer)
    std::vector<int> visualBg;            // decorative background frame, -1 = none

    void resize(int w_, int h_);
    uint8_t solidAt(int tx, int ty) const;   // out-of-bounds x => solid (walls), below map => empty
    int visualAt(int tx, int ty) const;
    void set(int tx, int ty, uint8_t s, int vis);

    float pixelW() const { return w * kTilePx; }
    float pixelH() const { return h * kTilePx; }
};

struct SpawnPoint {
    enum Kind { PlayerStart, Enemy, CoinLine, Potion, WeaponRack, Portal, Saw, BossArena } kind;
    float x = 0, y = 0;      // world px (center-bottom for entities)
    int roomIndex = 0;
    std::wstring arg;        // enemy id override (empty = roll from pool)
};

struct RoomInfo {
    int tileX0 = 0, tileW = 0; // span in the stitched map
    std::wstring tags;         // start / combat / platform / bonus / end / boss
};

struct GeneratedLevel {
    TileMap map;
    std::vector<SpawnPoint> spawns;
    std::vector<RoomInfo> rooms;
    int level = 1;
    Rect bossArena{0, 0, 0, 0};  // valid when boss room exists
    bool hasBoss = false;
};

// Loads room templates from <assetRoot>/rooms and stitches a full level.
// rng seeded per run. Returns false (Fatal-logged) if no templates found.
bool generateLevel(const std::wstring& assetRoot, int levelIndex,
                   int roomsMin, int roomsMax, std::mt19937& rng, GeneratedLevel& out);

// Draws visible tiles from the given atlas (20x9 grid, 18px, 1px gap), camera-culled.
void drawTiles(render::Renderer& r, render::TextureStore& store, const TileMap& map,
               const std::wstring& atlasPath, float camX, float camY, float tintMul = 1.0f);

} // namespace game
