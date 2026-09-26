#pragma once
#include "sessions.hpp"
#include <functional>
#include <map>

namespace ccode {
// Retain the snapshot API while sharing native I/O conversion with history readers.
inline std::filesystem::path SnapshotIoPath(const std::filesystem::path& path) {
    return NativeIoPath(path);
}
using SnapshotDigest = std::function<std::string(const std::filesystem::path&)>;
inline bool IsOperationalProfileLock(const std::filesystem::path& relative) {
    return relative == "frontend.lock" || relative == "metadata.lock";
}
// Caller holds the exclusive profile lock for the entire operation. A snapshot
// is a verified backup, NOT an engine-validated candidate or active profile.
inline std::filesystem::path CreateProfileSnapshot(const std::filesystem::path& sourceProfile,
    const std::filesystem::path& snapshotRoot, const std::string& id, const SnapshotDigest& digest) {
    namespace fs = std::filesystem;
    const char* operation = "location";
    try {
        const auto profile = SnapshotIoPath(sourceProfile);
        const auto snapshots = SnapshotIoPath(snapshotRoot);
        if (!ValidSessionId(id)) throw std::runtime_error("E_SNAPSHOT_ID");
        const auto relative = fs::weakly_canonical(snapshots).lexically_relative(fs::canonical(profile));
        if (relative.empty() || *relative.begin() != "..") throw std::runtime_error("E_SNAPSHOT_LOCATION");
        if (fs::is_symlink(fs::symlink_status(snapshots))) throw std::runtime_error("E_SNAPSHOT_LOCATION");
        operation = "create-staging";
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
                if (IsOperationalProfileLock(name)) continue;
                if (entry.is_symlink()) throw std::runtime_error("E_SNAPSHOT_LINK");
                if (entry.is_directory()) continue;
                if (!entry.is_regular_file()) throw std::runtime_error("E_SNAPSHOT_TYPE");
                files.emplace(name.generic_u8string(), entry.path());
            }
            return files;
        };
        operation = "inventory";
        const auto files = inventory();
        Json entries = Json::object();
        operation = "create-directories";
        for (const auto& entry : fs::recursive_directory_iterator(profile)) {
            if (entry.is_directory() && !entry.is_symlink())
                fs::create_directories(copied / entry.path().lexically_relative(profile));
        }
        for (const auto& item : files) {
            operation = "hash-source";
            const auto hash = digest(item.second);
            operation = "source-size";
            const auto size = fs::file_size(item.second);
            // Manifest names use /; extended Windows I/O requires native separators.
            const auto destination = SnapshotIoPath(copied / fs::u8path(item.first));
            operation = "create-parent";
            fs::create_directories(destination.parent_path());
            operation = "copy-file";
            fs::copy_file(item.second, destination, fs::copy_options::none);
            operation = "timestamp";
            fs::last_write_time(destination, fs::last_write_time(item.second));
            operation = "verify-copy";
            if (digest(destination) != hash || digest(item.second) != hash || fs::file_size(destination) != size)
                throw std::runtime_error("E_SNAPSHOT_CHANGED");
            entries[item.first] = {{"sha256", hash}, {"size", size}};
        }
        operation = "verify-source";
        if (inventory() != files) throw std::runtime_error("E_SNAPSHOT_CHANGED");
        for (const auto& item : files)
            if (digest(item.second) != entries[item.first]["sha256"])
                throw std::runtime_error("E_SNAPSHOT_CHANGED");
        const Json manifest = {{"schema", 1}, {"snapshotId", id}, {"files", entries}};
        operation = "manifest";
        std::ofstream output(staging / "manifest.json", std::ios::binary);
        output << manifest.dump(2) << '\n';
        output.close();
        if (!output) throw std::runtime_error("E_SNAPSHOT_WRITE");
        operation = "publish";
        fs::rename(staging, target);
        return snapshotRoot / id;
    } catch (const fs::filesystem_error& error) {
        // Never expose filesystem_error::what(): it contains user paths.
        const auto& category = error.code().category();
        const char* domain = category == std::system_category() ? "system" :
            category == std::generic_category() ? "generic" : "other";
        throw std::runtime_error(std::string("E_SNAPSHOT_FS: ") + operation + ": " +
            domain + ": " + std::to_string(error.code().value()));
    }
}

