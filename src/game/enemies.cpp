// enemies.cpp
#include "enemies.hpp"
#include "world.hpp"
#include "tables.hpp"
#include "items.hpp"
#include "strings.hpp"
#include "level.hpp"
#include "../audio/audio.hpp"
#include "../core/logger.hpp"
#include "../core/paths.hpp"
#include "../render/renderer.hpp"
#include "../render/texture.hpp"

#include <algorithm>

#include <cmath>

namespace game {

namespace {
constexpr float kTouchCd = 0.6f;
constexpr float kChaseWindup = 0.45f;   // visible telegraph before the strike
constexpr float kChaseStrike = 0.22f;   // active hit window
constexpr float kChaseRecover = 0.55f;
constexpr float kSwoopWindup = 0.45f;   // bat hover before the dive
constexpr float kBossWindup = 0.5f;     // boss melee windup (matches updBoss)
constexpr float kBossChargeWindup = 0.6f;

AI aiFromString(const std::wstring& s) {
    if (s == L"chase") return AI_CHASE;
    if (s == L"swoop") return AI_SWOOP;
    if (s == L"float") return AI_FLOAT;
    if (s == L"leap") return AI_LEAP;
    if (s == L"explode") return AI_EXPLODE;
    return AI_BOSS;
}

float randRange(World& w, float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(w.rng);
}

// Melee strike box in front of a chaser; shared by update and telegraph drawing.
Rect chaseStrikeBox(const Enemy& e) {
    constexpr float reach = 62.0f, hgt = 50.0f;
    const float y = e.y + (e.h - hgt) * 0.5f;
    const float x = e.facing > 0 ? e.x + e.w - 4.0f : e.x + 4.0f - reach;
    return {x, y, reach, hgt};
}

// Boss melee box, shared by the hit test and its telegraph.
Rect bossStrikeBox(const Enemy& e) {
    return {e.facing > 0 ? e.x + e.w - 10.0f : e.x - 140.0f, e.y + 10.0f, 150.0f, 80.0f};
}

// Top Y of a standable floor row (solid with open air above), -1 if none.
float standableTop(const TileMap& map, int tx, int clear = 4) {
    if (tx < 0 || tx >= map.w) return -1;
    for (int ty = 0; ty < map.h; ++ty) {
        if (map.solidAt(tx, ty) != SSolid) continue;
        bool ok = true;
        for (int k = 1; k <= clear; ++k) {
            if (ty - k < 0) break;
            if (map.solidAt(tx, ty - k) == SSolid) { ok = false; break; }
        }
        if (ok) return ty * kTilePx;
    }
    return -1;
}

// Summoned enemies must appear inside the arena on real floor; otherwise they
// spawn inside the door wall or beyond the fight area.
void spawnSummon(World& w, float nearX) {
    const Rect& ar = w.level.bossArena;
    float lo = ar.x + 3.0f * kTilePx;
    float hi = ar.x + ar.w - 3.0f * kTilePx;
    if (hi < lo) { float mid = ar.x + ar.w * 0.5f; lo = hi = mid; }
    float x = nearX < lo ? lo : (nearX > hi ? hi : nearX);
    int col = (int)(x / kTilePx);
    for (int d = 0; d <= 10; ++d) {
        for (int side = 0; side < 2; ++side) {
            int c = col + (side == 0 ? d : -d);
            float gy = standableTop(w.level.map, c);
            if (gy > 0) {
                spawnEnemy(w, L"slime", (c + 0.5f) * kTilePx, gy);
                return;
            }
        }
    }
}
} // namespace

bool spawnEnemy(World& w, const std::wstring& id, float cx, float bottom) {
    const EnemyRow* row = w.tables->enemy(id);
    if (!row) { LOG_WARN(L"spawnEnemy: unknown id %s", id.c_str()); return false; }
    Enemy e;
    e.id = id; e.row = row;
    e.ai = aiFromString(row->ai);
    e.hp = e.hpMax = row->hp;
    const float draw = 24.0f * (float)row->scale;
    if (e.ai == AI_BOSS) { e.w = 84; e.h = 92; }
    else if (e.ai == AI_LEAP) { e.w = 52; e.h = 62; }
    else { e.w = 42; e.h = 40; }
    e.x = cx - e.w * 0.5f;
    e.y = bottom - e.h;
    e.homeX = cx; e.homeY = bottom;
    e.facing = -1;
    e.clip.texturePath = core::assetPath(L"gfx\\chars.png");
    e.clip.frameW = 24; e.clip.frameH = 24;
    e.clip.startIndex = row->animStart;
    e.clip.frameCount = row->animCount;
    e.clip.fps = 6.0f;
    e.clip.loop = true;
    (void)draw;
    w.enemies.push_back(std::move(e));
    // Point the animator at the *final* clip location (deque keeps it stable).
    Enemy& placed = w.enemies.back();
    placed.anim.play(&placed.clip, true);
    return true;
}

void damageEnemy(World& w, Enemy& e, float dmg, float knock, float fromX, bool& outKilled) {
    outKilled = false;
    if (e.dead) return;
    e.hp -= dmg;
    e.flashT = 0.08f;
    e.hurtT = (e.ai == AI_BOSS) ? 0.05f : 0.12f;
    if (e.ai != AI_BOSS) {
        e.vx = (e.cx() < fromX ? -1.0f : 1.0f) * knock;
    }
    w.fx.floatText(e.cx(), e.y - 6, std::to_wstring((int)(dmg + 0.5f)),
                   render::Color::rgb(1, 0.85f, 0.3f), 18);
    w.fx.burst(e.cx(), e.cy(), render::Color::rgb(1, 0.5f, 0.3f), 6);
    // visible slash mark on every landed player hit
    w.fx.slash(e.cx() - 14.0f, e.cy() - 6.0f, 28, 12,
               render::Color::rgb(1, 0.95f, 0.7f), e.cx() >= fromX ? 1.0f : -1.0f);
    if (e.hp <= 0) {
        e.dead = true;
        outKilled = true;
        ++w.kills;
        w.audio->playSfx(L"kill");
        w.fx.burst(e.cx(), e.cy(), render::Color::rgb(0.9f, 0.2f, 0.3f), 14, 260);
        // loot
        if (const LootRow* lo = w.tables->loot(e.id)) {
            int n = lo->coinMin + (int)(w.rng() % (unsigned)std::max(1, lo->coinMax - lo->coinMin + 1));
            n = (int)(n * w.coinMult + 0.5f);
            for (int i = 0; i < n; ++i) spawnPickup(w, PK_COIN, e.cx(), e.cy(), 1);
            std::uniform_real_distribution<float> ch(0.0f, 1.0f);
            if (ch(w.rng) < lo->soulChance)
                for (int i = 0; i < lo->soulN; ++i) spawnPickup(w, PK_SOUL, e.cx(), e.cy(), 1);
            if (ch(w.rng) < lo->potionChance) spawnPickup(w, PK_POTION, e.cx(), e.cy(), 1);
        }
        if (e.ai == AI_FLOAT) {
            // death burst damages player if close
            if (std::fabs(w.player.cx() - e.cx()) < 44 && std::fabs(w.player.cy() - e.cy()) < 44)
                w.player.damage(w, 8.0f, e.cx());
            w.fx.burst(e.cx(), e.cy(), render::Color::rgb(0.7f, 0.4f, 1), 12, 200);
        }
        if (e.ai == AI_BOSS) {
            w.bossDefeated = true;
            w.fx.shake(14, 0.6f);
            w.audio->playSfx(L"bossdie");
        }
    } else {
        w.audio->playSfx(L"hit");
    }
}

void applyPlayerAttacks(World& w) {
    Player& p = w.player;
    if (!p.attackHitActive()) return;
    Rect box = p.attackBox();
    std::uniform_real_distribution<float> var(0.9f, 1.1f);
    for (auto& e : w.enemies) {
        if (e.dead || e.lastSwing == p.swingId()) continue;
        if (!rectsOverlap(box, e.rect())) continue;
        e.lastSwing = p.swingId();
        bool killed = false;
#ifdef EOTA_DEBUG
        if (w.cheatOneHit) {
            damageEnemy(w, e, e.hp + 1.0f, p.swingKnock(), p.cx(), killed);
        } else
#endif
        damageEnemy(w, e, p.swingDamage() * var(w.rng), p.swingKnock(), p.cx(), killed);
        w.fx.hitstop(0.06f);
        // heavy finisher interrupts non-boss actions
        if (!killed && e.ai != AI_BOSS && p.swingKnock() >= 300.0f) { e.state = 0; e.t = 0; }
    }
}

int aliveEnemyCount(const World& w) {
    int n = 0;
    for (auto& e : w.enemies) if (!e.dead && e.ai != AI_BOSS) ++n;
    return n;
}

Enemy* findBoss(World& w) {
    for (auto& e : w.enemies) if (e.ai == AI_BOSS && !e.dead) return &e;
    return nullptr;
}

// ---------------- per-AI updates ----------------

static void physicsStep(World& w, Enemy& e, float dt, bool gravity) {
    if (gravity) {
        e.vy += 1600.0f * dt;
        if (e.vy > 900) e.vy = 900;
    }
    Rect r = e.rect();
    MoveResult mr = moveAabb(r, e.vx * dt, e.vy * dt, w.level.map, false);
    e.x = r.x; e.y = r.y;
    e.onGround = mr.onGround;
    if (mr.onGround && e.vy > 0) e.vy = 0;
    if (mr.hitHead && e.vy < 0) e.vy = 0;
    if ((mr.hitLeft || mr.hitRight) && e.ai != AI_BOSS) e.vx = 0;
    if (mr.hitLeft || mr.hitRight) e.hitWall = true; else e.hitWall = false;
}

static void touchDamage(World& w, Enemy& e) {
    if (e.touchCd > 0) return;
    if (e.dead) return;
    // Melee chasers deal damage only through their telegraphed strike below;
    // body contact must not force a trade against a blinking target.
    if (e.ai == AI_CHASE) return;
    if (rectsOverlap(e.rect(), w.player.rect())) {
        // Every contact hit leaves a visible mark: wraiths/bats used to drain HP
        // with no trace at all, which read as "damage out of nowhere".
        w.player.damage(w, e.row->dmg, e.cx());
        render::Color c = (e.ai == AI_FLOAT) ? render::Color::rgb(0.75f, 0.5f, 1.0f)
                                             : render::Color::rgb(1, 0.85f, 0.45f);
        w.fx.burst(w.player.cx(), w.player.cy(), c, 10, 220);
        w.fx.slash(e.cx() - 12.0f, e.cy() - 6.0f, 24, 12, c,
                   e.cx() >= w.player.cx() ? 1.0f : -1.0f);
        e.touchCd = kTouchCd;
    }
}

static void updChase(World& w, Enemy& e, float dt) {
    Player& p = w.player;
    const float dx = p.cx() - e.cx();
    const float adx = std::fabs(dx);
    switch (e.state) {
        case 0: { // patrol / approach
            if (e.t <= 0) { e.t = randRange(w, 1.2f, 2.4f); e.facing = (w.rng() & 1) ? 1 : -1; }
            e.t -= dt;
            // stay near home
            if (e.homeX - e.cx() > 140) e.facing = 1;
            if (e.cx() - e.homeX > 140) e.facing = -1;
            if (e.hurtT <= 0) e.vx = e.facing * e.row->speed * 0.5f;
            if (adx < 150 && std::fabs(p.cy() - e.cy()) < 110 && p.alive()) {
                e.state = 1; e.t = kChaseWindup; e.vx = 0; e.strikeDone = false;
                e.facing = dx > 0 ? 1 : -1;
            }
            break;
        }
        case 1: // windup: hold still. Facing stays locked to where the player
                // was when the windup began (set on the state 0 -> 1 transition);
                // re-aiming now would punish the player for rolling through.
            e.t -= dt;
            e.vx = 0;
            if (e.t <= 0) {
                e.state = 2; e.t = kChaseStrike; e.strikeDone = false;
                w.audio->playSfx(L"swing", 0.5f);
                Rect ab = chaseStrikeBox(e);
                w.fx.slash(ab.x, ab.y + ab.h * 0.35f, ab.w, ab.h * 0.3f,
                           render::Color::rgb(1, 0.75f, 0.45f), e.facing > 0 ? 1.0f : -1.0f);
            }
            break;
        case 2: // strike: active hitbox in front, connects at most once
            e.t -= dt;
            e.vx = e.facing * e.row->speed * 0.6f;
            if (!e.strikeDone && rectsOverlap(chaseStrikeBox(e), p.rect())) {
                p.damage(w, e.row->dmg, e.cx());
                w.fx.burst(p.cx(), p.cy(), render::Color::rgb(1, 0.7f, 0.35f), 8, 200);
                e.strikeDone = true;
            }
            if (e.t <= 0) { e.state = 3; e.t = kChaseRecover; e.vx = 0; }
            break;
        case 3: // recover: punish window
            e.t -= dt;
            e.vx = 0;
            if (e.t <= 0) { e.state = 0; e.t = randRange(w, 0.5f, 1.1f); }
            break;
    }
}

static void updSwoop(World& w, Enemy& e, float dt) {
    Player& p = w.player;
    switch (e.state) {
        case 0: { // hang, gentle bob
            e.vx = e.vy = 0;
            e.y = e.homeY - e.h + std::sin((float)(w.runTime * 2.0) + e.homeX) * 4.0f;
            if (p.alive() && std::fabs(p.cx() - e.cx()) < 200 && p.cy() > e.cy()) {
                // telegraphed hover before the dive
                e.state = 4; e.t = kSwoopWindup;
                e.facing = p.cx() > e.cx() ? 1 : -1;
            }
            break;
        }
        case 4: // windup: hover in place, direction locked
            e.t -= dt;
            e.vx = e.vy = 0;
            if (e.t <= 0) {
                e.state = 1; e.t = 1.2f;
                e.vx = e.facing * e.row->speed * 1.1f;
                e.vy = e.row->speed * 1.5f;
                w.audio->playSfx(L"swing", 0.45f);
                w.fx.burst(e.cx(), e.cy(), render::Color::rgb(0.6f, 0.5f, 1.0f), 8, 160);
            }
            break;
        case 1: // diving
            e.t -= dt;
            if (e.t <= 0 || e.hitWall || e.onGround) { e.state = 2; }
            break;
        case 2: { // return home
            float dx = e.homeX - e.cx(), dy = (e.homeY - e.h) - e.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 12) { e.state = 0; e.vx = e.vy = 0; break; }
            float spd = e.row->speed * 0.8f;
            e.vx = dx / len * spd; e.vy = dy / len * spd;
            break;
        }
    }
}

