// app.cpp - application state machine implementation.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "app.hpp"
#include "../core/logger.hpp"
#include "../core/paths.hpp"
#include "../input/input.hpp"
#include "../game/hud.hpp"
#include "../game/enemies.hpp"
#include "../game/items.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace app {

namespace {
using render::Color;

constexpr int ID_BACK = 900;

float frand(std::mt19937& rng) { return (float)(rng() % 10000) / 10000.0f; }

// Top Y (px) of a floor the player can stand on in column tx: a solid tile with
// at least `clear` non-solid tiles directly above it. -1 if the column has none.
// (Bottom-up scans land inside filled underground columns, which is how pit
// respawns used to bury the player in solid rock.)
float standableTop(const game::TileMap& map, int tx, int clear = 4) {
    for (int ty = 0; ty < map.h; ++ty) {
        if (map.solidAt(tx, ty) != game::SSolid) continue;
        bool ok = true;
        for (int k = 1; k <= clear; ++k) {
            if (ty - k < 0) break; // open sky above is fine
            if (map.solidAt(tx, ty - k) == game::SSolid) { ok = false; break; }
        }
        if (ok) return ty * game::kTilePx;
    }
    return -1;
}
} // namespace

// ---------------------------------------------------------------- init

bool App::init(HWND hwnd) {
    assetsRoot_ = core::exeDir();
    if (!renderer_.init(hwnd)) { LOG_FATAL(L"app: renderer init failed"); return false; }
    if (!audio_.init()) { LOG_WARN(L"app: audio init failed, continuing silent"); }

    // --- sfx bank (files produced by scripts/build_assets.ps1) ---
    auto sfx = [&](const wchar_t* n) {
        audio_.loadSfx(n, assetsRoot_ + L"/sfx/" + n + L".wav");
    };
    sfx(L"swing"); sfx(L"hit"); sfx(L"kill"); sfx(L"hurt"); sfx(L"die");
    sfx(L"jump"); sfx(L"roll"); sfx(L"coin"); sfx(L"soul"); sfx(L"potion");
    sfx(L"buy"); sfx(L"error"); sfx(L"ui"); sfx(L"uiyes"); sfx(L"portal");
    sfx(L"bossdie"); sfx(L"slam"); sfx(L"boom"); sfx(L"charge"); sfx(L"door");

    // --- music ---
    audio_.loadMusic(L"title", assetsRoot_ + L"/bgm/title.mp3");
    audio_.loadMusic(L"cave",  assetsRoot_ + L"/bgm/cave.mp3");
    audio_.loadMusic(L"amb",   assetsRoot_ + L"/bgm/amb.mp3");
    audio_.loadMusic(L"boss",  assetsRoot_ + L"/bgm/boss.mp3");

    if (!tables_.loadAll(assetsRoot_)) return false;
    if (!strings_.load(assetsRoot_, L"zh")) return false;

    save_ = std::make_unique<game::Save>(
        core::joinPath(core::ensureAppDataSubdir(L"EchoesOfTheAbyss", L"saves"), L"save.txt"));
    save_->load(meta_);

    world_.tables = &tables_;
    world_.strings = &strings_;
    world_.audio = &audio_;
    world_.tex = &texStore_;
    world_.assetsRoot = assetsRoot_;

    audio_.playMusic(L"title", 0.55f);

    state_ = State::Title;
    showTitleMenu();
    LOG_INFO(L"app: init ok");
    return true;
}

void App::showTitleMenu() {
    menu_.setItems({
        {strings_.get(L"menu_start"),  1, true},
        {strings_.get(L"menu_forge"),  2, true},
        {strings_.get(L"menu_quit"),   3, true},
    });
}

void App::shutdown() {
    if (save_) save_->store(meta_);
    audio_.shutdown();
    texStore_.clear();
    renderer_.shutdown();
}

// ---------------------------------------------------------------- run setup

