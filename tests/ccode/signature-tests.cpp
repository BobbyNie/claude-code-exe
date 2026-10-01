#include "../../scripts/ccode/manifest-signature.hpp"
#include <cassert>
#include <iostream>
#include <vector>

std::vector<unsigned char> Hex(const std::string& text) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < text.size(); i += 2)
        bytes.push_back(static_cast<unsigned char>(std::stoul(text.substr(i, 2), nullptr, 16)));
    return bytes;
}
int main() {
    // RFC 8032 test 1 proves the native primitive independently of our domain.
    const auto key = Hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    auto signature = Hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555f"
                         "b8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    assert(ccode::VerifyEd25519({}, signature, key));
    assert(!ccode::VerifyEd25519({1}, signature, key));
    auto wrongKey = key;
    wrongKey[0] ^= 1;
    assert(!ccode::VerifyEd25519({}, signature, wrongKey));
    auto truncated = signature;
    truncated.pop_back();
    assert(!ccode::VerifyEd25519({}, truncated, key));
    signature[0] ^= 1;
    assert(!ccode::VerifyEd25519({}, signature, key));
    assert(!ccode::VerifyEd25519({}, {}, key));
    assert(!ccode::VerifyEd25519({}, signature, {}));
    std::cout << "native signature tests passed\n";
}
