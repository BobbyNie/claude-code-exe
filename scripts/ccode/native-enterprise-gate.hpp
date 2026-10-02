#pragma once
#include "locked-candidate-package.hpp"
#include <iterator>
#ifdef _WIN32
namespace ccode {
// Policy is a build-time type, never supplied by a command-line or environment
// value. Hold this object for the entire frontend/permission-worker invocation.
// The detached signature is an installed control file, not a fresh-package
// manifest entry (including its own hash would create a signing cycle).
template<class Policy> class NativeEnterpriseGate {
    LockedCandidateDirectories ancestors_;
    LockedCandidateFile signatureFile_;
    std::unique_ptr<LockedCandidatePackage> package_;
public:
    explicit NativeEnterpriseGate(const std::filesystem::path& program)
        : ancestors_(program), signatureFile_(program / L"manifest.sig") {
        const auto bytes = signatureFile_.ReadBounded(64);
        const std::vector<unsigned char> signature(bytes.begin(), bytes.end());
        const std::vector<unsigned char> publicKey(std::begin(Policy::SignerSpki), std::end(Policy::SignerSpki));
        package_ = std::make_unique<LockedCandidatePackage>(program, signature, publicKey,
            Policy::SignerPin, CandidateInventoryMode::Installed);
    }
    const nlohmann::json& Manifest() const { return package_->Manifest(); }
};
}
#endif