void App::startRun() {
    game::World fresh;
    fresh.tables = &tables_; fresh.strings = &strings_;
    fresh.audio = &audio_; fresh.tex = &texStore_;
    fresh.assetsRoot = assetsRoot_;
    fresh.rng.seed((unsigned)time(nullptr) ^ 0x9E3779B9u);
    world_ = std::move(fresh);

#ifdef EOTA_DEBUG
    LOG_INFO(L"heapcheck after-move: %d", (int)HeapValidate(GetProcessHeap(), 0, nullptr));
#endif

    // meta-derived bonuses
    float bonusHp = 20.0f * meta_.upgradeLevel(L"hp_up");
    float dmgMult = 1.0f + 0.10f * meta_.upgradeLevel(L"atk_up");
    float rollCdDelta = -0.15f * meta_.upgradeLevel(L"roll_cd");
    int extraPotions = meta_.upgradeLevel(L"potion_up");
    world_.coinMult = 1.0f + 0.25f * meta_.upgradeLevel(L"greed");

    world_.player.init(tables_);
    world_.player.hpMax = tables_.player().maxHp + bonusHp;
    world_.player.hp = world_.player.hpMax;
    world_.player.potions = tables_.player().potionsStart;
    world_.player.resetRun(bonusHp, dmgMult, rollCdDelta, extraPotions);

    bossHandled_ = false; bossTitleT_ = 0;
    startLevel(1);
    LOG_INFO(L"app: run started");
}

void App::startLevel(int idx) {
    const game::LevelRow* lr = tables_.level(idx);
    if (!lr) { LOG_FATAL(L"app: no level row for %d", idx); return; }

    world_.levelIndex = idx;
    world_.enemies.clear(); world_.pickups.clear(); world_.saws.clear();
    world_.racks.clear(); world_.portals.clear(); world_.waves.clear();
    world_.fx.clear();
    world_.bossActive = world_.bossDefeated = false;
    world_.bossDoorTileX = -1; world_.pendingExit = false; world_.pitRespawn = false;
    bossHandled_ = false;

    if (!game::generateLevel(assetsRoot_, idx, lr->roomsMin, lr->roomsMax,
                             world_.rng, world_.level)) {
        LOG_FATAL(L"app: level generation failed");
        return;
    }
    world_.tilesPath = assetsRoot_ + L"/gfx/tiles_" + lr->tint + L".png";

#ifdef EOTA_DEBUG
    auto heapCheck = [&](const wchar_t* tag) {
        BOOL ok = HeapValidate(GetProcessHeap(), 0, nullptr);
        LOG_INFO(L"heapcheck %ls: %d", tag, (int)ok);
    };
    heapCheck(L"after-genlevel");
#endif

    // materialize spawn points
    for (const auto& sp : world_.level.spawns) {
#ifdef EOTA_DEBUG
        BOOL hok = HeapValidate(GetProcessHeap(), 0, nullptr);
        if (!hok) { LOG_FATAL(L"heap corrupt before spawn kind=%d", (int)sp.kind); }
#endif
        switch (sp.kind) {
        case game::SpawnPoint::PlayerStart:
            world_.player.placeAt(sp.x, sp.y);
            levelStart_ = sp;
            break;
        case game::SpawnPoint::Enemy: {
            if (lr->enemyPool.empty()) break;
            if (frand(world_.rng) > std::min(1.0f, 0.8f * lr->density)) break;
            int total = 0;
            for (auto& p : lr->enemyPool) total += p.second;
            int rollv = (int)(world_.rng() % (unsigned)std::max(1, total));
            for (auto& p : lr->enemyPool) {
                rollv -= p.second;
                if (rollv < 0) { game::spawnEnemy(world_, p.first, sp.x, sp.y); break; }
            }
            break; }
        case game::SpawnPoint::CoinLine:
            for (int i = 0; i < 5; ++i)
                game::spawnPickup(world_, game::PK_COIN, sp.x + (i - 2) * 26.0f, sp.y - 6.0f, 1);
            break;
        case game::SpawnPoint::Potion:
            game::spawnPickup(world_, game::PK_POTION, sp.x, sp.y, 1);
            break;
        case game::SpawnPoint::WeaponRack: {
            game::Rack rk; rk.x = sp.x; rk.y = sp.y; world_.racks.push_back(rk); break; }
        case game::SpawnPoint::Portal: {
            game::Portal pt; pt.x = sp.x; pt.y = sp.y; world_.portals.push_back(pt); break; }
        case game::SpawnPoint::Saw: {
            game::Saw sw; sw.x0 = sp.x - 2.5f * game::kTilePx; sw.x1 = sp.x + 2.5f * game::kTilePx;
            sw.y = sp.y - game::kTilePx * 0.5f;
            world_.saws.push_back(sw); break; }
        case game::SpawnPoint::BossArena:
            break; // boss spawns on trigger
        }
    }

    world_.bannerT = 2.6f;
    updateCamera();
    audio_.playMusic(lr->bgm, 0.5f);
    LOG_INFO(L"app: entered level %d", idx);
}

