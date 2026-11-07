// audio.cpp - yumo wrapper implementation.
// NOTE: yumo::preloadAudio is asynchronous. addAudio(preloadId) throws if the
// preload has not finished yet, so every play path checks the readySign first.
#include "audio.hpp"
#include "yumo_except.hpp"
#include "../core/logger.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

namespace audio {

namespace {

Audio::Preload preloadAsync(const std::wstring& path, const wchar_t* tag) {
    Audio::Preload p;
    try {
        p.ready = std::make_shared<yumo::readySign>(false);
        p.id = yumo::preloadAudio(path.c_str(), p.ready.get());
        p.requested = true;
    } catch (const yumo::exception& e) {
        LOG_ERROR(L"preloadAudio(%ls) failed, type=%d", tag, (int)e.getType());
        p.requested = false;
    } catch (...) {
        LOG_ERROR(L"preloadAudio(%ls) failed (unknown)", tag);
        p.requested = false;
    }
    return p;
}

bool isReady(const Audio::Preload& p) {
    return p.requested && p.ready && p.ready->load() != false;
}

Audio* g_instance = nullptr; // for the playback-finished callback (single Audio)

} // namespace

bool Audio::init() {
    g_instance = this;
    yumo::registerPlaybackFinishedCallback([](size_t id) {
        if (g_instance && g_instance->bgmHasInstance_ && id == g_instance->bgmInstanceId_)
            g_instance->bgmFinished_.store(true);
    });
    LOG_INFO(L"audio init (yumo)");
    return true;
}

void Audio::shutdown() {
    yumo::unregisterPlaybackFinishedCallback();
    g_instance = nullptr;
    stopMusic();
}

void Audio::loadSfx(const std::wstring& name, const std::wstring& path) {
    if (sfx_.count(name) || isQueued(name, false)) return;
    pending_.push_back({false, name, path});
}

void Audio::loadMusic(const std::wstring& name, const std::wstring& path) {
    if (music_.count(name) || isQueued(name, true)) return;
    pending_.push_back({true, name, path});
}

bool Audio::isQueued(const std::wstring& name, bool music) const {
    for (const auto& p : pending_)
        if (p.music == music && p.name == name) return true;
    return false;
}

void Audio::issueNextPreload() {
    if (pending_.empty()) return;
    PendingLoad pl = std::move(pending_.front());
    pending_.erase(pending_.begin());
    Preload p = preloadAsync(pl.path, pl.name.c_str());
    if (!p.requested) return;
    if (pl.music) music_[pl.name] = std::move(p);
    else sfx_[pl.name] = std::move(p);
}

void Audio::playSfx(const std::wstring& name, float volume) {
    auto it = sfx_.find(name);
    if (it == sfx_.end()) return;
    if (!isReady(it->second)) return; // still decoding; skip this one-shot
    try {
        yumo::addAudio(it->second.id, volume * masterVolume_);
    } catch (const yumo::exception& e) {
        // InvalidData right after readySign is the known yumo race; a lost
        // one-shot is harmless, so don't spam the log with it.
        if (e.getType() != yumo::exception::type::InvalidData)
            LOG_WARN(L"addAudio(sfx %ls) failed, type=%d", name.c_str(), (int)e.getType());
    } catch (...) {
        LOG_WARN(L"addAudio(sfx %ls) failed (unknown)", name.c_str());
    }
}

void Audio::playMusic(const std::wstring& name, float volume) {
    if (currentBgm_ == name && (bgmHasInstance_ || bgmPending_)) return;
    stopMusic();
    currentBgm_ = name;
    bgmVolume_ = volume;
    bgmPending_ = true; // update() starts it once the preload is ready
}

void Audio::stopMusic() {
    if (bgmHasInstance_) {
        try { yumo::remove(bgmInstanceId_); } catch (...) {}
        bgmHasInstance_ = false;
    }
    currentBgm_.clear();
    bgmPending_ = false;
    bgmFinished_.store(false);
}

void Audio::update() {
    issueNextPreload();
    if (currentBgm_.empty()) return;
    auto it = music_.find(currentBgm_);
    if (it == music_.end()) {
        if (isQueued(currentBgm_, true)) return; // not issued yet: keep waiting
        LOG_WARN(L"music not loaded: %ls", currentBgm_.c_str());
        currentBgm_.clear();
        bgmPending_ = false;
        return;
    }
    // yumo has no loop flag: restart when the finished callback fires.
    bool needRestart = bgmPending_ || bgmFinished_.exchange(false);
    if (!needRestart) return;
    if (!isReady(it->second)) return; // keep pending until decode finishes
    try {
        auto ni = yumo::addAudio(it->second.id, bgmVolume_ * masterVolume_);
        bgmInstanceId_ = ni.instanceId;
        bgmHasInstance_ = true;
        bgmPending_ = false;
    } catch (const yumo::exception& e) {
        // InvalidData here is a known yumo race: readySign flips slightly before
        // the preloaded buffer is usable. Keep pending and retry next frame.
        if (e.getType() == yumo::exception::type::InvalidData) return;
        LOG_ERROR(L"addAudio(music %ls) failed, type=%d", currentBgm_.c_str(), (int)e.getType());
        bgmPending_ = false;
        currentBgm_.clear();
    } catch (...) {
        LOG_ERROR(L"addAudio(music %ls) failed (unknown)", currentBgm_.c_str());
        bgmPending_ = false;
        currentBgm_.clear();
    }
}

void Audio::setMasterVolume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    masterVolume_ = v;
    yumo::global.volume = v;
}

float Audio::masterVolume() const { return masterVolume_; }

void Audio::setMuted(bool m) {
    muted_ = m;
    yumo::global.mute = m;
}

bool Audio::muted() const { return muted_; }

void Audio::setPausedAll(bool p) {
    paused_ = p;
    yumo::global.stop = p;
}

} // namespace audio
