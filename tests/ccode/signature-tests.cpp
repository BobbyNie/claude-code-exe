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
    const std::string text = "{\"schemaVersion\":1,\"platform\":\"windows\",\"architecture\":\"x64\"}\n";
    auto manifest = std::vector<unsigned char>(text.begin(), text.end());
    auto der = Hex("302a300506032b6570032100d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const std::string pin = "06e3fd8fda29bb60ab59557de61edb0aecdb231134be30e75b455f8e1b792fa9";
    const auto signedManifest = Hex("dc8db7933c73e705da431536f60c74ae27a81da5eba1daad43b6cc8a02d14732f"
                                    "abc9c5813376490c09e66fb656cdab50509a3cbae61f2b3d5c7b09acf381b06");
    assert(ccode::VerifyManifestSignature(manifest, signedManifest, der, pin));
    manifest.push_back(' ');
    assert(!ccode::VerifyManifestSignature(manifest, signedManifest, der, pin));
    manifest.pop_back();
    assert(!ccode::VerifyManifestSignature(manifest, signedManifest, der, std::string(64, '0')));
    assert(!ccode::VerifyManifestSignature(manifest, signedManifest, der, ""));
    auto upperPin = pin;
    for (auto& ch : upperPin) if (ch >= 'a' && ch <= 'f') ch -= 'a' - 'A';
    assert(!ccode::VerifyManifestSignature(manifest, signedManifest, der, upperPin));
    auto noDomain = Hex("ae98afb4ba48523a5a0c04afb634ff3ab5f870399fad52fd4e373fa7cfaf400cd3"
                        "2f0e1dd46ca5e68c306a44d1e03dda27bb915765cfbc1ae5058af86bcfe80a");
    assert(!ccode::VerifyManifestSignature(manifest, noDomain, der, pin));
    assert(!ccode::VerifyManifestSignature({}, signedManifest, der, pin));
    assert(!ccode::VerifyManifestSignature(std::vector<unsigned char>(1048577), signedManifest, der, pin));
    auto extraDer = der; extraDer.push_back(0);
    assert(!ccode::VerifyManifestSignature(manifest, signedManifest, extraDer, "0bb7d17afffa5391e7032269984bb26e68666a04d59cfffc678ee47f36742a5c"));
    assert(!ccode::VerifyManifestSignature(manifest, {}, der, pin));
    der[0] ^= 1;
    assert(!ccode::VerifyManifestSignature(manifest, signedManifest, der, "b9a74897c46397ae141e353a00e27ec5984f60d90ca065bfb3b732f0eea77e20"));
    std::cout << "native signature tests passed\n";
}
