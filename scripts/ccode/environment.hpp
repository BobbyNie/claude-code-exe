#pragma once
#include "common.hpp"
#include <filesystem>
#include <map>
#include <vector>

namespace ccode {
// Build one real environment block. Children inherit the same namespace without hooks.
inline std::vector<std::wstring> BuildEnvironment(const std::vector<std::wstring>& inherited,
                                                const std::filesystem::path& profile) {
    std::map<std::wstring, std::wstring> entries;
    auto put = [&](const std::wstring& name, const std::wstring& value) {
        entries[Lower(name)] = name + L"=" + value;
    };
    for (const auto& entry : inherited) {
        const auto eq = entry.find(L'=', entry[0] == L'=' ? 1 : 0);
        if (eq == std::wstring::npos) continue;
        auto name = entry.substr(0, eq);
        if (StartsWithInsensitive(name, L"ANTHROPIC_") ||
            StartsWithInsensitive(name, L"CLAUDE_") ||
            Lower(name) == L"claudecode") continue;
        put(name, entry.substr(eq + 1));
    }
    for (const auto& entry : inherited) {
        auto eq = entry.find(L'=');
        if (eq == std::wstring::npos || eq == 0) continue;
        auto name = entry.substr(0, eq);
        if (StartsWithInsensitive(name, L"A_") || StartsWithInsensitive(name, L"C_"))
            put(ExpandEnvironmentName(name), entry.substr(eq + 1));
    }
    put(L"HOME", (profile / L"home").wstring());
    put(L"USERPROFILE", (profile / L"home").wstring());
    put(L"APPDATA", (profile / L"roaming").wstring());
    put(L"LOCALAPPDATA", (profile / L"local").wstring());
    put(L"TEMP", (profile / L"temp").wstring());
    put(L"TMP", (profile / L"temp").wstring());
    put(L"CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC", L"1");
    put(L"DISABLE_AUTOUPDATER", L"1");
    std::vector<std::wstring> result;
    for (const auto& item : entries) result.push_back(item.second);
    return result;
}
}
