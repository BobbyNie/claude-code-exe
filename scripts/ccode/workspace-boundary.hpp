#pragma once
#include <cwctype>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace ccode {
inline bool WorkspacePrefixEqual(const std::wstring& value, const std::wstring& prefix) {
    if (value.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (std::towlower(value[i]) != std::towlower(prefix[i])) return false;
    return true;
}

// Windows cannot create a child process whose current directory exceeds the
// legacy MAX_PATH current-directory boundary. Keep one character available
// for the separator that Windows may append before the terminating NUL.
inline void ValidateWorkspaceBoundary(const std::filesystem::path& workspace) {
    const auto value = workspace.wstring();
    if (WorkspacePrefixEqual(value, L"\\\\?\\UNC\\") ||
        (value.rfind(L"\\\\", 0) == 0 && !WorkspacePrefixEqual(value, L"\\\\?\\") &&
         !WorkspacePrefixEqual(value, L"\\\\.\\")))
        throw std::runtime_error("E_WORKSPACE_UNSUPPORTED");
    if (WorkspacePrefixEqual(value, L"\\\\?\\") || WorkspacePrefixEqual(value, L"\\\\.\\"))
        throw std::runtime_error("E_WORKSPACE_PATH");
    if (value.size() > 258)
        throw std::runtime_error("E_WORKSPACE_PATH_TOO_LONG");
}

inline std::filesystem::path ResolveWorkspaceSelection(const std::filesystem::path& invocationDirectory,
                                                       const std::filesystem::path& requested) {
    if (!requested.empty()) ValidateWorkspaceBoundary(requested);
    auto selected = (requested.empty() || requested.is_absolute())
        ? (requested.empty() ? invocationDirectory : requested)
        : invocationDirectory / requested;
    selected = selected.lexically_normal();
    ValidateWorkspaceBoundary(selected);
    std::error_code error;
    if (!std::filesystem::is_directory(selected, error) || error)
        throw std::runtime_error("E_WORKSPACE_PATH");
    return selected;
}
}
