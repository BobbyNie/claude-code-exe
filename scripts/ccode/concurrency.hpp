#pragma once

#include "workspaces.hpp"
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ccode {

struct SessionTurn {
    std::string id;
    bool resume;
};

inline SessionTurn PlanSessionTurn(const std::string& current) {
    if (current.empty()) return {NewWorkspaceId(), false};
    if (!ValidSessionId(current)) throw std::runtime_error("E_SESSION_ID");
    return {current, true};
}

inline bool IsConcurrencyLink(const std::filesystem::path& path,
                              const std::filesystem::file_status& status) {
    if (std::filesystem::is_symlink(status)) return true;
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(NativeIoPath(path).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    (void)path;
    return false;
#endif
}

inline std::filesystem::path PrepareSessionLockPath(
    const std::filesystem::path& data,
    const std::filesystem::path& profile,
    const std::string& session) {
    namespace fs = std::filesystem;
    if (!ValidSessionId(session)) throw std::runtime_error("E_SESSION_ID");
    const auto dataPath = fs::absolute(data).lexically_normal();
    const auto profilePath = fs::absolute(profile).lexically_normal();
    const auto relative = profilePath.lexically_relative(dataPath);
    if (relative.empty() || relative.is_absolute()) throw std::runtime_error("E_SESSION_LOCK_PATH");
    for (const auto& component : relative)
        if (component == "..") throw std::runtime_error("E_SESSION_LOCK_PATH");
    auto profileComponent = dataPath;
    auto profileStatus = fs::symlink_status(profileComponent);
    if (!fs::is_directory(profileStatus) || IsConcurrencyLink(profileComponent, profileStatus))
        throw std::runtime_error("E_SESSION_LOCK_PATH");
    for (const auto& component : relative) {
        profileComponent /= component;
        profileStatus = fs::symlink_status(profileComponent);
        if (!fs::is_directory(profileStatus) || IsConcurrencyLink(profileComponent, profileStatus))
            throw std::runtime_error("E_SESSION_LOCK_PATH");
    }
    const auto lockRoot = profilePath.parent_path() / "session-locks";
    const auto rootStatus = fs::symlink_status(lockRoot);
    if (fs::exists(rootStatus) && (IsConcurrencyLink(lockRoot, rootStatus) || !fs::is_directory(rootStatus)))
        throw std::runtime_error("E_SESSION_LOCK_PATH");
    fs::create_directories(lockRoot);
    auto directory = lockRoot;
    const auto lockRelative = profilePath.filename();
    if (lockRelative.empty() || lockRelative == "." || lockRelative == "..")
        throw std::runtime_error("E_SESSION_LOCK_PATH");
    for (const auto& component : fs::path(lockRelative)) {
        directory /= component;
        auto status = fs::symlink_status(directory);
        if (fs::exists(status)) {
            if (IsConcurrencyLink(directory, status) || !fs::is_directory(status))
                throw std::runtime_error("E_SESSION_LOCK_PATH");
            continue;
        }
        std::error_code error;
        fs::create_directory(directory, error);
        status = fs::symlink_status(directory);
        if (error || IsConcurrencyLink(directory, status) || !fs::is_directory(status))
            throw std::runtime_error("E_SESSION_LOCK_PATH");
    }
    const auto lock = directory / (session + ".lock");
    const auto lockStatus = fs::symlink_status(lock);
    if (fs::exists(lockStatus)) {
        std::error_code error;
        const auto links = fs::hard_link_count(lock, error);
        if (IsConcurrencyLink(lock, lockStatus) || !fs::is_regular_file(lockStatus) || error || links != 1)
            throw std::runtime_error("E_SESSION_LOCK_PATH");
    }
    return lock;
}

}  // namespace ccode
