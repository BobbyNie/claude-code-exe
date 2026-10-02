#pragma once
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include "locked-candidate-directories.hpp"
#include "extraction-errors.hpp"

namespace ccode {
inline void RequireRuntimeStagingObservation(bool disk, bool directory, bool reparse, unsigned links) {
    if (!disk || directory || reparse || links != 1) throw std::runtime_error("E_RUNTIME_PATH");
}
#ifdef _WIN32
// Never truncate a pathname before verifying its opened object. Keep the source
// handle alive through activation; a final retained reader verifies after close.
class RuntimeStagingFile {
    LockedCandidateDirectories parents_;
    HANDLE file_ = INVALID_HANDLE_VALUE;
public:
    explicit RuntimeStagingFile(const std::filesystem::path& directory) : parents_(directory) {
        file_ = CreateFileW((directory / L"engine.new").c_str(), GENERIC_READ | GENERIC_WRITE | DELETE,
            FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) throw std::runtime_error(ExtractionActivationError(GetLastError()));
        try {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(file_, &info)) throw std::runtime_error("E_RUNTIME_PATH");
            RequireRuntimeStagingObservation(GetFileType(file_) == FILE_TYPE_DISK,
                (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
                (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0, info.nNumberOfLinks);
        } catch (...) { CloseHandle(file_); file_ = INVALID_HANDLE_VALUE; throw; }
    }
    ~RuntimeStagingFile() { if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_); }
    RuntimeStagingFile(const RuntimeStagingFile&) = delete;
    RuntimeStagingFile& operator=(const RuntimeStagingFile&) = delete;
    void Write(const unsigned char* bytes, size_t size) {
        LARGE_INTEGER zero{};
        if (!SetFilePointerEx(file_, zero, nullptr, FILE_BEGIN) || !SetEndOfFile(file_))
            throw std::runtime_error(ExtractionWriteError(GetLastError()));
        for (size_t offset = 0; offset < size;) {
            const DWORD count = static_cast<DWORD>((size - offset) > 65536 ? 65536 : size - offset);
            DWORD written = 0;
            if (!WriteFile(file_, bytes + offset, count, &written, nullptr))
                throw std::runtime_error(ExtractionWriteError(GetLastError()));
            if (written != count) throw std::runtime_error("E_EXTRACT_WRITE");
            offset += written;
        }
        if (!FlushFileBuffers(file_)) throw std::runtime_error(ExtractionWriteError(GetLastError()));
    }
    void Activate() {
        const auto destination = (parents_.CanonicalPath() / L"engine.exe").wstring();
        // Do not rely on replacement rename to enforce a live reader's share
        // policy. Explicitly request DELETE on the existing target first.
        struct TargetOwner {
            HANDLE value;
            ~TargetOwner() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
        } target{CreateFileW(destination.c_str(), GENERIC_READ | DELETE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
        if (target.value == INVALID_HANDLE_VALUE) {
            const auto error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND)
                throw std::runtime_error(ExtractionActivationError(error));
        } else {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(target.value, &info))
                throw std::runtime_error("E_RUNTIME_PATH");
            RequireRuntimeStagingObservation(GetFileType(target.value) == FILE_TYPE_DISK,
                (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
                (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0, info.nNumberOfLinks);
        }
        const auto nameBytes = destination.size() * sizeof(wchar_t);
        const auto length = sizeof(FILE_RENAME_INFO) + nameBytes + sizeof(wchar_t);
        // operator new supplies alignment suitable for FILE_RENAME_INFO.
        void* storage = ::operator new(length);
        auto* rename = new(storage) FILE_RENAME_INFO{};
        rename->ReplaceIfExists = TRUE;
        rename->RootDirectory = nullptr;
        rename->FileNameLength = static_cast<DWORD>(nameBytes);
        // FileNameLength excludes NUL, but provide an explicit terminator too:
        // never let the Win32 path conversion observe uninitialized tail bytes.
        std::memcpy(rename->FileName, destination.c_str(), nameBytes + sizeof(wchar_t));
        const bool activated = SetFileInformationByHandle(file_, FileRenameInfo, rename,
            static_cast<DWORD>(length)) != FALSE;
        const auto error = activated ? ERROR_SUCCESS : GetLastError();
        rename->~FILE_RENAME_INFO();
        ::operator delete(storage);
        if (!activated) throw std::runtime_error(ExtractionActivationError(error));
    }
};
#endif
}
