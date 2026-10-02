#pragma once
#include "locked-candidate-directories.hpp"
#include "locked-candidate-file.hpp"
#include "authenticated-candidate-files.hpp"
#include "pe-image-contract.hpp"
#ifdef _WIN32
#include <set>

namespace ccode {
enum class CandidateInventoryMode { Fresh, Installed };
// A fresh static candidate, not an already-running program tree. Runtime/data
// exclusions are not permission to carry dynamic files into a new candidate.
class LockedCandidatePackage {
    std::filesystem::path root_;
    CandidateInventoryMode mode_;
    bool runtimePresent_ = false;
    LockedCandidateDirectories ancestors_;
    LockedCandidateFile manifestFile_;
    nlohmann::json manifest_;
    std::vector<std::unique_ptr<LockedCandidateDirectories>> staticDirectories_;
    std::unique_ptr<AuthenticatedCandidateFiles<LockedCandidateFile>> files_;
    LockedCandidateFile* executable_ = nullptr; // Owned by files_, never reopened for header reads.
    void CheckInventory() const {
        std::set<std::filesystem::path> expected{
            std::filesystem::path(L"manifest.json"), std::filesystem::path(L"docs"),
            std::filesystem::path(L"notices")};
        if (mode_ == CandidateInventoryMode::Installed) {
            expected.insert(std::filesystem::path(L"manifest.sig"));
            if (runtimePresent_)
                expected.insert(std::filesystem::path(L"runtime"));
        }
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
            if (mode_ == CandidateInventoryMode::Installed && relative == std::filesystem::path(L"runtime"))
                it.disable_recursion_pending();
            it.increment(error);
            if (error) throw std::runtime_error("E_MANIFEST_INVENTORY");
        }
        if (actual != expected) throw std::runtime_error("E_MANIFEST_INVENTORY");
    }
public:
    LockedCandidatePackage(const std::filesystem::path& root,
            const std::vector<unsigned char>& signature,
            const std::vector<unsigned char>& publicKeyDer,
            const std::string& compiledTrustedPin,
            CandidateInventoryMode mode = CandidateInventoryMode::Fresh)
        : root_(root), mode_(mode), ancestors_(root), manifestFile_(root / L"manifest.json") {
        const auto bytes = manifestFile_.ReadBounded(1048576);
        manifest_ = AuthenticateManifestDocument(bytes, signature, publicKeyDer, compiledTrustedPin);
        for (const auto& name : {L"docs", L"notices"})
            staticDirectories_.push_back(std::make_unique<LockedCandidateDirectories>(root_ / name));
        if (mode_ == CandidateInventoryMode::Installed && std::filesystem::exists(root_ / L"runtime")) {
            staticDirectories_.push_back(std::make_unique<LockedCandidateDirectories>(root_ / L"runtime"));
            runtimePresent_ = true;
        }
        CheckInventory();
        files_ = std::make_unique<AuthenticatedCandidateFiles<LockedCandidateFile>>(
            bytes, signature, publicKeyDer, compiledTrustedPin,
            [&](const std::string& relative) {
                auto file = std::make_unique<LockedCandidateFile>(root_ / std::filesystem::u8path(relative));
                if (relative == "ccode.exe") executable_ = file.get();
                return file;
            });
        CheckInventory();
        VerifyEmbeddedResources();
    }
    LockedCandidatePackage(const LockedCandidatePackage&) = delete;
    LockedCandidatePackage& operator=(const LockedCandidatePackage&) = delete;
    void VerifyEmbeddedResources() const {
        if (!executable_) throw std::runtime_error("E_MANIFEST_PE");
        RequireAmd64PeImage(executable_->Size(), [&](uint64_t offset, size_t count) {
            return executable_->ReadRange(offset, count);
        });
        struct Image {
            HMODULE handle;
            ~Image() { if (handle) FreeLibrary(handle); }
        } image{LoadLibraryExW((root_ / L"ccode.exe").c_str(), nullptr,
            LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)};
        if (!image.handle) throw std::runtime_error("E_MANIFEST_RESOURCE");
        auto resource = [&](int id) {
            auto info = FindResourceW(image.handle, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
            auto loaded = info ? LoadResource(image.handle, info) : nullptr;
            auto bytes = loaded ? static_cast<const unsigned char*>(LockResource(loaded)) : nullptr;
            DWORD size = info ? SizeofResource(image.handle, info) : 0;
            if (!bytes || !size) throw std::runtime_error("E_MANIFEST_RESOURCE");
            return std::make_pair(bytes, size);
        };
        const auto metadata = resource(102);
        if (metadata.second > 1048576) throw std::runtime_error("E_MANIFEST_RESOURCE");
        nlohmann::json embedded;
        try {
            embedded = ParseManifestDocument(std::string(
                reinterpret_cast<const char*>(metadata.first), metadata.second));
        } catch (...) { throw std::runtime_error("E_MANIFEST_RESOURCE"); }
        if (!ManifestMatchesEmbeddedProvenance(manifest_, embedded))
            throw std::runtime_error("E_MANIFEST_RESOURCE");
        const auto payload = resource(101);
        if (payload.second != manifest_["provenance"]["engineSize"].get<uint64_t>())
            throw std::runtime_error("E_MANIFEST_RESOURCE");
        size_t offset = 0;
        const auto digest = StreamSha256([&](unsigned char* buffer, size_t maximum) {
            const auto count = std::min<size_t>(maximum, payload.second - offset);
            std::copy_n(payload.first + offset, count, buffer);
            offset += count;
            return count;
        });
        if (digest != manifest_["provenance"]["engineSha256"].get<std::string>())
            throw std::runtime_error("E_MANIFEST_RESOURCE");
    }
    const nlohmann::json& Manifest() const { return manifest_; }
};
}
#endif
