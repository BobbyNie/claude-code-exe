#pragma once
#include <filesystem>
#include <stdexcept>
#include <memory>
#include <vector>
#ifdef _WIN32
#include <windows.h>

namespace ccode {
// Hold from the drive root outward so checked ancestors cannot be renamed or
// replaced while resolving descendants. This does not lock directory contents;
// all static files must separately be held and the inventory enumerated.
class LockedCandidateDirectories {
    struct DirectoryHandle {
        HANDLE value;
        explicit DirectoryHandle(HANDLE handle) : value(handle) {}
        ~DirectoryHandle() { CloseHandle(value); }
    };
    std::vector<std::unique_ptr<DirectoryHandle>> handles_;
    void Lock(const std::filesystem::path& path) {
        HANDLE handle = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("E_MANIFEST_DIRECTORY");
        std::unique_ptr<DirectoryHandle> owner;
        try { owner = std::make_unique<DirectoryHandle>(handle); }
        catch (...) { CloseHandle(handle); throw; }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle, &info) || GetFileType(handle) != FILE_TYPE_DISK ||
            !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("E_MANIFEST_DIRECTORY");
        handles_.push_back(std::move(owner));
    }
public:
    explicit LockedCandidateDirectories(const std::filesystem::path& candidate) {
        // Do not normalize '..' across a possible reparse point. Require an
        // explicit local-drive absolute namespace, with no traversal components.
        const auto root = candidate.root_name().wstring();
        if (!candidate.is_absolute() || root.size() != 2 || root[1] != L':' ||
            !((root[0] >= L'A' && root[0] <= L'Z') || (root[0] >= L'a' && root[0] <= L'z')))
            throw std::runtime_error("E_MANIFEST_DIRECTORY");
        auto current = candidate.root_path();
        Lock(current);
        for (const auto& component : candidate.relative_path()) {
            const auto name = component.wstring();
            if (name.empty()) continue;
            if (name == L"." || name == L".." || name.back() == L'.' || name.back() == L' ' ||
                name.find_first_of(L"<>:\"|?*") != std::wstring::npos)
                throw std::runtime_error("E_MANIFEST_DIRECTORY");
            current /= component;
            Lock(current);
        }
    }
    std::filesystem::path CanonicalPath() const {
        const auto handle = handles_.back()->value;
        const auto size = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!size || size > 32768) throw std::runtime_error("E_MANIFEST_DIRECTORY");
        std::wstring path(size, L'\0');
        const auto written = GetFinalPathNameByHandleW(handle, path.data(), size, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!written || written >= size) throw std::runtime_error("E_MANIFEST_DIRECTORY");
        path.resize(written);
        // Only local DOS drives are supported, never UNC/volume/device aliases.
        if (path.size() < 7 || path.compare(0, 4, L"\\\\?\\") != 0 || path[5] != L':' || path[6] != L'\\')
            throw std::runtime_error("E_MANIFEST_DIRECTORY");
        return std::filesystem::path(path.substr(4));
    }
    LockedCandidateDirectories(const LockedCandidateDirectories&) = delete;
    LockedCandidateDirectories& operator=(const LockedCandidateDirectories&) = delete;
};
}
#endif
