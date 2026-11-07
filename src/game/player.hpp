// player.hpp - player character: movement, jump, roll, 3-hit combo, hurt/death.
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

namespace input { class Input; }
namespace render { class Renderer; class TextureStore; }

namespace game {

struct World;
class Tables;
struct WeaponRow;

enum class PState { Normal, Roll, Dead };

class Player {
public:
    void init(const Tables& t);                 // build animation clips once
    void resetRun(float bonusMaxHp, float dmgMult, float rollCdDelta, int extraPotions);
    void placeAt(float cx, float bottom);

    void update(World& w, const input::Input& in, float dt);
    void draw(render::Renderer& r, render::TextureStore& ts) const;

    Rect rect() const { return {x_, y_, kW, kH}; }
    float cx() const { return x_ + kW * 0.5f; }
    float cy() const { return y_ + kH * 0.5f; }
    float bottom() const { return y_ + kH; }

    bool alive() const { return state_ != PState::Dead; }
    bool invulnerable() const { return iframeT_ > 0.0f || state_ == PState::Roll; }
    void damage(World& w, float dmg, float fromX);
    void heal(World& w, float amount);
    void usePotion(World& w);

    // Current swing hitbox; only valid while attackHitActive().
    bool attackHitActive() const;
    Rect attackBox() const;
    int swingId() const { return swingId_; }
    float swingDamage() const;   // includes weapon + meta dmg mult, +/-10% rolled by caller
    float swingKnock() const;

    float vx() const { return vx_; }
    float vy() const { return vy_; }
    int facing() const { return facing_; }

    float hp = 100, hpMax = 100;
    int potions = 1;
    float deathT = 0;                 // time since death (for death anim delay)
    const WeaponRow* weapon = nullptr;
    std::wstring weaponId;

    static constexpr float kW = 36.0f, kH = 58.0f;   // collision box
    static constexpr float kDraw = 72.0f;            // sprite draw size (24px x3)

private:
    void startSwing(const Tables& t);
    void setClip(const std::wstring& name);
    void updateAnim(float dt);

    PState state_ = PState::Normal;
    float x_ = 0, y_ = 0, vx_ = 0, vy_ = 0;
    int facing_ = 1;
    bool onGround_ = false;

    float rollT_ = 0, rollCdT_ = 0, dropT_ = 0;
    float iframeT_ = 0, hurtT_ = 0;
    float coyote_ = 0, jumpBuf_ = 0;

    int comboIdx_ = 0;                // 0..2
    int swingId_ = 0;
    float atkT_ = -1;                 // <0 = not attacking; else time in swing
    float atkDur_ = 0.22f;
    float comboWinT_ = 0;             // remaining window to chain next combo

    float dmgMult_ = 1, rollCdDelta_ = 0;

    render::SpriteSet clips_;
    render::Animator anim_;
    std::wstring curClip_;
};

} // namespace game
