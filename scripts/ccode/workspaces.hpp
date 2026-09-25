#pragma once
#include "sessions.hpp"
#include <array>
#include <random>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ccode {
inline std::string WorkspaceKey(const std::filesystem::path& workspace) {
    auto canonical = std::filesystem::canonical(workspace);
#ifdef _WIN32
    canonical = std::filesystem::path(Lower(canonical.wstring()));
#endif
    return canonical.u8string();
}
inline std::string NewWorkspaceId() {
    std::random_device random;
    std::array<unsigned char, 16> bytes{};
    for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
    bytes[6] = (bytes[6] & 15) | 64;
    bytes[8] = (bytes[8] & 63) | 128;
    const char* hex = "0123456789abcdef";
    std::string id;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) id += '-';
        id += hex[bytes[i] >> 4]; id += hex[bytes[i] & 15];
    }
    return id;
}
// Caller holds the profile lock. This registry is persistent identity metadata,
// not a substitute for authoritative engine transcripts. Runtime paths never
// participate in identity. Relocating the workspace itself needs explicit migration.
inline std::string ResolveWorkspace(const std::filesystem::path& registry,
                                    const std::filesystem::path& workspace) {
    namespace fs = std::filesystem;
    const auto key = WorkspaceKey(workspace);
    Json document = {{"schema", 1}, {"workspaces", Json::object()}};
    if (fs::exists(registry)) {
        std::ifstream input(registry, std::ios::binary);
        if (!input) throw std::runtime_error("E_WORKSPACE_READ");
        document = Json::parse(input, nullptr, false);
        if (!document.is_object() || !document.contains("schema") || document["schema"] != 1 ||
            !document.contains("workspaces") || !document["workspaces"].is_object())
            throw std::runtime_error("E_WORKSPACE_DATA");
        std::set<std::string> ids;
        for (const auto& item : document["workspaces"].items()) {
            if (item.key().empty() || !item.value().is_string() ||
                !ValidSessionId(item.value().get<std::string>()) ||
                !ids.insert(item.value().get<std::string>()).second)
                throw std::runtime_error("E_WORKSPACE_DATA");
        }
    }
    auto& entries = document["workspaces"];
    if (entries.contains(key)) return entries[key].get<std::string>();
    std::string id;
    bool duplicate;
    do {
        id = NewWorkspaceId();
        duplicate = false;
        for (const auto& value : entries) if (value == id) duplicate = true;
    } while (duplicate);
    entries[key] = id;
    fs::create_directories(registry.parent_path());
    auto candidate = registry; candidate += ".new";
    if (fs::is_symlink(fs::symlink_status(candidate))) throw std::runtime_error("E_WORKSPACE_WRITE");
    {
        std::ofstream output(candidate, std::ios::binary | std::ios::trunc);
        output << document.dump(2) << '\n';
        output.close();
        if (!output) throw std::runtime_error("E_WORKSPACE_WRITE");
    }
#ifdef _WIN32
    if (!MoveFileExW(candidate.c_str(), registry.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("E_WORKSPACE_WRITE");
#else
    std::error_code error;
    fs::rename(candidate, registry, error);
    if (error) throw std::runtime_error("E_WORKSPACE_WRITE");
#endif
    return id;
}
}
