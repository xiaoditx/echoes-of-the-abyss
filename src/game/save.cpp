// save.cpp - KV save file implementation. Format: one "key=value" per line, UTF-8.
#include "save.hpp"
#include "../core/logger.hpp"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace game {

static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
static std::wstring utf8ToWide2(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

bool Save::load(MetaState& m) {
    std::ifstream f(path_.c_str(), std::ios::binary);
    if (!f) {
        LOG_INFO(L"save: no save file at %s, starting fresh", path_.c_str());
        return true;
    }
    std::stringstream ss; ss << f.rdbuf();
    std::wstring text = utf8ToWide2(ss.str());
    size_t pos = 0;
    int lineNo = 0;
    while (pos < text.size()) {
        size_t nl = text.find(L'\n', pos);
        std::wstring line = text.substr(pos, nl == std::wstring::npos ? nl : nl - pos);
        pos = (nl == std::wstring::npos) ? text.size() : nl + 1;
        ++lineNo;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos || eq == 0) continue;
        std::wstring k = line.substr(0, eq), v = line.substr(eq + 1);
        int iv = _wtoi(v.c_str());
        if (k == L"soul") m.soul = iv;
        else if (k == L"stat_runs") m.runs = iv;
        else if (k == L"stat_kills") m.kills = iv;
        else if (k == L"stat_victories") m.victories = iv;
        else if (k == L"stat_best_time") m.bestTime = (float)_wtof(v.c_str());
        else if (k.rfind(L"up_", 0) == 0) m.upgradeLevels[k.substr(3)] = iv;
        else LOG_WARN(L"save: unknown key line %d: %s", lineNo, k.c_str());
    }
    LOG_INFO(L"save: loaded soul=%d runs=%d", m.soul, m.runs);
    return true;
}

bool Save::store(const MetaState& m) const {
    std::ofstream f(path_.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) {
        LOG_ERROR(L"save: cannot write %s", path_.c_str());
        return false;
    }
    std::wstring w;
    w += L"soul=" + std::to_wstring(m.soul) + L"\n";
    w += L"stat_runs=" + std::to_wstring(m.runs) + L"\n";
    w += L"stat_kills=" + std::to_wstring(m.kills) + L"\n";
    w += L"stat_victories=" + std::to_wstring(m.victories) + L"\n";
    wchar_t buf[64];
    swprintf_s(buf, L"stat_best_time=%.2f\n", m.bestTime);
    w += buf;
    for (auto& kv : m.upgradeLevels)
        w += L"up_" + kv.first + L"=" + std::to_wstring(kv.second) + L"\n";
    std::string u = wideToUtf8(w);
    f.write(u.data(), (std::streamsize)u.size());
    LOG_INFO(L"save: stored soul=%d", m.soul);
    return true;
}

} // namespace game