void App::finalizeRun(bool victory) {
    int converted = world_.coins / (victory ? 5 : 10);
    endSoulGain_ = world_.soulRun + converted;
    endKills_ = world_.kills;
    endCoins_ = world_.coins;
    endTime_ = world_.runTime;

    meta_.soul += endSoulGain_;
    meta_.runs += 1;
    meta_.kills += world_.kills;
    if (victory) {
        meta_.victories += 1;
        if (meta_.bestTime <= 0 || world_.runTime < meta_.bestTime)
            meta_.bestTime = world_.runTime;
    }
    save_->store(meta_);
    LOG_INFO(L"app: run finalized victory=%d soulGain=%d", victory ? 1 : 0, endSoulGain_);
}

// ---------------------------------------------------------------- per-frame

void App::frame(const input::Input& in, float dt) {
    audio_.update();

    if (renderer_.begin()) {
        switch (state_) {
        case State::Title: {
            float my = 400;
            int id = menu_.update(in, renderer_, my);
            if (id == 1) { audio_.playSfx(L"uiyes", 0.7f); startRun(); state_ = State::Run; }
            else if (id == 2) { audio_.playSfx(L"uiyes", 0.7f); rebuildForgeMenu(); state_ = State::Forge; }
            else if (id == 3) quit_ = true;
            drawTitle();
            break; }
        case State::Forge: {
            float my = 240;
            int id = menu_.update(in, renderer_, my);
            if (id == ID_BACK) { audio_.playSfx(L"ui", 0.6f); showTitleMenu(); state_ = State::Title; }
            else if (id >= 0 && id < (int)tables_.upgrades().size()) {
                const game::UpgradeRow& up = tables_.upgrades()[id];
                int cur = meta_.upgradeLevel(up.id);
                int cost = (int)(up.costBase * std::pow(up.costGrowth, cur) + 0.5f);
                if (cur >= up.maxLevel) audio_.playSfx(L"error", 0.6f);
                else if (meta_.soul < cost) audio_.playSfx(L"error", 0.6f);
                else {
                    meta_.soul -= cost;
                    meta_.upgradeLevels[up.id] = cur + 1;
                    save_->store(meta_);
                    audio_.playSfx(L"buy", 0.7f);
                    rebuildForgeMenu();
                }
            }
            drawForge();
            break; }
        case State::Run: {
            if (in.pressed(VK_ESCAPE)) {
                audio_.setPausedAll(true);
                menu_.setItems({
                    {strings_.get(L"menu_resume"),  1, true},
                    {strings_.get(L"menu_abandon"), 2, true},
                });
                state_ = State::Pause;
                break;
            }
            updateRun(in, dt);
            drawRun();
            break; }
        case State::Pause: {
            float my = 360;
            int id = menu_.update(in, renderer_, my);
            if (id == 1 || in.pressed(VK_ESCAPE)) {
                audio_.setPausedAll(false);
                state_ = State::Run;
            } else if (id == 2) {
                audio_.setPausedAll(false);
                finalizeRun(false);
                menu_.setItems({{strings_.get(L"menu_continue"), 1, true}});
                state_ = State::Death;
            }
            drawRun();      // frozen world behind
            drawPause();
            break; }
        case State::Death: {
            float my = 480;
            int id = menu_.update(in, renderer_, my);
            if (id == 1) {
                audio_.playSfx(L"uiyes", 0.7f);
                audio_.playMusic(L"title", 0.55f);
                showTitleMenu();
                state_ = State::Title;
            }
            drawDeath();
            break; }
        case State::Victory: {
            float my = 480;
            int id = menu_.update(in, renderer_, my);
            if (id == 1) {
                audio_.playSfx(L"uiyes", 0.7f);
                audio_.playMusic(L"title", 0.55f);
                showTitleMenu();
                state_ = State::Title;
            }
            drawVictory();
            break; }
        }
        renderer_.end();
    }
}

