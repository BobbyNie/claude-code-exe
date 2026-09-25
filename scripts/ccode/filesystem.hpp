#pragma once
#include <filesystem>
#include <stdexcept>

namespace ccode {
// Use extended-length paths only for local filesystem I/O. Do not alter stored
// relative names, engine cwd, or the caller-visible snapshot/candidate path.
inline std::filesystem::path NativeIoPath(const std::filesystem::path& path) {
#ifdef _WIN32
    auto absolute = std::filesystem::absolute(path).lexically_normal();
    absolute.make_preferred();
    const auto value = absolute.native();
    if (value.rfind(L"\\\\?\\", 0) == 0) return absolute;
    if (value.rfind(L"\\\\.\\", 0) == 0) throw std::runtime_error("E_SNAPSHOT_LOCATION");
    if (value.rfind(L"\\\\", 0) == 0) return std::filesystem::path(L"\\\\?\\UNC\\" + value.substr(2));
    return std::filesystem::path(L"\\\\?\\" + value);
#else
    return path;
#endif
}
}
