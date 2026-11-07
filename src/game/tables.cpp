// tables.cpp - CSV config table loading implementation.
#include "tables.hpp"
#include "../core/logger.hpp"
#include "../core/paths.hpp"

#include <windows.h>

#include <fstream>
#include <sstream>
#include <codecvt>
#include <locale>
#include <cstdlib>

namespace game {

// ---------- CSV parsing ----------

static std::wstring utf8ToWide(const std::string& s) {
    // Skip BOM
    size_t i = 0;
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        i = 3;
    if (i >= s.size()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str() + (int)i, (int)(s.size() - i), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str() + (int)i, (int)(s.size() - i), out.data(), n);
    return out;
}

std::vector<std::vector<std::wstring>> parseCsvFile(const std::wstring& path) {
    std::vector<std::vector<std::wstring>> rows;
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) {
        LOG_ERROR(L"tables: cannot open %s", path.c_str());
        return rows;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::wstring text = utf8ToWide(ss.str());

    std::vector<std::wstring> cur;
    std::wstring field;
    bool inQ = false, any = false;
    auto pushField = [&] { cur.push_back(field); field.clear(); };
    auto pushRow = [&] {
        if (!cur.empty() || any) { rows.push_back(cur); cur.clear(); }
        any = false;
    };
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (inQ) {
            if (c == L'"') {
                if (i + 1 < text.size() && text[i + 1] == L'"') { field += L'"'; ++i; }
                else inQ = false;
            } else field += c;
        } else if (c == L'"' && field.empty()) {
            inQ = true; any = true;
        } else if (c == L',') {
            pushField(); any = true;
        } else if (c == L'\r') {
            // skip
        } else if (c == L'\n') {
            pushField(); pushRow();
        } else {
            field += c; any = true;
        }
    }
    if (!field.empty() || !cur.empty() || any) { pushField(); pushRow(); }

    // drop comment/empty rows
    std::vector<std::vector<std::wstring>> out;
    for (auto& r : rows) {
        if (r.empty()) continue;
        if (r[0].empty() || r[0][0] == L'#') continue;
        // trim trailing empty fields
        while (!r.empty() && r.back().empty()) r.pop_back();
        if (!r.empty()) out.push_back(std::move(r));
    }
    return out;
}

static float toF(const std::wstring& s, float dflt = 0) {
    if (s.empty()) return dflt;
    return (float)_wtof(s.c_str());
}
static int toI(const std::wstring& s, int dflt = 0) {
    if (s.empty()) return dflt;
    return _wtoi(s.c_str());
}

// ---------- loading ----------

