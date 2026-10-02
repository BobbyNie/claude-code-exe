#pragma once
#include <filesystem>
#include <stdexcept>
#include <cwctype>
#include <memory>
#include "locked-candidate-directories.hpp"

namespace ccode {
inline bool EnterprisePathComponentEqual(const std::filesystem::path& left,
                                        const std::filesystem::path& right) {
    const auto a = left.wstring(), b = right.wstring();
#ifdef _WIN32
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()),
        b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
#else
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::towlower(a[i]) != std::towlower(b[i])) return false;
    return true;
#endif
}
inline bool EnterprisePathContains(const std::filesystem::path& parent,
                                   const std::filesystem::path& child) {
    auto p = parent.begin(), c = child.begin();
    for (; p != parent.end(); ++p, ++c)
        if (c == child.end() || !EnterprisePathComponentEqual(*p, *c)) return false;
    return true;
}
inline void RequireDisjointEnterpriseData(const std::filesystem::path& program,
                                         const std::filesystem::path& data) {
    if (!program.is_absolute() || !data.is_absolute() ||
        EnterprisePathContains(program, data) || EnterprisePathContains(data, program))
        throw std::runtime_error("E_ENTERPRISE_DATA");
}
#ifdef _WIN32
// Existing ancestors are held before creating anything. Compare their final
// Win32 paths, not the caller's spelling (case and DOS short-name aliases).
// Hold the final data root for the entire invocation; descendants still need
// their own profile/session integrity checks.
class LockedEnterpriseData {
    std::unique_ptr<LockedCandidateDirectories> existingAncestors_;
    std::unique_ptr<LockedCandidateDirectories> dataDirectories_;
public:
    LockedEnterpriseData(const std::filesystem::path& program,
                         const std::filesystem::path& data) {
        namespace fs = std::filesystem;
        try {
            RequireDisjointEnterpriseData(program, data);
            auto existing = data;
            fs::path suffix;
            for (;;) {
                std::error_code error;
                const auto status = fs::symlink_status(existing, error);
                if (error && error != std::errc::no_such_file_or_directory)
                    throw std::runtime_error("E_ENTERPRISE_DATA");
                if (fs::exists(status)) break;
                if (existing.empty() || existing == existing.root_path())
                    throw std::runtime_error("E_ENTERPRISE_DATA");
                const auto component = existing.filename().wstring();
                if (component.empty() || component == L"." || component == L".." ||
                    component.back() == L'.' || component.back() == L' ' ||
                    component.find_first_of(L"<>:\"|?*") != std::wstring::npos)
                    throw std::runtime_error("E_ENTERPRISE_DATA");
                suffix = suffix.empty() ? existing.filename() : existing.filename() / suffix;
                existing = existing.parent_path();
            }
            existingAncestors_ = std::make_unique<LockedCandidateDirectories>(existing);
            LockedCandidateDirectories programDirectories(program);
            const auto canonicalProgram = programDirectories.CanonicalPath();
            const auto canonicalData = suffix.empty() ? existingAncestors_->CanonicalPath() :
                existingAncestors_->CanonicalPath() / suffix;
            RequireDisjointEnterpriseData(canonicalProgram, canonicalData);
            // Never recursively create through an unchecked newly inserted
            // component. Lock each child before attempting the next mkdir.
            auto current = existing;
            for (const auto& component : suffix) {
                current /= component;
                fs::create_directory(current);
                auto child = std::make_unique<LockedCandidateDirectories>(current);
                dataDirectories_ = std::move(child); // new chain acquired before releasing old
            }
            if (!dataDirectories_)
                dataDirectories_ = std::make_unique<LockedCandidateDirectories>(data);
            RequireDisjointEnterpriseData(canonicalProgram, dataDirectories_->CanonicalPath());
        } catch (...) { throw std::runtime_error("E_ENTERPRISE_DATA"); }
    }
};
#endif
}
