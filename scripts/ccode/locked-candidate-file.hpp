#pragma once
#include <filesystem>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <algorithm>
#include "stream-sha256.hpp"
#include "manifest-inventory.hpp"
#ifdef _WIN32
#include <windows.h>

namespace ccode {
// Retain through candidate use. Denies concurrent writes/deletes to this file;
// callers must separately secure its parent directories and candidate identity.
class LockedCandidateFile {
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    uint64_t size_ = 0;
public:
    explicit LockedCandidateFile(const std::filesystem::path& path) {
        handle_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) throw std::runtime_error("E_MANIFEST_FILE");
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle_, &info) ||
            GetFileType(handle_) != FILE_TYPE_DISK ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
            info.nNumberOfLinks != 1) {
            CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE;
            throw std::runtime_error("E_MANIFEST_FILE");
        }
        size_ = (uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    }
    ~LockedCandidateFile() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    LockedCandidateFile(const LockedCandidateFile&) = delete;
    LockedCandidateFile& operator=(const LockedCandidateFile&) = delete;
    uint64_t Size() const { return size_; }
    std::string Sha256() {
        LARGE_INTEGER start{};
        if (!SetFilePointerEx(handle_, start, nullptr, FILE_BEGIN))
            throw std::runtime_error("E_MANIFEST_FILE");
        uint64_t remaining = size_;
        return StreamSha256([&](unsigned char* buffer, size_t capacity) -> size_t {
            if (!remaining) return 0;
            const auto count = static_cast<DWORD>(std::min<uint64_t>(remaining, capacity));
            DWORD read = 0;
            if (!ReadFile(handle_, buffer, count, &read, nullptr) || !read)
                throw std::runtime_error("E_MANIFEST_FILE");
            remaining -= read;
            return read;
        });
    }
    void Verify(const nlohmann::json& entry) {
        if (!ValidManifestFileEntry(entry) || entry["size"].get<uint64_t>() != Size())
            throw std::runtime_error("E_MANIFEST_FILE_MISMATCH");
        if (!ManifestFileMatchesObservation(entry, Size(), Sha256()))
            throw std::runtime_error("E_MANIFEST_FILE_MISMATCH");
    }
    std::string ReadRange(uint64_t offset, size_t count) {
        if (count > 4096 || offset > size_ || count > size_ - offset ||
            offset > uint64_t(INT64_MAX))
            throw std::runtime_error("E_MANIFEST_FILE_LIMIT");
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(handle_, position, nullptr, FILE_BEGIN))
            throw std::runtime_error("E_MANIFEST_FILE");
        std::string bytes(count, '\0');
        size_t consumed = 0;
        while (consumed < count) {
            DWORD read = 0;
            if (!ReadFile(handle_, &bytes[consumed], static_cast<DWORD>(count - consumed), &read, nullptr) || !read)
                throw std::runtime_error("E_MANIFEST_FILE");
            consumed += read;
        }
        return bytes;
    }
    std::string ReadBounded(size_t maximum) {
        if (size_ > maximum) throw std::runtime_error("E_MANIFEST_FILE_LIMIT");
        LARGE_INTEGER start{};
        if (!SetFilePointerEx(handle_, start, nullptr, FILE_BEGIN))
            throw std::runtime_error("E_MANIFEST_FILE");
        std::string bytes(static_cast<size_t>(size_), '\0');
        size_t offset = 0;
        while (offset < bytes.size()) {
            DWORD read = 0;
            const auto count = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 65536));
            if (!ReadFile(handle_, &bytes[offset], count, &read, nullptr) || read == 0)
                throw std::runtime_error("E_MANIFEST_FILE");
            offset += read;
        }
        return bytes;
    }
};
}
#endif
