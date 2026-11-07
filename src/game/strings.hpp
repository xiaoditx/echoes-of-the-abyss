// strings.hpp - localized text table (data/strings_<lang>.csv). Default zh.
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

class Strings {
public:
    bool load(const std::wstring& assetRoot, const std::wstring& lang = L"zh");
    // Returns the text for key, or the key itself (visible fallback) when missing.
    const std::wstring& get(const std::wstring& key) const;
private:
    std::unordered_map<std::wstring, std::wstring> map_;
};

} // namespace game
