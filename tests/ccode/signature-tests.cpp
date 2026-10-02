#include "../../scripts/ccode/manifest-signature.hpp"
#include "../../scripts/ccode/manifest-inventory.hpp"
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
    assert(ccode::ParseManifestDocument("{\"schemaVersion\":1}").at("schemaVersion") == 1);
    bool duplicateRejected = false;
    try { ccode::ParseManifestDocument("{\"schemaVersion\":1,\"schemaVersion\":2}"); }
    catch (const std::runtime_error& error) {
        duplicateRejected = std::string(error.what()) == "E_MANIFEST_DOCUMENT";
    }
    assert(duplicateRejected);
    for (const auto& invalid : std::vector<std::string>{
            "", "[]", "null", "{", "{} trailing",
            "{\"nested\":{\"x\":1,\"x\":2}}",
            "{\"x\":1,\"\u0078\":2}",
            std::string(1048577, ' '),
            std::string("{\"x\":") + std::string(40, '[') + "0" + std::string(40, ']') + "}"}) {
        bool rejected = false;
        try { ccode::ParseManifestDocument(invalid); }
        catch (const std::runtime_error& error) {
            rejected = std::string(error.what()) == "E_MANIFEST_DOCUMENT";
        }
        assert(rejected);
    }
    assert(ccode::ParseManifestDocument("{\"a\":{\"x\":1},\"b\":{\"x\":2}}").size() == 2);
    auto fileEntry = nlohmann::json{{"path", "notices/LICENSE.txt"}, {"size", 0},
                                   {"sha256", std::string(64, 'a')}};
    assert(ccode::ValidManifestFileEntry(fileEntry));
    fileEntry["path"] = "notices/../ccode.exe";
    assert(!ccode::ValidManifestFileEntry(fileEntry));
    for (const auto& path : {"ccode.exe", "docs/usage.md", "notices/NOTICE.md"}) {
        fileEntry["path"] = path;
        assert(ccode::ValidManifestFileEntry(fileEntry));
    }
    for (const auto& path : {"../ccode.exe", "C:/ccode.exe", "notices/", "notices/.",
            "notices/..", "notices/nested/a", "notices/a\\b", "notices/a:stream",
            "notices/CON.txt", "notices/lPt9", "notices/NUL", "notices/a.",
            "notices/a ", "notices/a?", "notices/a\n"}) {
        fileEntry["path"] = path;
        assert(!ccode::ValidManifestFileEntry(fileEntry));
    }
    fileEntry["path"] = "ccode.exe";
    for (const auto& size : std::vector<nlohmann::json>{true, -1, 1.0, "1", nullptr}) {
        fileEntry["size"] = size;
        assert(!ccode::ValidManifestFileEntry(fileEntry));
    }
    fileEntry["size"] = 1;
    for (const auto& hash : {std::string(64, 'A'), std::string(63, 'a'), std::string(64, 'g')}) {
        fileEntry["sha256"] = hash;
        assert(!ccode::ValidManifestFileEntry(fileEntry));
    }
    fileEntry["sha256"] = std::string(64, 'a');
    fileEntry["extra"] = 1;
    assert(!ccode::ValidManifestFileEntry(fileEntry));
    auto executableEntry = nlohmann::json{{"path", "ccode.exe"}, {"size", 1}, {"sha256", std::string(64, 'a')}};
    auto usageEntry = executableEntry; usageEntry["path"] = "docs/usage.md";
    auto noticeEntry = executableEntry; noticeEntry["path"] = "notices/LICENSE.txt";
    auto inventory = nlohmann::json{{"executable", executableEntry},
        {"files", nlohmann::json::array({executableEntry, usageEntry, noticeEntry})},
        {"notices", nlohmann::json::array({noticeEntry})}};
    assert(ccode::ValidManifestInventory(inventory));
    auto duplicatedInventory = inventory;
    duplicatedInventory["files"].push_back(noticeEntry);
    assert(!ccode::ValidManifestInventory(duplicatedInventory));
    auto aliasInventory = inventory;
    auto aliasEntry = noticeEntry; aliasEntry["path"] = "notices/license.txt";
    aliasInventory["files"].push_back(aliasEntry);
    aliasInventory["notices"].push_back(aliasEntry);
    assert(!ccode::ValidManifestInventory(aliasInventory));
    for (int mutation = 0; mutation < 6; ++mutation) {
        auto invalidInventory = inventory;
        if (mutation == 0) invalidInventory["files"].erase(0);
        if (mutation == 1) invalidInventory["notices"] = nlohmann::json::array();
        if (mutation == 2) invalidInventory["executable"]["size"] = 1.0;
        if (mutation == 3) invalidInventory["notices"][0]["sha256"] = std::string(64, 'b');
        if (mutation == 4) std::swap(invalidInventory["files"][0], invalidInventory["files"][1]);
        if (mutation == 5) invalidInventory["notices"].push_back(noticeEntry);
        assert(!ccode::ValidManifestInventory(invalidInventory));
    }
    auto rootManifest = inventory;
    rootManifest.update(nlohmann::json{{"schemaVersion", 1}, {"packageName", "ccode-enterprise"},
        {"packageVersion", "2.1.282"}, {"platform", "windows"}, {"architecture", "x64"},
        {"minimumWindowsBuild", 22000}, {"provenance", nlohmann::json::object()},
        {"runtimeBoundary", nlohmann::json::object()},
        {"excludedDynamicData", {"data/", "profile/", "runtime/", "sessions/", "temp/"}},
        {"redistributionApproval", "external-gate-not-asserted"},
        {"publicBoundary", {{"opaqueContents", {"ccode.exe"}},
            {"scannedText", {"docs/usage.md", "manifest.json", "notices/LICENSE.txt"}},
            {"parentDirectories", "excluded"}}}});
    assert(ccode::ValidManifestRootContract(rootManifest));
    auto floatSchema = rootManifest; floatSchema["schemaVersion"] = 1.0;
    assert(!ccode::ValidManifestRootContract(floatSchema));
    for (int mutation = 0; mutation < 8; ++mutation) {
        auto invalidRoot = rootManifest;
        if (mutation == 0) invalidRoot["extra"] = true;
        if (mutation == 1) invalidRoot.erase("platform");
        if (mutation == 2) invalidRoot["minimumWindowsBuild"] = 22000.0;
        if (mutation == 3) invalidRoot["architecture"] = "arm64";
        if (mutation == 4) invalidRoot["publicBoundary"]["scannedText"] = nlohmann::json::array();
        if (mutation == 5) invalidRoot["publicBoundary"]["extra"] = true;
        if (mutation == 6) invalidRoot["redistributionApproval"] = "approved";
        if (mutation == 7) invalidRoot["excludedDynamicData"].push_back("other/");
        assert(!ccode::ValidManifestRootContract(invalidRoot));
    }
    const std::string officialBase = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/2.1.282";
    rootManifest["provenance"] = {{"schemaVersion", 1}, {"packageName", "ccode"},
        {"packageVersion", "1.0"}, {"adapterRevision", std::string(40, 'a')},
        {"engineVersion", "2.1.282"}, {"engineSha256", std::string(64, 'b')},
        {"engineSize", 1}, {"officialManifestUrl", officialBase + "/manifest.json"},
        {"officialManifestSha256", std::string(64, 'c')},
        {"officialPayloadUrl", officialBase + "/win32-x64/claude.exe"}};
    rootManifest["runtimeBoundary"] = ccode::BoundaryManifest();
    assert(ccode::ValidManifestProvenanceContract(rootManifest));
    auto wrongSource = rootManifest;
    wrongSource["provenance"]["officialPayloadUrl"] = "https://example.invalid/claude.exe";
    assert(!ccode::ValidManifestProvenanceContract(wrongSource));
    for (int mutation = 0; mutation < 8; ++mutation) {
        auto invalidSource = rootManifest;
        auto& source = invalidSource["provenance"];
        if (mutation == 0) source["engineSize"] = true;
        if (mutation == 1) source["engineSize"] = 0;
        if (mutation == 2) source["engineSize"] = 1.0;
        if (mutation == 3) source["engineVersion"] = "2.1.221";
        if (mutation == 4) source["adapterRevision"] = std::string(40, 'A');
        if (mutation == 5) source["extra"] = 1;
        if (mutation == 6) invalidSource["runtimeBoundary"]["schemaVersion"] = 1.0;
        if (mutation == 7) source["officialManifestUrl"] = officialBase + "/manifest.json?override=1";
        assert(!ccode::ValidManifestProvenanceContract(invalidSource));
    }
    // RFC8032 public test seed only; never a production trust policy.
    auto signDocument = [](const std::string& document) {
        auto seed = Hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
        unsigned char secret[64], publicBytes[32];
        crypto_ed25519_key_pair(secret, publicBytes, seed.data());
        const char domain[] = "ccode-enterprise-manifest-v1";
        std::vector<unsigned char> bytes(domain, domain + sizeof(domain));
        bytes.insert(bytes.end(), document.begin(), document.end());
        std::vector<unsigned char> result(64);
        crypto_ed25519_sign(result.data(), secret, bytes.data(), bytes.size());
        return result;
    };
    auto rawDocument = rootManifest.dump() + "\n";
    auto documentSignature = signDocument(rawDocument);
    auto validDer = Hex("302a300506032b6570032100d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    assert(ccode::AuthenticateManifestDocument(rawDocument, documentSignature, validDer, pin) == rootManifest);
    bool changedRejected = false;
    try { ccode::AuthenticateManifestDocument(rawDocument + " ", documentSignature, validDer, pin); }
    catch (const std::runtime_error& error) {
        changedRejected = std::string(error.what()) == "E_MANIFEST_SIGNATURE";
    }
    assert(changedRejected);
    auto signedBadSchema = rootManifest;
    signedBadSchema["architecture"] = "arm64";
    for (const auto& item : std::vector<std::pair<std::string, std::string>>{
            {signedBadSchema.dump(), "E_MANIFEST_SCHEMA"},
            {"{\"schemaVersion\":1,\"schemaVersion\":2}", "E_MANIFEST_DOCUMENT"}}) {
        bool rejected = false;
        try { ccode::AuthenticateManifestDocument(item.first, signDocument(item.first), validDer, pin); }
        catch (const std::runtime_error& error) { rejected = error.what() == item.second; }
        assert(rejected);
    }
    bool signatureFirst = false;
    try { ccode::AuthenticateManifestDocument("not JSON", documentSignature, validDer, pin); }
    catch (const std::runtime_error& error) {
        signatureFirst = std::string(error.what()) == "E_MANIFEST_SIGNATURE";
    }
    assert(signatureFirst);
    std::cout << "native signature tests passed\n";
}
