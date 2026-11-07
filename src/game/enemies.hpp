// enemies.hpp - enemy entities (5 normal AI types + boss) and boss shockwaves.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "physics.hpp"
#include "../render/sprites.hpp"
#include <string>
#include <vector>

namespace render { class Renderer; class TextureStore; }

namespace game {

struct World;
struct EnemyRow;

enum AI : int { AI_CHASE = 0, AI_SWOOP, AI_FLOAT, AI_LEAP, AI_EXPLODE, AI_BOSS };

struct Enemy {
    std::wstring id;
    const EnemyRow* row = nullptr;
    int ai = AI_CHASE;

    float x = 0, y = 0, w = 48, h = 48;   // collision box
    float vx = 0, vy = 0;
    int facing = -1;
    float hp = 10, hpMax = 10;
    bool onGround = false;

    int state = 0;        // per-AI state machine
    float t = 0;          // generic state timer
    float atkCd = 0;      // generic attack cooldown
    float homeX = 0, homeY = 0;
    bool strikeDone = false; // current telegraphed strike already connected

    float flashT = 0;     // white flash after being hit
    float hurtT = 0;      // stagger
    float touchCd = 0;    // contact damage cooldown
    bool hitWall = false; // set by physics step
    int lastSwing = -1;   // last player swingId that hit us
    bool dead = false, removeMe = false;

    // boss
    int phase = 1;
    float chargeCd = 4.0f, slamCd = 3.0f, summonCd = 8.0f;
    bool charging = false;

    render::Animator anim;
    render::AnimClip clip;  // owned clip (built from row)

    Rect rect() const { return {x, y, w, h}; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

// Boss ground shockwave.
struct Wave {
    float x, y;      // top-left of the wave box (h = 40)
    float vx;
    float life = 3.0f;
    bool removeMe = false;
    Rect rect() const { return {x, y, 34, 44}; }
};

// Spawn one enemy of id at (cx, bottom). Returns false if id unknown.
bool spawnEnemy(World& w, const std::wstring& id, float cx, float bottom);

void updateEnemies(World& w, float dt);
void drawEnemies(World& w, render::Renderer& r, render::TextureStore& ts);

// Applies player melee swings to all enemies (called from app after player update).
void applyPlayerAttacks(World& w);

int aliveEnemyCount(const World& w);   // excludes boss
Enemy* findBoss(World& w);             // nullptr when none / dead

// Damage helpers shared by hazards.
void damageEnemy(World& w, Enemy& e, float dmg, float knock, float fromX, bool& outKilled);

} // namespace game
