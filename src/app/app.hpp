// app.hpp - application state machine: Title / Forge / Run / Pause / Death / Victory.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../render/renderer.hpp"
#include "../render/texture.hpp"
#include "../audio/audio.hpp"
#include "../ui/menu.hpp"
#include "../game/tables.hpp"
#include "../game/strings.hpp"
#include "../game/save.hpp"
#include "../game/world.hpp"
#include "../game/enemies.hpp"   // complete types needed: World holds these vectors by value
#include "../game/items.hpp"
#include <memory>

namespace input { class Input; }

namespace app {

class App {
public:
    bool init(HWND hwnd);
    void shutdown();

    void onResize() { renderer_.onResize(); }
    // One fixed game step (dt fixed by caller) + one render.
    void frame(const input::Input& in, float dt);

    bool quitRequested() const { return quit_; }

private:
    enum class State { Title, Forge, Run, Pause, Death, Victory };

    void startRun();
    void startLevel(int idx);
    void finalizeRun(bool victory);   // convert currencies, update meta, store save

    void updateRun(const input::Input& in, float dt);
    void updateCamera();
    void handleBossLogic();

    void drawTitle();
    void drawForge();
    void drawRun();
    void drawPause();
    void drawDeath();
    void drawVictory();

    void rebuildForgeMenu();
    void showTitleMenu();   // (re)build the main menu items; used on every return to Title

#ifdef EOTA_DEBUG
    // Tester cheats (debug builds only).
    void handleCheats(const input::Input& in);
    void enterBossFight();
    void drawCheatOverlay();
#endif

    // services
    render::Renderer renderer_;
    render::TextureStore texStore_;
    audio::Audio audio_;
    game::Tables tables_;
    game::Strings strings_;
    game::MetaState meta_;
    std::unique_ptr<game::Save> save_;
    std::wstring assetsRoot_;

    // state
    State state_ = State::Title;
    bool quit_ = false;
    game::World world_;
    ui::Menu menu_;

    // forge
    int forgeSel_ = 0;

    // run bookkeeping
    float bossTitleT_ = 0;
    bool bossHandled_ = false;
    game::SpawnPoint levelStart_;   // fallback respawn point

    // death/victory summary
    int endKills_ = 0, endCoins_ = 0, endSoulGain_ = 0;
    float endTime_ = 0;
};

} // namespace app
