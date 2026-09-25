#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace ccode {

struct ProfileRestoreResult {
    size_t copied = 0;
    size_t skipped = 0;
};

namespace profile_detail {
namespace fs = std::filesystem;

inline void CopyMissing(const fs::path& source, const fs::path& target,
                        ProfileRestoreResult& result) {
    const auto sourceStatus = fs::symlink_status(source);
    if (!fs::exists(sourceStatus)) return;
    const auto targetStatus = fs::symlink_status(target);
    // Links/junctions must not turn a local migration into writes elsewhere.
    if (fs::is_symlink(sourceStatus) || fs::is_symlink(targetStatus)) {
        ++result.skipped;
        return;
    }
    if (fs::is_directory(sourceStatus)) {
        if (fs::exists(targetStatus) && !fs::is_directory(targetStatus)) {
            ++result.skipped;
            return;
        }
        fs::create_directories(target);
        for (const auto& entry : fs::directory_iterator(source)) {
            CopyMissing(entry.path(), target / entry.path().filename(), result);
        }
    } else if (fs::is_regular_file(sourceStatus)) {
        if (fs::exists(targetStatus)) {
            ++result.skipped;
            return;
        }
        const auto timestamp = fs::last_write_time(source);
        if (fs::copy_file(source, target, fs::copy_options::skip_existing)) {
            fs::last_write_time(target, timestamp);
            ++result.copied;
        } else {
            ++result.skipped;
        }
    } else {
        ++result.skipped;
    }
}
}  // namespace profile_detail

// Copy once; originals remain available for manual conflict recovery.
inline ProfileRestoreResult RestoreLegacyProfile(const std::filesystem::path& home) {
    namespace fs = std::filesystem;
    ProfileRestoreResult result;
    const auto marker = home / ".cc-profile-restored-v1";
    if (fs::exists(fs::symlink_status(marker))) return result;
    if (!fs::exists(fs::symlink_status(home / ".cc")) &&
        !fs::exists(fs::symlink_status(home / ".cc.json"))) return result;
    profile_detail::CopyMissing(home / ".cc", home / ".claude", result);
    profile_detail::CopyMissing(home / ".cc.json", home / ".claude.json", result);
    std::ofstream completed(marker);
    completed << "Legacy profile copied without replacing existing files.\n";
    completed.close();
    if (!completed) throw std::runtime_error("Unable to record profile recovery completion");
    return result;
}

}  // namespace ccode
