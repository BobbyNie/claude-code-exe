#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>
#include "locked-candidate-directories.hpp"
#include "locked-candidate-file.hpp"

namespace ccode {
inline void RequireRuntimePayloadObservation(uint64_t expectedSize, const std::string& expectedHash,
                                             uint64_t observedSize, const std::string& observedHash) {
    if (!expectedSize || expectedHash.size() != 64 ||
        expectedHash.find_first_not_of("0123456789abcdef") != std::string::npos ||
        expectedSize != observedSize || expectedHash != observedHash)
        throw std::runtime_error("E_RUNTIME_INTEGRITY");
}
#ifdef _WIN32
// Returned by PrepareRuntime and retained through every actual engine use.
// The preparation lock alone is insufficient: its lifetime ends before use.
// Parents cannot be renamed, and the verified engine cannot be written/deleted
// while a child process is started and throughout its execution.
class RetainedRuntimePayload {
    std::filesystem::path path_;
    LockedCandidateDirectories parents_;
    LockedCandidateFile engine_;
public:
    RetainedRuntimePayload(const std::filesystem::path& path, uint64_t size, const std::string& hash)
        : path_(path), parents_(path.parent_path()), engine_(path) {
        try { RequireRuntimePayloadObservation(size, hash, engine_.Size(), engine_.Sha256()); }
        catch (...) { throw std::runtime_error("E_RUNTIME_INTEGRITY"); }
    }
    const std::filesystem::path& Path() const { return path_; }
};
#endif
}