// ---------------------------------------------------------------- run update

void App::updateCamera() {
    float px = world_.player.cx(), py = world_.player.cy();
    float maxX = std::max(0.0f, world_.level.map.pixelW() - render::kLogicalW);
    float maxY = std::max(0.0f, world_.level.map.pixelH() - render::kLogicalH);
    world_.camX = std::max(0.0f, std::min(px - render::kLogicalW * 0.5f, maxX));
    world_.camY = std::max(0.0f, std::min(py - render::kLogicalH * 0.55f, maxY));
}

void App::handleBossLogic() {
    game::World& w = world_;
    if (!w.level.hasBoss) return;

    if (!w.bossActive && !w.bossDefeated &&
        w.player.cx() > w.level.bossArena.x + 2.0f * game::kTilePx) {
        // close the door: solid column at the arena's left edge
        int doorTx = (int)(w.level.bossArena.x / game::kTilePx);
        w.bossDoorTileX = doorTx;
        for (int ty = 0; ty < w.level.map.h; ++ty)
            w.level.map.set(doorTx, ty, game::SSolid, 12);
        // Spawn on the first *standable* floor row. groundBottom() scans bottom-up
        // and lands on the filled rock under the arena, which buried the boss.
        int bossTx = (int)((w.level.bossArena.x + w.level.bossArena.w * 0.6f) / game::kTilePx);
        int bossCol = -1;
        float gy = -1;
        for (int d = 0; d < 20 && gy < 0; ++d) {
            for (int side = 0; side < 2 && gy < 0; ++side) {
                int col = bossTx + (side == 0 ? d : -d);
                if (col < 0 || col >= w.level.map.w) continue;
                float y = standableTop(w.level.map, col);
                if (y > 0) { bossCol = col; gy = y; }
            }
        }
        if (gy < 0) { // no floor at all: fall back to the arena bottom
            bossCol = bossTx;
            gy = w.level.bossArena.y + w.level.bossArena.h - game::kTilePx;
        }
        game::spawnEnemy(w, L"boss", (bossCol + 0.5f) * game::kTilePx, gy);
        w.bossActive = true;
        bossTitleT_ = 3.0f;
        audio_.playSfx(L"door", 0.9f);
        audio_.playMusic(L"boss", 0.6f);
        w.fx.shake(6.0f, 0.5f);
        LOG_INFO(L"app: boss fight started");
    }

    if (w.bossActive && w.bossDefeated && !bossHandled_) {
        bossHandled_ = true;
        // reopen the door
        for (int ty = 0; ty < w.level.map.h; ++ty)
            w.level.map.set(w.bossDoorTileX, ty, game::SEmpty, -1);
        // spawn exit portal deeper in the arena (same standable-floor lookup)
        int ptTx = (int)((w.level.bossArena.x + w.level.bossArena.w * 0.8f) / game::kTilePx);
        int ptCol = -1;
        float gy = -1;
        for (int d = 0; d < 20 && gy < 0; ++d) {
            for (int side = 0; side < 2 && gy < 0; ++side) {
                int col = ptTx + (side == 0 ? d : -d);
                if (col < 0 || col >= w.level.map.w) continue;
                float y = standableTop(w.level.map, col);
                if (y > 0) { ptCol = col; gy = y; }
            }
        }
        if (gy < 0) { ptCol = ptTx; gy = w.level.bossArena.y + w.level.bossArena.h - game::kTilePx; }
        game::Portal pt; pt.x = (ptCol + 0.5f) * game::kTilePx; pt.y = gy;
        w.portals.push_back(pt);
        audio_.playSfx(L"bossdie", 0.9f);
        audio_.playMusic(L"amb", 0.5f);
        w.fx.shake(8.0f, 0.6f);
        LOG_INFO(L"app: boss defeated, portal spawned");
    }
}

