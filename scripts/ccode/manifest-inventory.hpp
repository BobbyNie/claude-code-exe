#pragma once
#include "vendor/json.hpp"
#include "boundary.hpp"
#include "manifest-signature.hpp"
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

// Validate the complete static file set before opening candidate paths.
// Other root fields and filesystem identities are separate required checks.
inline bool ValidManifestInventory(const nlohmann::json& manifest) {
    if (!manifest.is_object() || !manifest.contains("files") ||
        !manifest.contains("notices") || !manifest.contains("executable") ||
        !manifest["files"].is_array() || !manifest["notices"].is_array() ||
        manifest["notices"].empty()) return false;
    std::set<std::string> paths;
    std::set<std::string> foldedPaths;
    std::string previous;
    const auto& files = manifest["files"];
    for (const auto& entry : files) {
        if (!ValidManifestFileEntry(entry)) return false;
        const auto path = entry["path"].get<std::string>();
        if (!previous.empty() && previous >= path) return false;
        previous = path;
        if (!paths.insert(path).second) return false;
        auto folded = path;
        std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) {
            return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        });
        if (!foldedPaths.insert(folded).second) return false;
    }
    if (!paths.count("ccode.exe") || !paths.count("docs/usage.md") ||
        files.size() != manifest["notices"].size() + 2) return false;
    previous.clear();
    for (const auto& notice : manifest["notices"]) {
        if (!ValidManifestFileEntry(notice)) return false;
        const auto path = notice["path"].get<std::string>();
        if (path.compare(0, 8, "notices/") != 0 ||
            (!previous.empty() && previous >= path)) return false;
        previous = path;
        auto entry = std::find_if(files.begin(), files.end(), [&](const auto& candidate) {
            return candidate["path"] == path;
        });
        if (entry == files.end() || entry->dump() != notice.dump()) return false;
    }
    auto executable = std::find_if(files.begin(), files.end(), [](const auto& entry) {
        return entry["path"] == "ccode.exe";
    });
    return executable != files.end() && executable->dump() == manifest["executable"].dump();
}

// Root contract only: provenance and runtimeBoundary need independent validation.
inline bool ValidManifestRootContract(const nlohmann::json& manifest) {
    using Json = nlohmann::json;
    const std::set<std::string> required = {"schemaVersion", "packageName", "packageVersion",
        "platform", "architecture", "minimumWindowsBuild", "executable", "provenance",
        "runtimeBoundary", "files", "notices", "publicBoundary", "excludedDynamicData",
        "redistributionApproval"};
    if (!manifest.is_object() || manifest.size() != required.size()) return false;
    for (const auto& key : required) if (!manifest.contains(key)) return false;
    if (!manifest["schemaVersion"].is_number_integer() || manifest["schemaVersion"].dump() != "1" ||
        !manifest["minimumWindowsBuild"].is_number_integer() || manifest["minimumWindowsBuild"].dump() != "22000" ||
        manifest["packageName"] != "ccode-enterprise" || manifest["platform"] != "windows" ||
        manifest["architecture"] != "x64" || !manifest["packageVersion"].is_string() ||
        manifest["packageVersion"].get<std::string>().empty() ||
        !manifest["provenance"].is_object() || !manifest["runtimeBoundary"].is_object() ||
        manifest["redistributionApproval"] != "external-gate-not-asserted" ||
        manifest["excludedDynamicData"].dump() != Json({"data/", "profile/", "runtime/", "sessions/", "temp/"}).dump() ||
        !ValidManifestInventory(manifest)) return false;
    auto scanned = Json::array({"docs/usage.md", "manifest.json"});
    for (const auto& entry : manifest["notices"]) scanned.push_back(entry["path"]);
    const Json expected = {{"opaqueContents", Json::array({"ccode.exe"})},
        {"scannedText", scanned}, {"parentDirectories", "excluded"}};
    return manifest["publicBoundary"].dump() == expected.dump();
}

inline bool ValidManifestProvenanceContract(const nlohmann::json& manifest) {
    if (!ValidManifestRootContract(manifest)) return false;
    const auto& source = manifest["provenance"];
    const std::set<std::string> required = {"schemaVersion", "packageName", "packageVersion",
        "adapterRevision", "engineVersion", "engineSha256", "engineSize",
        "officialManifestUrl", "officialManifestSha256", "officialPayloadUrl"};
    if (source.size() != required.size()) return false;
    for (const auto& key : required) if (!source.contains(key)) return false;
    auto hex = [](const nlohmann::json& value, size_t length) {
        if (!value.is_string()) return false;
        const auto text = value.get<std::string>();
        return text.size() == length && std::all_of(text.begin(), text.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    };
    if (!source["schemaVersion"].is_number_integer() || source["schemaVersion"].dump() != "1" ||
        source["packageName"] != "ccode" || source["packageVersion"] != "1.0" ||
        source["engineVersion"] != manifest["packageVersion"] ||
        !(hex(source["adapterRevision"], 40) || hex(source["adapterRevision"], 64)) ||
        !hex(source["engineSha256"], 64) || !hex(source["officialManifestSha256"], 64) ||
        !source["engineSize"].is_number_integer() ||
        (!source["engineSize"].is_number_unsigned() && source["engineSize"].get<int64_t>() <= 0) ||
        (source["engineSize"].is_number_unsigned() && source["engineSize"].get<uint64_t>() == 0) ||
        manifest["runtimeBoundary"].dump() != BoundaryManifest().dump()) return false;
    const auto version = manifest["packageVersion"].get<std::string>();
    // Version is one URL path segment, not candidate-controlled URL syntax.
    if (!std::all_of(version.begin(), version.end(), [](char c) {
        return (c >= '0' && c <= '9') || c == '.';
    }) || version.front() == '.' || version.back() == '.') return false;
    const auto base = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/" + version;
    return source["officialManifestUrl"] == base + "/manifest.json" &&
           source["officialPayloadUrl"] == base + "/win32-x64/claude.exe";
}

// Authenticate original bytes before interpreting any candidate-supplied paths.
// The pin must originate from independently approved compiled policy.
// This has no filesystem side effects; callers still must lock/verify files.
inline nlohmann::json AuthenticateManifestDocument(const std::string& originalBytes,
        const std::vector<unsigned char>& signature,
        const std::vector<unsigned char>& publicKeyDer, const std::string& trustedPin) {
    if (!VerifyManifestSignature(std::vector<unsigned char>(originalBytes.begin(), originalBytes.end()),
                                 signature, publicKeyDer, trustedPin))
        throw std::runtime_error("E_MANIFEST_SIGNATURE");
    auto document = ParseManifestDocument(originalBytes);
    if (!ValidManifestProvenanceContract(document))
        throw std::runtime_error("E_MANIFEST_SCHEMA");
    return document;
}

// Metadata must come from the locked executable's resource, not a sidecar or
// candidate subprocess. This comparison does not itself load/hash the payload.
inline bool ManifestMatchesEmbeddedProvenance(const nlohmann::json& manifest,
                                             const nlohmann::json& embedded) {
    if (!ValidManifestProvenanceContract(manifest) || !embedded.is_object()) return false;
    auto expected = manifest["provenance"];
    expected["platform"] = "windows";
    expected["architecture"] = "x64";
    return embedded.dump() == expected.dump();
}

}
