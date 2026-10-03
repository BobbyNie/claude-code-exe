#pragma once
#include "common.hpp"
#include <filesystem>
#include <map>
#include <vector>

namespace ccode {
// Build one real environment block. Children inherit the same namespace without hooks.
inline std::vector<std::wstring> BuildEnvironment(const std::vector<std::wstring>& inherited,
                                                const std::filesystem::path& profile,
                                                const std::wstring& gateway = {}) {
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
    // Inherited process settings must not disable HTTPS certificate validation.
    put(L"NODE_TLS_REJECT_UNAUTHORIZED", L"1");
    // A failed request must return control, not replay work behind the frontend.
    // Apply after aliases so inherited settings cannot weaken this contract.
    put(L"CLAUDE_CODE_MAX_RETRIES", L"0");
    put(L"CLAUDE_CODE_RETRY_WATCHDOG", L"0");
    put(L"CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK", L"1");
    if (!gateway.empty()) {
        // This is the private invocation-local bridge hop, not the upstream API.
        // Keep existing exclusions and proxy settings for other child/tool traffic.
        const auto existing = entries.find(L"no_proxy");
        auto bypass = existing == entries.end() ? std::wstring() :
            existing->second.substr(existing->second.find(L'=') + 1);
        if (!bypass.empty()) bypass += L",";
        put(L"NO_PROXY", bypass + L"127.0.0.1");
        // Override after aliases, using the same case-insensitive namespace.
        put(L"ANTHROPIC_BASE_URL", gateway);
        put(L"A_BASE_URL", gateway);
    }
    std::vector<std::wstring> result;
    for (const auto& item : entries) result.push_back(item.second);
    return result;
}
}
