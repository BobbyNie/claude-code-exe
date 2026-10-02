#pragma once
#include "manifest-inventory.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace ccode {
// The trusted opener must secure candidate/parent identity before resolving
// paths and return retained read locks. This owns all listed static files;
// it is not an enumeration, parent-lock, embedded-resource or startup gate.
template<class File> class AuthenticatedCandidateFiles {
    nlohmann::json manifest_;
    std::vector<std::unique_ptr<File>> files_;
public:
    template<class Opener>
    AuthenticatedCandidateFiles(const std::string& originalBytes,
            const std::vector<unsigned char>& signature,
            const std::vector<unsigned char>& publicKeyDer,
            const std::string& compiledTrustedPin, Opener&& open)
        : manifest_(AuthenticateManifestDocument(originalBytes, signature,
                                                 publicKeyDer, compiledTrustedPin)) {
        for (const auto& entry : manifest_["files"]) {
            auto file = open(entry["path"].template get<std::string>());
            if (!file) throw std::runtime_error("E_MANIFEST_FILE");
            files_.push_back(std::move(file));
        }
        // Acquire the entire listed set before reading any file content.
        for (size_t i = 0; i < files_.size(); ++i)
            files_[i]->Verify(manifest_["files"][i]);
    }
    AuthenticatedCandidateFiles(const AuthenticatedCandidateFiles&) = delete;
    AuthenticatedCandidateFiles& operator=(const AuthenticatedCandidateFiles&) = delete;
    const nlohmann::json& Manifest() const { return manifest_; }
};
}