bool Tables::loadAll(const std::wstring& root) {
    const std::wstring dir = core::joinPath(root, L"tables");

    // player.csv: key,value
    for (auto& r : parseCsvFile(core::joinPath(dir, L"player.csv"))) {
        if (r.size() < 2 || r[0] == L"key") continue;
        const std::wstring& k = r[0]; float v = toF(r[1]);
        if (k == L"max_hp") player_.maxHp = v;
        else if (k == L"move_speed") player_.moveSpeed = v;
        else if (k == L"jump_vel") player_.jumpVel = v;
        else if (k == L"gravity") player_.gravity = v;
        else if (k == L"roll_dist") player_.rollDist = v;
        else if (k == L"roll_iframe") player_.rollIframe = v;
        else if (k == L"roll_cd") player_.rollCd = v;
        else if (k == L"hurt_iframe") player_.hurtIframe = v;
        else if (k == L"combo_window") player_.comboWindow = v;
        else if (k == L"potions_start") player_.potionsStart = (int)v;
    }

    for (auto& r : parseCsvFile(core::joinPath(dir, L"weapons.csv"))) {
        if (r.size() < 7 || r[0] == L"id") continue;
        WeaponRow w;
        w.id = r[0]; w.nameKey = r[1];
        w.dmgMult = toF(r[2], 1); w.speedMult = toF(r[3], 1);
        w.rangeBonus = toF(r[4]); w.knockMult = toF(r[5], 1);
        // combo damages separated by ';'
        std::wstring cur;
        for (wchar_t c : r[6]) {
            if (c == L';') { if (!cur.empty()) { w.comboDmg.push_back(toF(cur)); cur.clear(); } }
            else cur += c;
        }
        if (!cur.empty()) w.comboDmg.push_back(toF(cur));
        if (w.comboDmg.empty()) w.comboDmg = {10, 12, 18};
        weapons_.push_back(std::move(w));
    }

    for (auto& r : parseCsvFile(core::joinPath(dir, L"enemies.csv"))) {
        if (r.size() < 12 || r[0] == L"id") continue;
        EnemyRow e;
        e.id = r[0]; e.nameKey = r[1];
        e.hp = toF(r[2], 10); e.dmg = toF(r[3], 5); e.speed = toF(r[4], 50);
        e.coins = toI(r[5], 1); e.soul = toI(r[6], 1);
        e.animStart = toI(r[7]); e.animCount = toI(r[8], 2); e.scale = toI(r[9], 3);
        e.ai = r[11];
        enemies_.push_back(std::move(e));
    }

    for (auto& r : parseCsvFile(core::joinPath(dir, L"upgrades.csv"))) {
        if (r.size() < 9 || r[0] == L"id") continue;
        UpgradeRow u;
        u.id = r[0]; u.nameKey = r[1]; u.descKey = r[2];
        u.maxLevel = toI(r[3], 1); u.costBase = toF(r[4], 10); u.costGrowth = toF(r[5], 1.5f);
        u.effectKey = r[7]; u.effectPerLevel = toF(r[8]);
        upgrades_.push_back(std::move(u));
    }

    for (auto& r : parseCsvFile(core::joinPath(dir, L"levels.csv"))) {
        if (r.size() < 9 || r[0] == L"level") continue;
        LevelRow l;
        l.level = toI(r[1], 1) ;
        // accept header either "level" with numeric first col
        l.level = toI(r[0], 1);
        l.roomsMin = toI(r[2], 5); l.roomsMax = toI(r[3], 7);
        // pool "slime:5;bat:3"
        std::wstring cur, idw; int wgt = 1; bool inW = false;
        auto flush = [&] {
            if (!idw.empty()) l.enemyPool.push_back({idw, wgt});
            idw.clear(); wgt = 1; inW = false;
        };
        for (wchar_t c : r[5]) {
            if (c == L':') inW = true;
            else if (c == L';') flush();
            else if (inW) { if (isdigit(c)) wgt = wgt * 10 + (c - L'0'); }
            else idw += c;
        }
        flush();
        l.density = toF(r[6], 1);
        l.tint = r[7]; l.bgm = r[8];
        levels_.push_back(std::move(l));
    }

    for (auto& r : parseCsvFile(core::joinPath(dir, L"loot.csv"))) {
        if (r.size() < 6 || r[0] == L"source") continue;
        LootRow lo;
        lo.id = r[0];
        lo.coinMin = toI(r[1]); lo.coinMax = toI(r[2]);
        lo.soulChance = toF(r[3]); lo.soulN = toI(r[4], 1);
        lo.potionChance = toF(r[5]);
        loot_.push_back(std::move(lo));
    }

    for (auto& r : parseCsvFile(core::joinPath(dir, L"player_anims.csv"))) {
        if (r.size() < 2) continue;
        if (r[0] == L"name") continue;
        if (r[0] == L"frame_w") { playerFrameW = toI(r[1], playerFrameW); continue; }
        if (r[0] == L"frame_h") { playerFrameH = toI(r[1], playerFrameH); continue; }
        if (r[0] == L"sheet_cols") { playerSheetCols = toI(r[1], playerSheetCols); continue; }
        if (r.size() < 5) continue;
        PlayerAnimRow a;
        a.name = r[0]; a.startIndex = toI(r[1]); a.frameCount = toI(r[2], 1);
        a.fps = toF(r[3], 10); a.loop = toI(r[4], 1) != 0;
        playerAnims_.push_back(std::move(a));
    }

    LOG_INFO(L"tables: weapons=%zu enemies=%zu upgrades=%zu levels=%zu loot=%zu anims=%zu",
             weapons_.size(), enemies_.size(), upgrades_.size(), levels_.size(), loot_.size(), playerAnims_.size());
    if (enemies_.empty() || levels_.empty() || playerAnims_.empty()) {
        LOG_FATAL(L"tables: required tables missing/empty in %s", dir.c_str());
        return false;
    }
    return true;
}

const WeaponRow* Tables::weapon(const std::wstring& id) const {
    for (auto& w : weapons_) if (w.id == id) return &w;
    return nullptr;
}
const EnemyRow* Tables::enemy(const std::wstring& id) const {
    for (auto& e : enemies_) if (e.id == id) return &e;
    return nullptr;
}
const UpgradeRow* Tables::upgrade(const std::wstring& id) const {
    for (auto& u : upgrades_) if (u.id == id) return &u;
    return nullptr;
}
const LevelRow* Tables::level(int lv) const {
    for (auto& l : levels_) if (l.level == lv) return &l;
    return nullptr;
}
const LootRow* Tables::loot(const std::wstring& id) const {
    for (auto& lo : loot_) if (lo.id == id) return &lo;
    return nullptr;
}

} // namespace game
