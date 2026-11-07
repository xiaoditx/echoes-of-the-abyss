// world.hpp - shared per-run gameplay context passed to all entity updates.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "level.hpp"
#include "fx.hpp"
#include "player.hpp"
#include <random>
#include <vector>
#include <deque>
#include <string>

namespace audio { class Audio; }
namespace render { class TextureStore; }

namespace game {

class Tables;
class Strings;

struct Enemy;   // enemies.hpp
struct Wave;
struct Pickup;  // items.hpp
struct Saw;
struct Rack;
struct Portal;

struct World {
    // services (owned by App)
    const Tables* tables = nullptr;
    const Strings* strings = nullptr;
    audio::Audio* audio = nullptr;
    render::TextureStore* tex = nullptr;
    std::wstring assetsRoot;   // absolute path to the assets/ folder

    // level & entities
    GeneratedLevel level;
    Fx fx;
    std::mt19937 rng;
    Player player;
    // deque: Enemy animators hold pointers into their own clip member,
    // so element addresses must stay stable across spawn/erase.
    std::deque<Enemy> enemies;
    std::vector<Wave> waves;
    std::vector<Pickup> pickups;
    std::vector<Saw> saws;
    std::vector<Rack> racks;
    std::vector<Portal> portals;

    // camera
    float camX = 0, camY = 0;

#ifdef EOTA_DEBUG
    // Cheat toggles (debug builds only): see App::handleCheats for the keys.
    bool cheatGod = false;      // ignore all player damage
    bool cheatOneHit = false;   // every player hit kills instantly
#endif

    // run state
    int levelIndex = 1;
    int coins = 0, soulRun = 0, kills = 0;
    float runTime = 0;
    float coinMult = 1.0f;
    bool pitRespawn = false;       // set by player when falling into a pit
    bool pendingExit = false;      // portal used
    bool bossActive = false, bossDefeated = false;
    int bossDoorTileX = -1;        // tile column of the boss door (solid while active)

    // per-level presentation
    float bannerT = 0;             // >0: show level name banner
    std::wstring tilesPath;        // tinted tile atlas for this level
};

} // namespace game