void App::updateRun(const input::Input& in, float dt) {
    game::World& w = world_;

#ifdef EOTA_DEBUG
    handleCheats(in);
#endif

    w.fx.update(dt);                       // fx always runs on real time
    float wdt = dt * w.fx.timeScale();     // hitstop freezes the world
    w.runTime += wdt;
    if (w.bannerT > 0) w.bannerT -= dt;
    if (bossTitleT_ > 0) bossTitleT_ -= dt;

    w.player.update(w, in, wdt);
    game::applyPlayerAttacks(w);
    game::updateEnemies(w, wdt);
    game::updateItems(w, wdt);

    if (in.pressed(L'E')) game::tryInteract(w);

    // pit fall respawn: back to the entry of the current room
    if (w.pitRespawn) {
        w.pitRespawn = false;
        int ptx = (int)(w.player.cx() / game::kTilePx);
        float rx = levelStart_.x, ry = levelStart_.y;
        for (const auto& room : w.level.rooms) {
            if (ptx >= room.tileX0 && ptx < room.tileX0 + room.tileW) {
                // Walk right from the room entry until a genuinely standable
                // column shows up (floor with open air above it).
                int end = room.tileX0 + room.tileW - 1;
                for (int col = room.tileX0 + 2; col < end; ++col) {
                    float gy = standableTop(w.level.map, col);
                    if (gy > 0) { rx = (col + 0.5f) * game::kTilePx; ry = gy; break; }
                }
                break;
            }
        }
        w.player.placeAt(rx, ry);
    }

    handleBossLogic();
    updateCamera();

    // level exit
    if (w.pendingExit) {
        w.pendingExit = false;
        if (w.levelIndex >= 3) {
            finalizeRun(true);
            menu_.setItems({{strings_.get(L"menu_continue"), 1, true}});
            state_ = State::Victory;
        } else {
            startLevel(w.levelIndex + 1);
        }
        return;
    }

    // death
    if (!w.player.alive() && w.player.deathT > 1.4f) {
        finalizeRun(false);
        menu_.setItems({{strings_.get(L"menu_continue"), 1, true}});
        state_ = State::Death;
    }
}

// ---------------------------------------------------------------- debug cheats

#ifdef EOTA_DEBUG
void App::handleCheats(const input::Input& in) {
    game::World& w = world_;

    if (in.pressed(VK_F1)) {
        w.cheatGod = !w.cheatGod;
        w.audio->playSfx(L"uiyes", 0.6f);
        LOG_INFO(L"cheat: god mode %ls", w.cheatGod ? L"ON" : L"OFF");
    }
    if (in.pressed(VK_F2)) {
        w.cheatOneHit = !w.cheatOneHit;
        w.audio->playSfx(L"uiyes", 0.6f);
        LOG_INFO(L"cheat: one-hit kill %ls", w.cheatOneHit ? L"ON" : L"OFF");
    }
    if (in.pressed(VK_F3)) {
        w.player.hp = w.player.hpMax;
        if (w.player.potions < 3) w.player.potions = 3;
        w.fx.floatText(w.player.cx(), w.player.cy() - 30, L"FULL",
                       render::Color::rgb(0.4f, 1, 0.5f), 20);
        w.fx.burst(w.player.cx(), w.player.cy(), render::Color::rgb(0.4f, 1, 0.5f), 10, 160);
        w.audio->playSfx(L"potion");
        LOG_INFO(L"cheat: refilled hp/potions");
    }
    if (in.pressed(VK_F4)) {
        // Reuse the normal portal exit: next level, or victory on the boss level.
        w.pendingExit = true;
        LOG_INFO(L"cheat: skip level %d", w.levelIndex);
    }
    if (in.pressed(VK_F5)) enterBossFight();
}

void App::enterBossFight() {
    game::World& w = world_;
    if (w.levelIndex != 3 || !w.level.hasBoss) startLevel(3);
    if (w.bossActive) { LOG_INFO(L"cheat: boss fight already active"); return; }

    const game::Rect& ar = w.level.bossArena;
    int tx = (int)((ar.x + ar.w * 0.5f) / game::kTilePx);
    float gy = standableTop(w.level.map, tx);
    for (int d = 0; d <= 20 && gy < 0; ++d) {
        for (int side = 0; side < 2 && gy < 0; ++side) {
            int col = tx + (side == 0 ? d : -d);
            if (col < 0 || col >= w.level.map.w) continue;
            float y = standableTop(w.level.map, col);
            if (y > 0) { tx = col; gy = y; }
        }
    }
    if (gy > 0) w.player.placeAt((tx + 0.5f) * game::kTilePx, gy);
    w.player.hp = w.player.hpMax;
    updateCamera();
    LOG_INFO(L"cheat: warped into boss arena (level 3, col=%d)", tx);
}