static void updFloat(World& w, Enemy& e, float dt) {
    Player& p = w.player;
    if (!p.alive()) { e.vx = e.vy = 0; return; }
    float dx = p.cx() - e.cx(), dy = p.cy() - e.cy();
    float len = std::sqrt(dx * dx + dy * dy) + 0.01f;
    if (e.hurtT <= 0) {
        e.vx = dx / len * e.row->speed;
        e.vy = dy / len * e.row->speed;
        e.facing = dx > 0 ? 1 : -1;
    }
}

static void updLeap(World& w, Enemy& e, float dt) {
    Player& p = w.player;
    const float dx = p.cx() - e.cx();
    switch (e.state) {
        case 0: // approach
            if (e.hurtT <= 0) e.vx = (dx > 0 ? 1 : -1) * e.row->speed;
            e.facing = dx > 0 ? 1 : -1;
            if (std::fabs(dx) < 95 && e.onGround && p.alive()) { e.state = 1; e.t = 0.5f; e.vx = 0; }
            break;
        case 1: // windup: leap direction locked to facing at windup start
            e.t -= dt; e.vx = 0;
            if (e.t <= 0) {
                e.state = 2;
                e.vy = -560;
                e.vx = e.facing * 260.0f;
            }
            break;
        case 2: // airborne
            if (e.onGround) {
                e.state = 3; e.t = 0.6f; e.vx = 0;
                w.fx.shake(5, 0.2f);
                w.audio->playSfx(L"slam", 0.5f);
                // landing shock: small aoe
                if (std::fabs(p.cx() - e.cx()) < 80 && std::fabs(p.bottom() - (e.y + e.h)) < 60)
                    p.damage(w, e.row->dmg, e.cx());
            }
            break;
        case 3:
            e.t -= dt;
            if (e.t <= 0) e.state = 0;
            break;
    }
}

