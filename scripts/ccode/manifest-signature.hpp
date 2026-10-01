#pragma once
#include "vendor/monocypher/monocypher-ed25519.h"
#include <vector>

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
} // namespace ccode