void App::drawCheatOverlay() {
    game::World& w = world_;
    std::wstring line = L"DEBUG  F1 无敌 ";
    line += (w.cheatGod ? L"ON " : L"OFF");
    line += L"  |  F2 一击必杀 ";
    line += (w.cheatOneHit ? L"ON " : L"OFF");
    line += L"  |  F3 回满血/药  |  F4 跳过本层  |  F5 直达第三层 Boss";
    renderer_.drawText(line, 16, 692, render::kLogicalW - 32, 20, 13,
                       Color::rgb(1.0f, 0.9f, 0.45f), 0, true);
}
#endif

// ---------------------------------------------------------------- drawing

void App::drawTitle() {
    renderer_.fillRect(0, 0, render::kLogicalW, render::kLogicalH, Color::rgb(0.05f, 0.04f, 0.09f));
    renderer_.fillRect(0, 500, render::kLogicalW, 220, Color::rgb(0.08f, 0.05f, 0.12f));
    renderer_.drawText(strings_.get(L"game_title"), 0, 150, render::kLogicalW, 80, 56,
                       Color::rgb(0.95f, 0.88f, 0.6f), 1, true);
    renderer_.drawText(strings_.get(L"game_subtitle"), 0, 240, render::kLogicalW, 30, 18,
                       Color::rgb(0.55f, 0.55f, 0.65f), 1);
    menu_.draw(renderer_, 400);
    // footer: meta stats
    std::wstring stats = strings_.get(L"hud_soul") + L" " + std::to_wstring(meta_.soul) +
        L"    " + strings_.get(L"stats_runs") + L" " + std::to_wstring(meta_.runs) +
        L"    " + strings_.get(L"stats_victories") + L" " + std::to_wstring(meta_.victories);
    renderer_.drawText(stats, 0, 640, render::kLogicalW, 24, 15, Color::rgb(0.5f, 0.5f, 0.6f), 1);
#ifdef EOTA_DEBUG
    renderer_.drawText(L"DEBUG 作弊：F1 无敌 / F2 一击必杀 / F3 回满血与药 / F4 跳过本层 / F5 直达第三层 Boss",
                       0, 692, render::kLogicalW, 20, 13, Color::rgb(1.0f, 0.9f, 0.45f), 1, true);
#endif
}

void App::rebuildForgeMenu() {
    std::vector<ui::MenuItem> items;
    int i = 0;
    for (const auto& up : tables_.upgrades()) {
        int cur = meta_.upgradeLevel(up.id);
        int cost = (int)(up.costBase * std::pow(up.costGrowth, cur) + 0.5f);
        std::wstring text = strings_.get(up.nameKey) + L"  Lv " + std::to_wstring(cur) +
                            L"/" + std::to_wstring(up.maxLevel);
        if (cur >= up.maxLevel) text += L"  MAX";
        else text += L"  - " + std::to_wstring(cost) + L" " + strings_.get(L"hud_soul");
        items.push_back({text, i, true});
        ++i;
    }
    items.push_back({strings_.get(L"menu_back"), ID_BACK, true});
    menu_.setItems(std::move(items));
}

void App::drawForge() {
    renderer_.fillRect(0, 0, render::kLogicalW, render::kLogicalH, Color::rgb(0.05f, 0.04f, 0.09f));
    renderer_.drawText(strings_.get(L"forge_title"), 0, 70, render::kLogicalW, 60, 40,
                       Color::rgb(0.95f, 0.88f, 0.6f), 1, true);
    renderer_.drawText(strings_.get(L"hud_soul") + L"  " + std::to_wstring(meta_.soul),
                       0, 150, render::kLogicalW, 30, 22, Color::rgb(0.45f, 0.8f, 1.0f), 1);
    // description of the hovered upgrade
    int sel = menu_.selected();
    if (sel >= 0 && sel < (int)tables_.upgrades().size())
        renderer_.drawText(strings_.get(tables_.upgrades()[sel].descKey),
                           0, 190, render::kLogicalW, 24, 15, Color::rgb(0.6f, 0.6f, 0.68f), 1);
    menu_.draw(renderer_, 240);
}

