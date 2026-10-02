#pragma once
#include "locked-candidate-directories.hpp"
#include "locked-candidate-file.hpp"
#include "authenticated-candidate-files.hpp"
#ifdef _WIN32
#include <set>

namespace ccode {
// A fresh static candidate, not an already-running program tree. Runtime/data
// exclusions are not permission to carry dynamic files into a new candidate.
class LockedCandidatePackage {
    std::filesystem::path root_;
    LockedCandidateDirectories ancestors_;
    LockedCandidateFile manifestFile_;
    nlohmann::json manifest_;
    std::vector<std::unique_ptr<LockedCandidateDirectories>> staticDirectories_;
    std::unique_ptr<AuthenticatedCandidateFiles<LockedCandidateFile>> files_;
    void CheckInventory() const {
        std::set<std::filesystem::path> expected{
            std::filesystem::path(L"manifest.json"), std::filesystem::path(L"docs"),
            std::filesystem::path(L"notices")};
        for (const auto& entry : manifest_["files"])
            expected.insert(std::filesystem::u8path(entry["path"].get<std::string>()));
        std::set<std::filesystem::path> actual;
        std::error_code error;
        std::filesystem::recursive_directory_iterator it(root_, error), end;
        if (error) throw std::runtime_error("E_MANIFEST_INVENTORY");
        while (it != end) {
            const auto relative = it->path().lexically_relative(root_);
            // Check before descending: never traverse an unexpected directory.
            if (!expected.count(relative)) throw std::runtime_error("E_MANIFEST_INVENTORY");
            actual.insert(relative);
            it.increment(error);
            if (error) throw std::runtime_error("E_MANIFEST_INVENTORY");
        }
        if (actual != expected) throw std::runtime_error("E_MANIFEST_INVENTORY");
    }
public:
    LockedCandidatePackage(const std::filesystem::path& root,
            const std::vector<unsigned char>& signature,
            const std::vector<unsigned char>& publicKeyDer,
            const std::string& compiledTrustedPin)
        : root_(root), ancestors_(root), manifestFile_(root / L"manifest.json") {
        const auto bytes = manifestFile_.ReadBounded(1048576);
        manifest_ = AuthenticateManifestDocument(bytes, signature, publicKeyDer, compiledTrustedPin);
        for (const auto& name : {L"docs", L"notices"})
            staticDirectories_.push_back(std::make_unique<LockedCandidateDirectories>(root_ / name));
        CheckInventory();
        files_ = std::make_unique<AuthenticatedCandidateFiles<LockedCandidateFile>>(
            bytes, signature, publicKeyDer, compiledTrustedPin,
            [&](const std::string& relative) {
                return std::make_unique<LockedCandidateFile>(root_ / std::filesystem::u8path(relative));
            });
        CheckInventory();
    }
    LockedCandidatePackage(const LockedCandidatePackage&) = delete;
    LockedCandidatePackage& operator=(const LockedCandidatePackage&) = delete;
    const nlohmann::json& Manifest() const { return manifest_; }
};
}
#endif
