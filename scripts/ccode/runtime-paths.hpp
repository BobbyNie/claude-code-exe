#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ccode {
// Validate our runtime entries before opening files. This is not a defense
// against concurrent replacement by another process with the same identity.
inline void ValidateRuntimePaths(const std::filesystem::path& program, const std::string& hash) {
    namespace fs = std::filesystem;
    if (hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("E_RUNTIME_PATH");
    const auto root = program / "runtime";
    const auto runtime = root / hash;
    for (const auto& path : {root, runtime, runtime / "prepare.lock", runtime / "engine.exe", runtime / "engine.new"}) {
        std::error_code error;
        const auto status = fs::symlink_status(path, error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw std::runtime_error("E_RUNTIME_PATH");
        if (!fs::exists(status)) continue;
        bool linked = fs::is_symlink(status);
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("E_RUNTIME_PATH");
        linked = linked || (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#endif
        const bool directory = path == root || path == runtime;
        if (linked || (directory ? !fs::is_directory(status) : !fs::is_regular_file(status)))
            throw std::runtime_error("E_RUNTIME_PATH");
    }
}
}
