// save.hpp - meta progression state + KV save file in AppData.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>
#include <unordered_map>

namespace game {

struct MetaState {
    int soul = 0;
    // upgrade id -> level
    std::unordered_map<std::wstring, int> upgradeLevels;
    // lifetime stats
    int runs = 0, kills = 0, victories = 0;
    float bestTime = 0; // seconds, 0 = never

    int upgradeLevel(const std::wstring& id) const {
        auto it = upgradeLevels.find(id);
        return it == upgradeLevels.end() ? 0 : it->second;
    }
    // Aggregated effects
    float bonusMaxHp() const;       // needs tables -> computed by caller instead
};

class Save {
public:
    explicit Save(std::wstring path) : path_(std::move(path)) {}

    bool load(MetaState& m);   // missing file = fresh state (true)
    bool store(const MetaState& s) const;

private:
    std::wstring path_;
};

} // namespace game
