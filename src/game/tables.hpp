// tables.hpp - CSV config table loading (data/tables/*.csv). See design/data-schema.md.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>
#include <vector>
#include <unordered_map>

namespace game {

struct PlayerTuning {
    float maxHp = 100, moveSpeed = 220, jumpVel = -560, gravity = 1600;
    float rollDist = 130, rollIframe = 0.35f, rollCd = 0.2f;
    float hurtIframe = 0.8f, comboWindow = 0.45f;
    int potionsStart = 1;
};

struct WeaponRow {
    std::wstring id, nameKey;
    float dmgMult = 1, speedMult = 1, rangeBonus = 0, knockMult = 1;
    std::vector<float> comboDmg; // per-swing base damage
};

struct EnemyRow {
    std::wstring id, nameKey;
    float hp = 10, dmg = 5, speed = 50;
    int coins = 1, soul = 1;
    int animStart = 0, animCount = 2, scale = 3;
    std::wstring ai;
};

struct UpgradeRow {
    std::wstring id, nameKey, descKey;
    int maxLevel = 1;
    float costBase = 10, costGrowth = 1.5f;
    std::wstring effectKey;
    float effectPerLevel = 0;
};

struct LevelRow {
    int level = 1, roomsMin = 5, roomsMax = 7;
    std::vector<std::pair<std::wstring, int>> enemyPool; // (id, weight)
    float density = 1.0f;
    std::wstring tint, bgm;
};

struct LootRow {
    std::wstring id;
    int coinMin = 0, coinMax = 0;
    float soulChance = 0; int soulN = 0;
    float potionChance = 0;
};

// Parsed animation clip definition for the player sheet (data/tables/player_anims.csv).
struct PlayerAnimRow {
    std::wstring name;
    int startIndex = 0, frameCount = 1;
    float fps = 10;
    bool loop = true;
};

class Tables {
public:
    // Loads all tables from <assetRoot>/tables. Fatal-logs and returns false on hard failure.
    bool loadAll(const std::wstring& assetRoot);

    const PlayerTuning& player() const { return player_; }
    const WeaponRow* weapon(const std::wstring& id) const;
    const EnemyRow* enemy(const std::wstring& id) const;
    const UpgradeRow* upgrade(const std::wstring& id) const;
    const LevelRow* level(int lv) const;
    const LootRow* loot(const std::wstring& enemyId) const;

    const std::vector<WeaponRow>& weapons() const { return weapons_; }
    const std::vector<UpgradeRow>& upgrades() const { return upgrades_; }
    const std::vector<PlayerAnimRow>& playerAnims() const { return playerAnims_; }

    // Player sheet layout (from player_anims.csv header meta or sheet.csv). Defaults 50x37 grid.
    int playerFrameW = 50, playerFrameH = 37, playerSheetCols = 8;

private:
    PlayerTuning player_;
    std::vector<WeaponRow> weapons_;
    std::vector<EnemyRow> enemies_;
    std::vector<UpgradeRow> upgrades_;
    std::vector<LevelRow> levels_;
    std::vector<LootRow> loot_;
    std::vector<PlayerAnimRow> playerAnims_;
};

// Minimal CSV parser: UTF-8 (BOM ok), '#' comment lines, double-quoted fields, \r\n tolerant.
// Returns rows of fields. Empty file -> empty vector (not an error by itself).
std::vector<std::vector<std::wstring>> parseCsvFile(const std::wstring& path);

} // namespace game