static void updExplode(World& w, Enemy& e, float dt) {
    Player& p = w.player;
    const float dx = p.cx() - e.cx();
    switch (e.state) {
        case 0: // hop toward player
            if (e.onGround) {
                e.t -= dt;
                if (e.t <= 0 && p.alive()) {
                    e.t = 0.7f;
                    e.vy = -380;
                    e.vx = (dx > 0 ? 1 : -1) * e.row->speed;
                    e.facing = dx > 0 ? 1 : -1;
                }
            }
            if (std::fabs(dx) < 62 && std::fabs(p.cy() - e.cy()) < 70 && p.alive()) { e.state = 1; e.t = 0.8f; }
            break;
        case 1: // fuse
            e.t -= dt;
            e.vx = 0;
            if (e.t <= 0) {
                e.dead = true; e.removeMe = false; // counted as kill-less death
                w.fx.shake(8, 0.3f);
                w.fx.burst(e.cx(), e.cy(), render::Color::rgb(1, 0.6f, 0.1f), 22, 320);
                w.audio->playSfx(L"boom");
                float pdx = p.cx() - e.cx(), pdy = p.cy() - e.cy();
                if (pdx * pdx + pdy * pdy < 70.0f * 70.0f) p.damage(w, e.row->dmg, e.cx());
            }
            break;
    }
}

