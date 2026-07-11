#pragma once

#include <algorithm>
#include <cwctype>
#include <string>

namespace ccode {

inline std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

inline bool StartsWithInsensitive(const std::wstring& value, const std::wstring& prefix) {
    return value.size() >= prefix.size() &&
           Lower(value.substr(0, prefix.size())) == Lower(prefix);
}

inline bool ContainsForbiddenText(const std::wstring& value) {
    const std::wstring lower = Lower(value);
    return lower.find(L"anthropic") != std::wstring::npos ||
           lower.find(L"claude") != std::wstring::npos;
}

inline std::wstring MapEnvironmentName(const std::wstring& requested) {
    const std::wstring aPrefix = L"ANTHROPIC_";
    const std::wstring cPrefix = L"CLAUDE_CODE_";
    if (StartsWithInsensitive(requested, aPrefix)) {
        return L"A_" + requested.substr(aPrefix.size());
    }
    if (StartsWithInsensitive(requested, cPrefix)) {
        return L"C_" + requested.substr(cPrefix.size());
    }
    return requested;
}

inline bool HasApiCredential(const std::wstring& apiKey, const std::wstring& authToken) {
    return !apiKey.empty() || !authToken.empty();
}

inline void ReplaceInsensitive(std::wstring& value, const std::wstring& needle,
                               const std::wstring& replacement) {
    size_t position = 0;
    while (position < value.size()) {
        const std::wstring lower = Lower(value);
        const size_t found = lower.find(Lower(needle), position);
        if (found == std::wstring::npos) {
            return;
        }
        value.replace(found, needle.size(), replacement);
        position = found + replacement.size();
    }
}

inline std::wstring RewritePath(std::wstring path) {
    ReplaceInsensitive(path, L"anthropic", L"aa");
    ReplaceInsensitive(path, L"claude", L"cc");
    return path;
}

inline bool IsValidGatewayUrl(const std::wstring& url) {
    return !url.empty();
}

inline std::wstring GatewayHost(const std::wstring& url) {
    const size_t scheme = url.find(L"://");
    if (scheme == std::wstring::npos) return {};
    const size_t start = scheme + 3;
    const size_t end = url.find_first_of(L"/:?#", start);
    const std::wstring host = url.substr(start, end == std::wstring::npos ? end : end - start);
    return host.empty() ? std::wstring() : Lower(host);
}

inline bool IsAllowedNetworkHost(const std::wstring& requested, const std::wstring& gatewayUrl) {
    const std::wstring gateway = GatewayHost(gatewayUrl);
    return !gateway.empty() && Lower(requested) == gateway && !ContainsForbiddenText(requested);
}

}  // namespace ccode
