#pragma once
#include "sessions.hpp"
#include <functional>
#include <map>

namespace ccode {
using SnapshotDigest = std::function<std::string(const std::filesystem::path&)>;
// Caller holds the exclusive profile lock for the entire operation. A snapshot
// is a verified backup, NOT an engine-validated candidate or active profile.
inline std::filesystem::path CreateProfileSnapshot(const std::filesystem::path& profile,
    const std::filesystem::path& snapshots, const std::string& id, const SnapshotDigest& digest) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(id)) throw std::runtime_error("E_SNAPSHOT_ID");
    const auto relative = fs::weakly_canonical(snapshots).lexically_relative(fs::canonical(profile));
    if (relative.empty() || *relative.begin() != "..") throw std::runtime_error("E_SNAPSHOT_LOCATION");
    if (fs::is_symlink(fs::symlink_status(snapshots))) throw std::runtime_error("E_SNAPSHOT_LOCATION");
    fs::create_directories(snapshots);
    const auto target = snapshots / id;
    const auto staging = snapshots / (id + ".pending");
    if (fs::exists(fs::symlink_status(target)) || !fs::create_directory(staging))
        throw std::runtime_error("E_SNAPSHOT_EXISTS");
    // Incomplete candidates are deliberately retained, never reused or activated.
    // No failure path deletes source data or a previous snapshot.
    const auto copied = staging / "profile";
    fs::create_directory(copied);
    auto inventory = [&]() {
        std::map<std::string, fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(profile)) {
            auto name = entry.path().lexically_relative(profile);
            if (name == "frontend.lock") continue;
            if (entry.is_symlink()) throw std::runtime_error("E_SNAPSHOT_LINK");
            if (entry.is_directory()) continue;
            if (!entry.is_regular_file()) throw std::runtime_error("E_SNAPSHOT_TYPE");
            files.emplace(name.generic_u8string(), entry.path());
        }
        return files;
    };
    const auto files = inventory();
    Json entries = Json::object();
    for (const auto& entry : fs::recursive_directory_iterator(profile)) {
        if (entry.is_directory() && !entry.is_symlink())
            fs::create_directories(copied / entry.path().lexically_relative(profile));
    }
    for (const auto& item : files) {
        const auto hash = digest(item.second);
        const auto size = fs::file_size(item.second);
        const auto destination = copied / fs::u8path(item.first);
        fs::create_directories(destination.parent_path());
        fs::copy_file(item.second, destination, fs::copy_options::none);
        fs::last_write_time(destination, fs::last_write_time(item.second));
        if (digest(destination) != hash || digest(item.second) != hash || fs::file_size(destination) != size)
            throw std::runtime_error("E_SNAPSHOT_CHANGED");
        entries[item.first] = {{"sha256", hash}, {"size", size}};
    }
    if (inventory() != files) throw std::runtime_error("E_SNAPSHOT_CHANGED");
    for (const auto& item : files)
        if (digest(item.second) != entries[item.first]["sha256"])
            throw std::runtime_error("E_SNAPSHOT_CHANGED");
    const Json manifest = {{"schema", 1}, {"snapshotId", id}, {"files", entries}};
    std::ofstream output(staging / "manifest.json", std::ios::binary);
    output << manifest.dump(2) << '\n';
    output.close();
    if (!output) throw std::runtime_error("E_SNAPSHOT_WRITE");
    fs::rename(staging, target);
    return target;
}
}
