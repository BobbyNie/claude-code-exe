#pragma once
#include "vendor/json.hpp"
#include <set>
#include <vector>
#include <stdexcept>
#include <string>

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
}