static void updBoss(World& w, Enemy& e, float dt) {
    Player& p = w.player;
    const float dx = p.cx() - e.cx();
    const float adx = std::fabs(dx);
    const float frac = e.hp / e.hpMax;
    e.phase = frac > 0.66f ? 1 : (frac > 0.33f ? 2 : 3);

    if (e.chargeCd > 0) e.chargeCd -= dt;
    if (e.atkCd > 0) e.atkCd -= dt;
    if (e.slamCd > 0) e.slamCd -= dt;
    if (e.summonCd > 0) e.summonCd -= dt;

    switch (e.state) {
        case 0: { // walk toward player
            if (e.hurtT <= 0) e.vx = (dx > 0 ? 1 : -1) * e.row->speed * 0.55f;
            e.facing = dx > 0 ? 1 : -1;
            if (!p.alive()) { e.vx = 0; break; }
            if (adx < 150 && e.atkCd <= 0) { e.state = 1; e.t = kBossWindup; e.vx = 0; }
            else if (e.chargeCd <= 0 && adx > 260) { e.state = 2; e.t = kBossChargeWindup; e.vx = 0; }
            else if (e.phase >= 2 && e.slamCd <= 0 && e.onGround && adx < 420) {
                e.state = 5; e.vy = -760; e.vx = (dx > 0 ? 1 : -1) * 120.0f;
                w.audio->playSfx(L"jump", 0.8f);
            } else if (e.phase >= 2 && e.summonCd <= 0 && aliveEnemyCount(w) < 3) {
                e.summonCd = 10.0f;
                spawnSummon(w, e.cx() - 110.0f);
                spawnSummon(w, e.cx() + 110.0f);
                w.fx.floatText(e.cx(), e.y - 20, L"!", render::Color::rgb(1, 0.3f, 0.3f), 30);
            }
            break;
        }
        case 1: // melee windup
            e.t -= dt;
            if (e.t <= 0) {
                e.state = 0; e.atkCd = 2.0f;
                w.audio->playSfx(L"swing", 0.9f);
                Rect hit = bossStrikeBox(e);
                w.fx.slash(hit.x, hit.y + hit.h * 0.3f, hit.w, hit.h * 0.4f,
                           render::Color::rgb(1, 0.9f, 0.7f), e.facing > 0 ? 1.0f : -1.0f);
                if (rectsOverlap(hit, p.rect())) {
                    p.damage(w, e.row->dmg, e.cx());
                    w.fx.burst(p.cx(), p.cy(), render::Color::rgb(1, 0.6f, 0.3f), 10, 220);
                }
            }
            break;
        case 2: // charge windup (flash handled in draw)
            e.t -= dt;
            if (e.t <= 0) {
                e.state = 3; e.charging = true;
                e.vx = e.facing * 640.0f;
                w.audio->playSfx(L"charge", 0.8f);
            }
            break;
        case 3: // charging
            if (e.hitWall) {
                e.state = 4; e.t = 1.0f; e.vx = 0; e.charging = false;
                e.chargeCd = e.phase == 3 ? 3.6f : 6.0f;
                w.fx.shake(10, 0.35f);
                w.audio->playSfx(L"slam");
            }
            // contact damage while charging
            if (rectsOverlap(e.rect(), p.rect())) {
                p.damage(w, e.row->dmg + 4, e.cx());
                w.fx.burst(p.cx(), p.cy(), render::Color::rgb(1, 0.7f, 0.3f), 10, 240);
            }
            break;
        case 4: // stunned (punish window)
            e.t -= dt;
            if (e.t <= 0) e.state = 0;
            break;
        case 5: // slam jump
            if (e.onGround && e.vy >= 0) {
                e.state = 0;
                e.slamCd = e.phase == 3 ? 4.0f : 7.0f;
                w.fx.shake(12, 0.4f);
                w.audio->playSfx(L"slam");
                // two ground waves
                Wave wl, wr;
                wl.x = e.x - 30; wl.y = e.y + e.h - 44; wl.vx = -340;
                wr.x = e.x + e.w;  wr.y = e.y + e.h - 44; wr.vx = 340;
                w.waves.push_back(wl); w.waves.push_back(wr);
                if (e.phase == 3) { // second pair, slightly delayed positions
                    wl.x -= 60; wr.x += 60; wl.vx = -300; wr.vx = 300;
                    w.waves.push_back(wl); w.waves.push_back(wr);
                }
                w.fx.burst(e.cx(), e.y + e.h, render::Color::rgb(0.8f, 0.6f, 0.3f), 18, 300);
            }
            break;
    }
}

