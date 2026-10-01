#pragma once
#include "vendor/monocypher/monocypher-ed25519.h"
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif

namespace ccode {
// Native primitive only. A caller must independently approve/pin the signer
// and validate the candidate before execution; this is not yet a startup gate.
inline bool VerifyEd25519(const std::vector<unsigned char>& message,
                          const std::vector<unsigned char>& signature,
                          const std::vector<unsigned char>& publicKey) {
    if (signature.size() != 64 || publicKey.size() != 32) return false;
    return crypto_ed25519_check(signature.data(), publicKey.data(),
                                message.data(), message.size()) == 0;
}
// The only supported SPKI encoding is RFC8410 Ed25519 without parameters.
// Pin bytes come from independent approved policy, never from the candidate.
inline bool VerifyManifestSignature(const std::vector<unsigned char>& manifest,
        const std::vector<unsigned char>& signature,
        const std::vector<unsigned char>& publicKeyDer, const std::string& trustedPin) {
    constexpr unsigned char prefix[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
                                       0x2b, 0x65, 0x70, 0x03, 0x21, 0x00};
    if (manifest.empty() || manifest.size() > 1048576 || signature.size() != 64 ||
        publicKeyDer.size() != 44 ||
        !std::equal(std::begin(prefix), std::end(prefix), publicKeyDer.begin()) ||
        trustedPin.size() != 64 ||
        !std::all_of(trustedPin.begin(), trustedPin.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        })) return false;
    std::array<unsigned char, 32> digest{};
#ifdef _WIN32
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
            const_cast<PUCHAR>(publicKeyDer.data()), static_cast<ULONG>(publicKeyDer.size()),
            digest.data(), static_cast<ULONG>(digest.size())) < 0) return false;
#elif defined(__APPLE__)
    // Local engineering tests use the platform hash implementation, not a
    // replacement hash algorithm. The product target remains Windows x64.
    if (!CC_SHA256(publicKeyDer.data(), static_cast<CC_LONG>(publicKeyDer.size()),
                   digest.data())) return false;
#else
    return false; // unsupported platform cannot approve a signature
#endif
    const char* hex = "0123456789abcdef";
    std::string actualPin;
    for (auto byte : digest) {
        actualPin += hex[byte >> 4]; actualPin += hex[byte & 15];
    }
    if (actualPin != trustedPin) return false;
    constexpr char domain[] = "ccode-enterprise-manifest-v1";
    std::vector<unsigned char> message(domain, domain + sizeof(domain));
    message.insert(message.end(), manifest.begin(), manifest.end());
    return VerifyEd25519(message, signature,
        std::vector<unsigned char>(publicKeyDer.begin() + sizeof(prefix), publicKeyDer.end()));
}
} // namespace ccode
