#pragma once
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string_view>
#include "locked-candidate-directories.hpp"
#include "extraction-errors.hpp"
#ifdef _WIN32
#include <winternl.h>
#endif

namespace ccode {
inline void RequireRuntimeStagingObservation(bool disk, bool directory, bool reparse, unsigned links) {
    if (!disk || directory || reparse || links != 1) throw std::runtime_error("E_RUNTIME_PATH");
}
inline bool ShouldRetryRuntimeActivation(std::string_view code, uint64_t elapsed, uint64_t budget) {
    return elapsed < budget && (code == "E_EXTRACT_ACCESS" ||
        code == "E_EXTRACT_SHARING" || code == "E_EXTRACT_LOCKED");
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
    void Activate(DWORD waitMilliseconds = 0) {
        // Keep source and parent guards throughout bounded contention recovery.
        const auto budget = waitMilliseconds > 30000 ? 30000 : waitMilliseconds;
        const auto started = GetTickCount64();
        for (;;) {
            try { ActivateOnce(); return; }
            catch (const std::runtime_error& error) {
                const auto elapsed = GetTickCount64() - started;
                if (!ShouldRetryRuntimeActivation(error.what(), elapsed, budget)) throw;
                const auto remaining = budget - elapsed;
                Sleep(static_cast<DWORD>(remaining < 50 ? remaining : 50));
            }
        }
    }
private:
    void ActivateOnce() {
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
        // Classic replacement cannot replace a target with an outstanding
        // probe handle. Release this probe before rename; retain source/parents.
        // The probe is not an atomic target reservation. Native classic rename
        // and the final retained hash remain required before engine execution.
        if (target.value != INVALID_HANDLE_VALUE) {
            const auto handle = target.value;
            target.value = INVALID_HANDLE_VALUE;
            if (!CloseHandle(handle)) throw std::runtime_error("E_EXTRACT_ACTIVATE");
        }
        // Resolve the fixed sibling against the retained directory object, not
        // a second full-DOS-path conversion inside the rename operation.
        const std::wstring leaf = L"engine.exe";
        const auto nameBytes = leaf.size() * sizeof(wchar_t);
        const auto length = sizeof(FILE_RENAME_INFO) + nameBytes + sizeof(wchar_t);
        // operator new supplies alignment suitable for FILE_RENAME_INFO.
        void* storage = ::operator new(length);
        auto* rename = new(storage) FILE_RENAME_INFO{};
        rename->ReplaceIfExists = TRUE;
        rename->RootDirectory = parents_.BorrowedLeafHandle();
        rename->FileNameLength = static_cast<DWORD>(nameBytes);
        // FileNameLength excludes NUL, but provide an explicit terminator too:
        // never let the Win32 path conversion observe uninitialized tail bytes.
        std::memcpy(rename->FileName, leaf.c_str(), nameBytes + sizeof(wchar_t));
        // Pass the retained root directly through the native directory-relative
        // rename contract, avoiding the Win32 pathname conversion layer.
        using SetInformation = NTSTATUS (NTAPI *)(HANDLE, PIO_STATUS_BLOCK, PVOID,
                                                   ULONG, FILE_INFORMATION_CLASS);
        using StatusToError = ULONG (NTAPI *)(NTSTATUS);
        const auto module = GetModuleHandleW(L"ntdll.dll");
        const auto setInformation = reinterpret_cast<SetInformation>(
            GetProcAddress(module, "NtSetInformationFile"));
        const auto statusToError = reinterpret_cast<StatusToError>(
            GetProcAddress(module, "RtlNtStatusToDosError"));
        IO_STATUS_BLOCK io{};
        // FILE_INFORMATION_CLASS::FileRenameInformation = 10. FILE_RENAME_INFO
        // has the same BOOLEAN/root/length/name layout for the classic rename.
        const auto status = setInformation && statusToError
            ? setInformation(file_, &io, rename, static_cast<ULONG>(length),
                             static_cast<FILE_INFORMATION_CLASS>(10))
            : static_cast<NTSTATUS>(0xC0000002L);
        const bool activated = status == 0;
        const auto error = activated ? ERROR_SUCCESS
            : statusToError ? statusToError(status) : ERROR_NOT_SUPPORTED;
        rename->~FILE_RENAME_INFO();
        ::operator delete(storage);
        if (!activated) throw std::runtime_error(ExtractionActivationError(error));
    }
};
#endif
}