void updateEnemies(World& w, float dt) {
    // Index loop, not range-for: boss summons push_back into w.enemies mid-loop,
    // and deque::push_back invalidates iterators. Element references stay valid.
    for (size_t i = 0; i < w.enemies.size(); ++i) {
        Enemy& e = w.enemies[i];
        if (e.removeMe) continue;
        if (e.flashT > 0) e.flashT -= dt;
        if (e.hurtT > 0) e.hurtT -= dt;
        if (e.touchCd > 0) e.touchCd -= dt;
        if (e.dead) {
            // brief death flash then removal
            e.t += dt;
            if (e.t > 0.25f) e.removeMe = true;
            continue;
        }
        const bool gravity = !(e.ai == AI_SWOOP || e.ai == AI_FLOAT);
        switch (e.ai) {
            case AI_CHASE: updChase(w, e, dt); break;
            case AI_SWOOP: updSwoop(w, e, dt); break;
            case AI_FLOAT: updFloat(w, e, dt); break;
            case AI_LEAP: updLeap(w, e, dt); break;
            case AI_EXPLODE: updExplode(w, e, dt); break;
            case AI_BOSS: updBoss(w, e, dt); break;
        }
        physicsStep(w, e, dt, gravity);
        touchDamage(w, e);
        e.anim.update(dt);
        if (e.y > w.level.map.pixelH() + 200) { e.dead = true; e.removeMe = true; }
    }
    // waves
    for (auto& v : w.waves) {
        v.x += v.vx * dt;
        v.life -= dt;
        if (v.life <= 0) v.removeMe = true;
        Rect r = v.rect();
        if (rectVsSolid(r, w.level.map)) v.removeMe = true;
        if (!v.removeMe && rectsOverlap(r, w.player.rect())) {
            w.player.damage(w, 20.0f, v.x + 17);
            w.fx.burst(w.player.cx(), w.player.cy(), render::Color::rgb(1, 0.85f, 0.4f), 12, 240);
            w.fx.slash(v.x, v.y + 12, 34, 20, render::Color::rgb(1, 0.9f, 0.5f),
                       v.vx > 0 ? 1.0f : -1.0f);
            v.removeMe = true;
        }
    }
    // compact
    auto& es = w.enemies;
    es.erase(std::remove_if(es.begin(), es.end(), [](const Enemy& e) { return e.removeMe; }), es.end());
    // remove_if move-assigns survivors; each animator would still point at the
    // original (possibly erased) slot. Re-point them at their own clip.
    for (auto& e : es) e.anim.retarget(&e.clip);
    auto& ws = w.waves;
    ws.erase(std::remove_if(ws.begin(), ws.end(), [](const Wave& v) { return v.removeMe; }), ws.end());
}

