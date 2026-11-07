// player.cpp
#include "player.hpp"
#include "world.hpp"
#include "tables.hpp"
#include "strings.hpp"
#include "fx.hpp"
#include "level.hpp"
#include "../input/input.hpp"
#include "../audio/audio.hpp"
#include "../core/logger.hpp"
#include "../core/paths.hpp"
#include "../render/renderer.hpp"

#include <windows.h>
#include <cmath>

namespace game {

namespace {
constexpr float kBaseSwingDur[3] = {0.22f, 0.22f, 0.30f}; // total per combo step
constexpr float kHitFrom = 0.35f, kHitTo = 0.75f;         // active window as fraction of dur
constexpr float kRollDur = 0.32f;
constexpr float kHurtTime = 0.22f;
constexpr float kCoyote = 0.08f;
constexpr float kJumpBuffer = 0.12f;
} // namespace

void Player::init(const Tables& t) {
    clips_ = render::SpriteSet();
    // The animator may still point into the previous clip map (destroyed by the
    // assignment above); drop the cached clip name so setClip() re-resolves it.
    anim_ = render::Animator();
    curClip_.clear();
    const std::wstring tex = core::assetPath(L"gfx\\chars.png");
    for (const auto& a : t.playerAnims()) {
        render::AnimClip c;
        c.texturePath = tex;
        c.frameW = t.playerFrameW; c.frameH = t.playerFrameH;
        c.startIndex = a.startIndex; c.frameCount = a.frameCount;
        c.fps = a.fps; c.loop = a.loop;
        clips_.addClip(a.name, c);
    }
    weaponId = L"sword_prison";
    weapon = t.weapon(weaponId);
}

void Player::resetRun(float bonusMaxHp, float dmgMult, float rollCdDelta, int extraPotions) {
    // Tuning base values are applied by the caller (world) before reset via tables;
    // here we only fold in meta bonuses.
    dmgMult_ = dmgMult; rollCdDelta_ = rollCdDelta;
    potions += extraPotions;
    (void)bonusMaxHp;
}

void Player::placeAt(float cx, float bottom) {
    x_ = cx - kW * 0.5f; y_ = bottom - kH;
    vx_ = vy_ = 0;
    state_ = PState::Normal;
    iframeT_ = hurtT_ = rollT_ = rollCdT_ = dropT_ = 0;
    atkT_ = -1; comboIdx_ = 0; comboWinT_ = 0;
    deathT = 0;
    setClip(L"idle");
}

void Player::setClip(const std::wstring& name) {
    if (curClip_ == name) return;
    const render::AnimClip* c = clips_.find(name);
    if (c) { anim_.play(c); curClip_ = name; }
}

bool Player::attackHitActive() const {
    if (atkT_ < 0 || atkDur_ <= 0) return false;
    const float f = atkT_ / atkDur_;
    return f >= kHitFrom && f <= kHitTo;
}

Rect Player::attackBox() const {
    const float range = 60.0f + (weapon ? weapon->rangeBonus : 0.0f);
    const float h = 46.0f;
    Rect b;
    b.w = range; b.h = h;
    b.y = y_ + 6.0f;
    b.x = facing_ > 0 ? (x_ + kW - 4.0f) : (x_ + 4.0f - range);
    return b;
}

float Player::swingDamage() const {
    if (!weapon || comboIdx_ < 0 || comboIdx_ >= (int)weapon->comboDmg.size()) return 10.0f;
    return weapon->comboDmg[comboIdx_] * weapon->dmgMult * dmgMult_;
}
float Player::swingKnock() const {
    const float base[3] = {120.0f, 140.0f, 320.0f};
    int i = comboIdx_ < 0 ? 0 : (comboIdx_ > 2 ? 2 : comboIdx_);
    return base[i] * (weapon ? weapon->knockMult : 1.0f);
}

void Player::startSwing(const Tables& t) {
    const float spd = weapon ? weapon->speedMult : 1.0f;
    int step = comboIdx_;
    if (comboWinT_ <= 0.0f) { step = 0; comboIdx_ = 0; }
    atkDur_ = kBaseSwingDur[step] / (spd > 0.01f ? spd : 1.0f);
    atkT_ = 0.0f;
    ++swingId_;
    (void)t;
}

void Player::damage(World& w, float dmg, float fromX) {
    if (!alive() || invulnerable()) return;
#ifdef EOTA_DEBUG
    if (w.cheatGod) return;   // debug cheat: invincible
#endif
    hp -= dmg;
    iframeT_ = w.tables->player().hurtIframe;
    hurtT_ = kHurtTime;
    vx_ = (cx() < fromX ? -1.0f : 1.0f) * 190.0f;
    atkT_ = -1; comboWinT_ = 0; // interrupted
    w.fx.shake(6.0f, 0.25f);
    w.fx.burst(cx(), cy(), render::Color::rgb(1, 0.3f, 0.25f), 10);
    w.audio->playSfx(L"hurt");
    if (hp <= 0) {
        hp = 0;
        state_ = PState::Dead;
        deathT = 0;
        setClip(L"dead");
        w.audio->playSfx(L"die");
        LOG_INFO(L"player died at x=%.0f level=%d", x_, w.levelIndex);
    } else {
        setClip(L"hurt");
    }
}

void Player::heal(World& w, float amount) {
    if (!alive()) return;
    hp += amount;
    if (hp > hpMax) hp = hpMax;
    w.fx.floatText(cx(), y_ - 8, L"+" + std::to_wstring((int)amount), render::Color::rgb(0.4f, 1, 0.5f), 20);
    w.fx.burst(cx(), cy(), render::Color::rgb(0.4f, 1, 0.5f), 8, 120);
}

void Player::usePotion(World& w) {
    if (!alive() || potions <= 0 || hp >= hpMax) return;
    --potions;
    heal(w, hpMax * 0.5f);
    w.audio->playSfx(L"potion");
}

void Player::update(World& w, const input::Input& in, float dt) {
    const auto& tune = w.tables->player();

    if (state_ == PState::Dead) {
        deathT += dt;
        anim_.update(dt);
        return;
    }

    // timers
    if (iframeT_ > 0) iframeT_ -= dt;
    if (hurtT_ > 0) hurtT_ -= dt;
    if (rollCdT_ > 0) rollCdT_ -= dt;
    if (dropT_ > 0) dropT_ -= dt;
    if (comboWinT_ > 0) { comboWinT_ -= dt; if (comboWinT_ <= 0) comboIdx_ = 0; }

    const bool left = in.down(VK_LEFT) || in.down('A');
    const bool right = in.down(VK_RIGHT) || in.down('D');
    const bool downHeld = in.down(VK_DOWN) || in.down('S');
    const bool jumpPress = in.pressed(VK_SPACE) || in.pressed(VK_UP) || in.pressed('W');
    const bool jumpRel = in.released(VK_SPACE) || in.released(VK_UP) || in.released('W');
    const bool atkPress = in.pressed('J') || in.mousePressed(input::MouseBtn::Left);
    const bool rollPress = in.pressed(VK_SHIFT) || in.pressed('K') || in.mousePressed(input::MouseBtn::Right);
    const bool potionPress = in.pressed('Q') || in.pressed('F');

    if (potionPress) usePotion(w);

    // --- roll ---
    if (state_ == PState::Roll) {
        rollT_ -= dt;
        vx_ = facing_ * (tune.rollDist / kRollDur);
        // Short fixed cooldown: the next roll is ready right after the animation
        // plus a small step (Dead Cells rhythm). Meta upgrades can shave it to ~0.
        if (rollT_ <= 0) {
            state_ = PState::Normal;
            rollCdT_ = tune.rollCd + rollCdDelta_;
            if (rollCdT_ < 0.05f) rollCdT_ = 0.05f;
        }
    } else if (rollPress && rollCdT_ <= 0 && onGround_) {
        state_ = PState::Roll;
        rollT_ = kRollDur;
        iframeT_ = std::max(iframeT_, tune.rollIframe);
        if (left && !right) facing_ = -1;
        if (right && !left) facing_ = 1;
        w.audio->playSfx(L"roll", 0.6f);
        setClip(L"roll");
    }

    // --- horizontal move ---
    if (state_ != PState::Roll) {
        float dir = 0;
        if (left) dir -= 1;
        if (right) dir += 1;
        float spd = tune.moveSpeed;
        if (atkT_ >= 0) spd *= 0.4f;   // attacking slows movement
        if (hurtT_ > 0) { /* knockback already in vx_ */ }
        else vx_ = dir * spd;
        if (dir != 0 && hurtT_ <= 0) facing_ = dir > 0 ? 1 : -1;
    }

    // --- attack ---
    if (atkT_ >= 0) {
        atkT_ += dt;
        if (atkT_ >= atkDur_) {
            atkT_ = -1;
            comboWinT_ = tune.comboWindow;
            if (comboIdx_ < 2) {} // wait for chain
            else comboIdx_ = 0;
        }
    }
    if (atkPress && state_ != PState::Roll && hurtT_ <= 0) {
        if (atkT_ < 0) {
            if (comboWinT_ > 0 && comboIdx_ < 2) ++comboIdx_;
            startSwing(*w.tables);
            w.audio->playSfx(L"swing", 0.7f);
            // white arc: particles at the blade
            Rect ab = attackBox();
            w.fx.burst(facing_ > 0 ? ab.x + ab.w : ab.x, ab.y + ab.h * 0.5f,
                       render::Color::rgb(1, 1, 1), 6, 220);
        }
    }

    // --- jumping ---
    if (onGround_) coyote_ = kCoyote; else if (coyote_ > 0) coyote_ -= dt;
    if (jumpPress) jumpBuf_ = kJumpBuffer; else if (jumpBuf_ > 0) jumpBuf_ -= dt;

    if (jumpBuf_ > 0 && state_ != PState::Roll) {
        if (downHeld && onGround_) {
            dropT_ = 0.2f; // drop through one-way platform
            jumpBuf_ = 0;
        } else if (coyote_ > 0) {
            vy_ = tune.jumpVel;
            onGround_ = false; coyote_ = 0; jumpBuf_ = 0;
            w.audio->playSfx(L"jump", 0.5f);
        }
    }
    if (jumpRel && vy_ < 0) vy_ *= 0.5f; // short hop

    // --- physics ---
    vy_ += tune.gravity * dt;
    if (vy_ > 900) vy_ = 900;
    Rect r = rect();
    MoveResult mr = moveAabb(r, vx_ * dt, vy_ * dt, w.level.map, dropT_ > 0);
    x_ = r.x; y_ = r.y;
    onGround_ = mr.onGround;
    if (mr.hitHead && vy_ < 0) vy_ = 0;
    if (onGround_ && vy_ > 0) vy_ = 0;

    // pit fall: damage + respawn at room entry (handled by app via flag)
    if (y_ > w.level.map.pixelH() + 80.0f) {
        damage(w, 15.0f, cx());
        iframeT_ = std::max(iframeT_, 1.0f);
        w.pitRespawn = true;
    }

    updateAnim(dt);
}

void Player::updateAnim(float dt) {
    if (state_ == PState::Dead) { setClip(L"dead"); anim_.update(dt); return; }
    if (state_ == PState::Roll) { setClip(L"roll"); anim_.update(dt); return; }
    if (hurtT_ > 0) { setClip(L"hurt"); }
    else if (atkT_ >= 0) { setClip(L"attack"); }
    else if (!onGround_) { setClip(vy_ < 0 ? L"jump" : L"fall"); }
    else if (std::fabs(vx_) > 10.0f) { setClip(L"run"); }
    else setClip(L"idle");
    anim_.update(dt);
}

void Player::draw(render::Renderer& r, render::TextureStore& ts) const {
    // blink during hurt iframes (not roll)
    if (iframeT_ > 0 && state_ != PState::Roll) {
        int phase = (int)(iframeT_ * 12.0f);
        if ((phase & 1) == 0) return;
    }
    const float dx = cx() - kDraw * 0.5f;
    const float dy = y_ + kH - kDraw;
    anim_.draw(r, ts, dx, dy, kDraw, kDraw, facing_ < 0);

    // attack arc indicator
    if (attackHitActive()) {
        Rect ab = attackBox();
        r.fillRect(ab.x, ab.y, ab.w, ab.h, render::Color::rgb(1, 1, 1, 0.35f));
    }
}

} // namespace game
