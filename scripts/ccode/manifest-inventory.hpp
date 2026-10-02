#pragma once
#include "vendor/json.hpp"
#include <set>
#include <vector>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace ccode {
// Parsing is not signature verification. Call only after authenticating the
// exact original bytes; schema/inventory and locked-file checks remain required.
inline nlohmann::json ParseManifestDocument(const std::string& bytes) {
    const auto reject = [] { throw std::runtime_error("E_MANIFEST_DOCUMENT"); };
    if (bytes.empty() || bytes.size() > 1048576) reject();
    using Json = nlohmann::json;
    std::vector<std::set<std::string>> keys;
    auto unique = [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 32) reject();
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        else if (event == Json::parse_event_t::object_end) keys.pop_back();
        else if (event == Json::parse_event_t::key &&
                 !keys.back().insert(value.get<std::string>()).second) reject();
        return true;
    };
    try {
        auto result = Json::parse(bytes, unique, false);
        if (result.is_discarded() || !result.is_object()) reject();
        return result;
    } catch (...) {
        reject();
    }
    throw std::runtime_error("E_MANIFEST_DOCUMENT");
}
inline bool ValidManifestFileEntry(const nlohmann::json& entry) {
    if (!entry.is_object() || entry.size() != 3 || !entry.contains("path") ||
        !entry.contains("size") || !entry.contains("sha256") ||
        !entry["path"].is_string() || !entry["sha256"].is_string() ||
        !entry["size"].is_number_integer() ||
        (!entry["size"].is_number_unsigned() && entry["size"].get<int64_t>() < 0)) return false;
    const auto hash = entry["sha256"].get<std::string>();
    if (hash.size() != 64 || !std::all_of(hash.begin(), hash.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    })) return false;
    const auto path = entry["path"].get<std::string>();
    if (path == "ccode.exe" || path == "docs/usage.md") return true;
    if (path.compare(0, 8, "notices/") != 0) return false;
    const auto name = path.substr(8);
    if (name.empty() || name.back() == '.' || name.back() == ' ') return false;
    if (std::any_of(name.begin(), name.end(), [](unsigned char c) {
        return c < 32 || c == 127 || std::string("<>:\"/\\|?*").find(c) != std::string::npos;
    })) return false;
    auto stem = name.substr(0, name.find('.'));
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    });
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 && (stem.substr(0, 3) == "com" || stem.substr(0, 3) == "lpt") &&
         stem[3] >= '1' && stem[3] <= '9')) return false;
    return true;
}

}