void App::drawRun() {
    game::World& w = world_;
    // background
    renderer_.fillRect(0, 0, render::kLogicalW, render::kLogicalH, Color::rgb(0.06f, 0.06f, 0.10f));

    renderer_.setCamera(w.camX + w.fx.shakeX(), w.camY + w.fx.shakeY());
    game::drawTiles(renderer_, texStore_, w.level.map, w.tilesPath, w.camX, w.camY);
    game::drawItems(w, renderer_, texStore_);
    game::drawEnemies(w, renderer_, texStore_);
    w.player.draw(renderer_, texStore_);
    w.fx.draw(renderer_);
    renderer_.clearCamera();

    game::drawHud(w, renderer_, texStore_);

#ifdef EOTA_DEBUG
    drawCheatOverlay();
#endif

    // boss intro title
    if (bossTitleT_ > 0.0f) {
        float a = std::min(1.0f, bossTitleT_);
        renderer_.drawText(strings_.get(L"boss_name"), 0, 120, render::kLogicalW, 50, 38,
                           Color::rgb(0.95f, 0.4f, 0.35f, a), 1, true);
    }
}

void App::drawPause() {
    renderer_.fillRect(0, 0, render::kLogicalW, render::kLogicalH, Color::rgb(0.02f, 0.02f, 0.04f, 0.7f));
    renderer_.drawText(strings_.get(L"pause_title"), 0, 240, render::kLogicalW, 60, 40,
                       Color::rgb(0.9f, 0.9f, 0.95f), 1, true);
    menu_.draw(renderer_, 360);
}

namespace {
void drawEndScreen(render::Renderer& r, const game::Strings& S, const std::wstring& titleKey,
                   const std::wstring& lineKey, Color titleC,
                   int kills, int coins, int soulGain, float time, ui::Menu& menu) {
    r.fillRect(0, 0, render::kLogicalW, render::kLogicalH, Color::rgb(0.04f, 0.03f, 0.07f));
    r.drawText(S.get(titleKey), 0, 110, render::kLogicalW, 70, 48, titleC, 1, true);
    r.drawText(S.get(lineKey), 0, 195, render::kLogicalW, 28, 17, Color::rgb(0.65f, 0.65f, 0.72f), 1);

    wchar_t buf[256];
    // UCRT wide printf treats %s as char*; wide strings must use %ls.
    swprintf(buf, 256, L"%ls  %d", S.get(L"death_kills").c_str(), kills);
    r.drawText(buf, 0, 290, render::kLogicalW, 26, 19, Color::rgb(0.85f, 0.85f, 0.9f), 1);
    swprintf(buf, 256, L"%ls  %d", S.get(L"death_coins").c_str(), coins);
    r.drawText(buf, 0, 326, render::kLogicalW, 26, 19, Color::rgb(0.98f, 0.85f, 0.3f), 1);
    swprintf(buf, 256, L"%ls  +%d", S.get(L"death_soul_gain").c_str(), soulGain);
    r.drawText(buf, 0, 362, render::kLogicalW, 26, 19, Color::rgb(0.45f, 0.8f, 1.0f), 1);
    swprintf(buf, 256, L"%ls  %02d:%02d", S.get(L"death_time").c_str(),
             (int)(time / 60), (int)time % 60);
    r.drawText(buf, 0, 398, render::kLogicalW, 26, 19, Color::rgb(0.7f, 0.7f, 0.78f), 1);
    menu.draw(r, 480);
}
} // namespace

void App::drawDeath() {
    drawEndScreen(renderer_, strings_, L"death_title", L"death_line",
                  Color::rgb(0.9f, 0.3f, 0.3f), endKills_, endCoins_, endSoulGain_, endTime_, menu_);
}

void App::drawVictory() {
    drawEndScreen(renderer_, strings_, L"victory_title", L"victory_line",
                  Color::rgb(0.95f, 0.85f, 0.4f), endKills_, endCoins_, endSoulGain_, endTime_, menu_);
}

} // namespace app