inline Json VerifyProfileSnapshot(const std::filesystem::path& snapshotPath, const SnapshotDigest& digest) {
    namespace fs = std::filesystem;
    const auto snapshot = SnapshotIoPath(snapshotPath);
    const auto manifestPath = snapshot / "manifest.json";
    const auto profile = snapshot / "profile";
    for (const auto& path : {snapshot, manifestPath, profile})
        if (fs::is_symlink(fs::symlink_status(path))) throw std::runtime_error("E_SNAPSHOT_LINK");
    if (!fs::is_directory(profile)) throw std::runtime_error("E_SNAPSHOT_INTEGRITY");
    std::ifstream input(manifestPath, std::ios::binary);
    if (!input) throw std::runtime_error("E_SNAPSHOT_READ");
    const auto manifest = Json::parse(input, nullptr, false);
    if (!manifest.is_object() || !manifest.contains("schema") || manifest["schema"] != 1 ||
        !manifest.contains("snapshotId") || !manifest["snapshotId"].is_string() ||
        !ValidSessionId(manifest["snapshotId"].get<std::string>()) ||
        manifest["snapshotId"] != snapshot.filename().u8string() ||
        !manifest.contains("files") || !manifest["files"].is_object())
        throw std::runtime_error("E_SNAPSHOT_DATA");
    const auto& files = manifest["files"];
    for (const auto& item : files.items()) {
        const auto path = fs::u8path(item.key());
        if (item.key().empty() || path.is_absolute() || path.has_root_name() ||
            item.key().find('\\') != std::string::npos || item.key().find(':') != std::string::npos)
            throw std::runtime_error("E_SNAPSHOT_DATA");
        for (const auto& part : path)
            if (part == ".." || part == "." || part.empty()) throw std::runtime_error("E_SNAPSHOT_DATA");
        const auto& entry = item.value();
        if (!entry.is_object() || !entry.contains("size") || !entry["size"].is_number_unsigned() ||
            !entry.contains("sha256") || !entry["sha256"].is_string())
            throw std::runtime_error("E_SNAPSHOT_DATA");
        const auto hash = entry["sha256"].get<std::string>();
        if (hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("E_SNAPSHOT_DATA");
    }
    size_t seen = 0;
    for (const auto& entry : fs::recursive_directory_iterator(profile)) {
        if (entry.is_symlink()) throw std::runtime_error("E_SNAPSHOT_LINK");
        if (entry.is_directory()) continue;
        if (!entry.is_regular_file()) throw std::runtime_error("E_SNAPSHOT_TYPE");
        const auto name = entry.path().lexically_relative(profile).generic_u8string();
        if (!files.contains(name) || files[name]["size"] != entry.file_size() ||
            files[name]["sha256"] != digest(entry.path())) throw std::runtime_error("E_SNAPSHOT_INTEGRITY");
        ++seen;
    }
    if (seen != files.size()) throw std::runtime_error("E_SNAPSHOT_INTEGRITY");
    return manifest;
}

inline std::filesystem::path StageProfileCandidate(const std::filesystem::path& snapshotPath,
    const std::filesystem::path& candidateRoot, const std::string& id, const SnapshotDigest& digest) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(id)) throw std::runtime_error("E_SNAPSHOT_ID");
    const auto snapshot = SnapshotIoPath(snapshotPath);
    const auto candidates = SnapshotIoPath(candidateRoot);
    const auto source = VerifyProfileSnapshot(snapshot, digest);
    if (fs::is_symlink(fs::symlink_status(candidates))) throw std::runtime_error("E_SNAPSHOT_LINK");
    fs::create_directories(candidates);
    const auto target = candidates / id;
    if (fs::exists(fs::symlink_status(target))) throw std::runtime_error("E_SNAPSHOT_EXISTS");
    const auto staged = CreateProfileSnapshot(snapshot / "profile", candidates / ".staging", id, digest);
    const auto copied = VerifyProfileSnapshot(staged, digest);
    if (copied["files"] != source["files"] || VerifyProfileSnapshot(snapshot, digest) != source)
        throw std::runtime_error("E_SNAPSHOT_CHANGED");
    const Json metadata = {{"schema", 1}, {"candidateId", id},
        {"sourceSnapshotId", source["snapshotId"]}, {"state", "staged"}};
    std::ofstream output(staged / "candidate.json", std::ios::binary);
    output << metadata.dump(2) << '\n';
    output.close();
    if (!output) throw std::runtime_error("E_SNAPSHOT_WRITE");
    fs::rename(staged, target);
    return candidateRoot / id;
}

}
