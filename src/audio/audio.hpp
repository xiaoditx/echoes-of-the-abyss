// audio.hpp - thin game-facing wrapper over yumo audio: BGM loop management + SFX bank.
// Only WAV (PCM, <=2ch) and MP3 are supported by the underlying library.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <memory>
#include <atomic>

#include "audioPlayer.hpp" // yumo audio (develop/libs) - for readySign/atomic

namespace audio {

class Audio {
public:
    bool init();                       // currently just marks ready (yumo needs no explicit init)
    void shutdown();

    // Register a sound effect under a short name; preloads it (async).
    void loadSfx(const std::wstring& name, const std::wstring& path);
    // Preload a music track (async).
    void loadMusic(const std::wstring& name, const std::wstring& path);

    // Play a previously registered sfx. volume 0..1.
    void playSfx(const std::wstring& name, float volume = 1.0f);

    // Start looping a music track (restarts from beginning if already playing another).
    void playMusic(const std::wstring& name, float volume = 1.0f);
    void stopMusic();

    // Call once per frame: restarts the BGM instance when it finished (manual loop).
    void update();

    // Global volume / mute (applied to everything via yumo's global sign).
    void setMasterVolume(float v);     // 0..1
    float masterVolume() const;
    void setMuted(bool m);
    bool muted() const;

    // Pause/resume everything (used by pause menu).
    void setPausedAll(bool p);

    // Nested so cpp helpers can reference it; not for external use.
    struct Preload {
        size_t id = 0;
        bool requested = false;
        // Heap-allocated: the yumo reader thread writes through this pointer,
        // so the address must stay stable across map rehashes/copies.
        std::shared_ptr<yumo::readySign> ready;
    };

private:
    // yumo's decoder is unreliable when many preloadAudio calls run concurrently
    // (addAudio then throws InvalidData for a random subset). Loads are therefore
    // queued and issued one per update() call.
    struct PendingLoad {
        bool music = false;
        std::wstring name, path;
    };

    std::vector<PendingLoad> pending_;
    std::unordered_map<std::wstring, Preload> sfx_;
    std::unordered_map<std::wstring, Preload> music_;

    bool isQueued(const std::wstring& name, bool music) const;
    void issueNextPreload();

    std::wstring currentBgm_;
    size_t bgmInstanceId_ = 0;
    bool bgmHasInstance_ = false;      // instanceId==0 is a VALID id in yumo
    bool bgmPending_ = false;          // waiting for preload / retry after race
    std::atomic<bool> bgmFinished_{false}; // set by yumo finished callback
    float bgmVolume_ = 1.0f;
    float masterVolume_ = 1.0f;
    bool muted_ = false;
    bool paused_ = false;
};

} // namespace audio
