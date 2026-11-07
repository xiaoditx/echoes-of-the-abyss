// strings.cpp - localized text table implementation.
#include "strings.hpp"
#include "tables.hpp"
#include "../core/logger.hpp"
#include "../core/paths.hpp"

namespace game {

bool Strings::load(const std::wstring& assetRoot, const std::wstring& lang) {
    map_.clear();
    std::wstring path = core::joinPath(assetRoot, L"strings_" + lang + L".csv");
    for (auto& r : parseCsvFile(path)) {
        if (r.size() < 2 || r[0] == L"key") continue;
        map_[r[0]] = r[1];
    }
    LOG_INFO(L"strings: loaded %zu entries from %s", map_.size(), path.c_str());
    if (map_.empty()) {
        LOG_ERROR(L"strings: no entries loaded (lang=%s)", lang.c_str());
        return false;
    }
    return true;
}

const std::wstring& Strings::get(const std::wstring& key) const {
    auto it = map_.find(key);
    if (it != map_.end()) return it->second;
    return key; // fallback: show the key so missing text is visible
}

} // namespace game