void drawEnemies(World& w, render::Renderer& r, render::TextureStore& ts) {
    for (auto& e : w.enemies) {
        if (e.removeMe) continue;
        const float draw = 24.0f * (float)e.row->scale;
        float dx = e.cx() - draw * 0.5f;
        float dy = e.y + e.h - draw;
        float opacity = e.dead ? 0.5f : 1.0f;
        // explode fuse blink
        if (e.ai == AI_EXPLODE && e.state == 1) {
            if (((int)(e.t * 12) & 1) == 0) opacity = 0.35f;
        }
        e.anim.draw(r, ts, dx, dy, draw, draw, e.facing > 0, opacity);
        // hit flash
        if (e.flashT > 0)
            r.fillRect(e.x - 2, e.y - 2, e.w + 4, e.h + 4, render::Color::rgb(1, 1, 1, 0.65f));
        // boss phase-3 red aura & windup glow
        if (e.ai == AI_BOSS) {
            if (e.phase == 3)
                r.drawRect(e.x - 4, e.y - 4, e.w + 8, e.h + 8, render::Color::rgb(1, 0.15f, 0.1f, 0.8f), 3.0f);
            if (e.state == 2)
                r.fillRect(e.x - 3, e.y - 4, e.w + 8, 6, render::Color::rgb(1, 0.8f, 0.1f, 0.9f));
            if (e.state == 4)
                r.drawRect(e.x - 6, e.y - 6, e.w + 12, e.h + 12, render::Color::rgb(0.4f, 0.9f, 1, 0.9f), 2.0f);
        }
        // chase windup: show where the incoming strike will land, filling up as
        // the timer runs out, plus an alert mark overhead.
        if (e.ai == AI_CHASE && e.state == 1) {
            const float k = 1.0f - std::max(0.0f, e.t) / kChaseWindup;
            Rect ab = chaseStrikeBox(e);
            r.fillRect(ab.x, ab.y, ab.w, ab.h, render::Color::rgb(1, 0.42f, 0.2f, 0.10f + 0.20f * k));
            r.drawRect(ab.x, ab.y, ab.w, ab.h, render::Color::rgb(1, 0.55f, 0.25f, 0.35f + 0.55f * k), 1.5f);
            r.fillRect(e.cx() - 2, e.y - 16, 4, 11, render::Color::rgb(1, 0.35f, 0.2f, 0.85f));
        }
        // leap windup: flashing outline so the pounce is readable.
        if (e.ai == AI_LEAP && e.state == 1) {
            r.drawRect(e.x - 4, e.y - 4, e.w + 8, e.h + 8, render::Color::rgb(1, 0.6f, 0.2f, 0.7f), 2.0f);
            if (((int)(e.t * 16) & 1) == 0)
                r.fillRect(e.x, e.y, e.w, e.h, render::Color::rgb(1, 0.5f, 0.2f, 0.16f));
        }
        // bat dive windup: hover warning + locked dive direction, so the swoop
        // is no longer an invisible instant hit.
        if (e.ai == AI_SWOOP && e.state == 4) {
            const float k = 1.0f - std::max(0.0f, e.t) / kSwoopWindup;
            r.drawRect(e.x - 6, e.y - 6, e.w + 12, e.h + 12,
                       render::Color::rgb(1, 0.5f, 0.25f, 0.4f + 0.5f * k), 2.0f);
            if (((int)(e.t * 18) & 1) == 0)
                r.fillRect(e.x, e.y, e.w, e.h, render::Color::rgb(1, 0.45f, 0.2f, 0.22f));
            r.fillRect(e.cx() - 2, e.y - 15, 4, 10, render::Color::rgb(1, 0.35f, 0.2f, 0.85f));
            // dive path preview: a dim band along the locked direction
            float bx = e.facing > 0 ? e.cx() + 10 : e.cx() - 10 - 90;
            r.fillRect(bx, e.cy() - 8, 90, 16, render::Color::rgb(1, 0.4f, 0.2f, 0.10f + 0.18f * k));
        }
        // wraith: contact damage is a melee attack, so show a pulsing aura when
        // it is close enough to hurt instead of draining HP out of nowhere.
        if (e.ai == AI_FLOAT && w.player.alive()) {
            float ddx = e.cx() - w.player.cx(), ddy = e.cy() - w.player.cy();
            if (ddx * ddx + ddy * ddy < 95.0f * 95.0f) {
                float pulse = 0.5f + 0.5f * std::sin(w.runTime * 9.0f);
                r.drawRect(e.x - 4, e.y - 4, e.w + 8, e.h + 8,
                           render::Color::rgb(0.8f, 0.5f, 1.0f, 0.35f + 0.4f * pulse), 2.0f);
            }
        }
        // boss windups: melee strike box, charge lane, and slam landing zone.
        if (e.ai == AI_BOSS) {
            if (e.state == 1) {
                const float k = 1.0f - std::max(0.0f, e.t) / kBossWindup;
                Rect ab = bossStrikeBox(e);
                r.fillRect(ab.x, ab.y, ab.w, ab.h,
                           render::Color::rgb(1, 0.32f, 0.12f, 0.08f + 0.20f * k));
                r.drawRect(ab.x, ab.y, ab.w, ab.h,
                           render::Color::rgb(1, 0.4f, 0.18f, 0.4f + 0.5f * k), 2.0f);
                r.fillRect(e.cx() - 3, e.y - 24, 6, 16, render::Color::rgb(1, 0.3f, 0.15f, 0.9f));
            } else if (e.state == 2) {
                const float k = 1.0f - std::max(0.0f, e.t) / kBossChargeWindup;
                float laneX = e.facing > 0 ? e.x + e.w : e.x - 360.0f;
                r.fillRect(laneX, e.y + e.h - 34, 360, 34,
                           render::Color::rgb(1, 0.2f, 0.12f, 0.06f + 0.18f * k));
                r.drawRect(laneX, e.y + e.h - 34, 360, 34,
                           render::Color::rgb(1, 0.3f, 0.15f, 0.35f + 0.55f * k), 2.0f);
                r.fillRect(e.cx() - 3, e.y - 24, 6, 16, render::Color::rgb(1, 0.3f, 0.15f, 0.9f));
            } else if (e.state == 5) {
                int col = (int)(e.cx() / kTilePx);
                float gy = standableTop(w.level.map, col);
                if (gy > 0) {
                    float pulse = 0.5f + 0.5f * std::sin(w.runTime * 12.0f);
                    r.fillRect(e.cx() - 92, gy - 10, 184, 10,
                               render::Color::rgb(1, 0.55f, 0.2f, 0.25f + 0.35f * pulse));
                    r.drawRect(e.cx() - 92, gy - 10, 184, 10,
                               render::Color::rgb(1, 0.7f, 0.3f, 0.6f + 0.35f * pulse), 2.0f);
                }
            }
        }
    }
    for (auto& v : w.waves) {
        r.fillRect(v.x, v.y, 34, 44, render::Color::rgb(0.9f, 0.75f, 0.3f, 0.9f));
        r.fillRect(v.x + 6, v.y + 8, 22, 28, render::Color::rgb(1, 0.95f, 0.6f, 0.9f));
    }
}

} // namespace game
