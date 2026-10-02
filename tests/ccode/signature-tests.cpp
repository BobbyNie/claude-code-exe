#include "../../scripts/ccode/manifest-signature.hpp"
#include "../../scripts/ccode/manifest-inventory.hpp"
#include "../../scripts/ccode/locked-candidate-file.hpp"
#include "../../scripts/ccode/authenticated-candidate-files.hpp"
#include <memory>
#include "../../scripts/ccode/native-enterprise-gate.hpp"
#include "../../scripts/ccode/retained-runtime.hpp"
#include "../../scripts/ccode/pe-image-contract.hpp"
#include "../../scripts/ccode/locked-candidate-package.hpp"
#include "../../scripts/ccode/locked-candidate-directories.hpp"
#include <filesystem>
#include <fstream>
#include "../../scripts/ccode/stream-sha256.hpp"
#include <cassert>
#include <iostream>
#include <vector>

std::vector<unsigned char> Hex(const std::string& text) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < text.size(); i += 2)
        bytes.push_back(static_cast<unsigned char>(std::stoul(text.substr(i, 2), nullptr, 16)));
    return bytes;
}
// Public engineering key only. Production must use the generated approved policy.
struct FixtureSignerPolicy {
    inline static const std::vector<unsigned char> SignerSpki = Hex(
        "302a300506032b6570032100d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    inline static constexpr char SignerPin[] = "06e3fd8fda29bb60ab59557de61edb0aecdb231134be30e75b455f8e1b792fa9";
};
int main(int argc, char** argv) {
    std::string peFixture(512, '\0');
    auto put16 = [&](size_t offset, unsigned value) {
        peFixture[offset] = static_cast<char>(value);
        peFixture[offset + 1] = static_cast<char>(value >> 8);
    };
    auto put32 = [&](size_t offset, unsigned value) {
        for (size_t i = 0; i < 4; ++i) peFixture[offset + i] = static_cast<char>(value >> (8 * i));
    };
    put16(0, 0x5a4d); put32(60, 64); put32(64, 0x4550);
    put16(68, 0x8664); put16(70, 1); put16(84, 240); put16(86, 2); put16(88, 0x20b);
    auto peReader = [&](uint64_t offset, size_t size) { return peFixture.substr(static_cast<size_t>(offset), size); };
    ccode::RequireAmd64PeImage(peFixture.size(), peReader);
    for (int mutation = 0; mutation < 7; ++mutation) {
        const auto saved = peFixture;
        if (mutation == 0) put16(68, 0xaa64);
        if (mutation == 1) put16(88, 0x10b);
        if (mutation == 2) put16(86, 0x2002);
        if (mutation == 3) put32(60, 500);
        if (mutation == 4) put16(70, 96);
        if (mutation == 5) put16(84, 0);
        if (mutation == 6) peFixture.resize(63);
        bool invalidPeRejected = false;
        try { ccode::RequireAmd64PeImage(peFixture.size(), peReader); }
        catch (const std::runtime_error& error) {
            invalidPeRejected = std::string(error.what()) == "E_MANIFEST_PE";
        }
        assert(invalidPeRejected);
        peFixture = saved;
    }
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
    struct FixtureFile {
        nlohmann::json observation;
        int& alive;
        explicit FixtureFile(nlohmann::json value, int& count) : observation(value), alive(count) { ++alive; }
        ~FixtureFile() { --alive; }
        void Verify(const nlohmann::json& expected) {
            if (!ccode::ManifestFileMatchesObservation(expected,
                    observation["size"].get<uint64_t>(), observation["sha256"].get<std::string>()))
                throw std::runtime_error("E_MANIFEST_FILE_MISMATCH");
        }
    };
    int aliveFiles = 0, openedFiles = 0;
    auto openFixture = [&](const std::string& path) {
        ++openedFiles;
        for (const auto& entry : rootManifest["files"])
            if (entry["path"] == path) return std::make_unique<FixtureFile>(entry, aliveFiles);
        throw std::runtime_error("unexpected fixture path");
    };
    {
        ccode::AuthenticatedCandidateFiles<FixtureFile> candidate(
            rawDocument, documentSignature, validDer, pin, openFixture);
        assert(candidate.Manifest() == rootManifest);
        assert(aliveFiles == static_cast<int>(rootManifest["files"].size()));
        assert(openedFiles == aliveFiles);
    }
    assert(aliveFiles == 0);
    const int openedBeforeBadSignature = openedFiles;
    bool candidateSignatureRejected = false;
    try {
        ccode::AuthenticatedCandidateFiles<FixtureFile> candidate(
            rawDocument + " ", documentSignature, validDer, pin, openFixture);
    } catch (const std::runtime_error& error) {
        candidateSignatureRejected = std::string(error.what()) == "E_MANIFEST_SIGNATURE";
    }
    assert(candidateSignatureRejected && openedFiles == openedBeforeBadSignature);
    auto openTampered = [&](const std::string& path) {
        auto file = openFixture(path);
        if (path == "notices/LICENSE.txt") file->observation["sha256"] = std::string(64, '0');
        return file;
    };
    bool candidateTamperRejected = false;
    try {
        ccode::AuthenticatedCandidateFiles<FixtureFile> candidate(
            rawDocument, documentSignature, validDer, pin, openTampered);
    } catch (const std::runtime_error& error) {
        candidateTamperRejected = std::string(error.what()) == "E_MANIFEST_FILE_MISMATCH";
    }
    assert(candidateTamperRejected && aliveFiles == 0);


#ifdef _WIN32
    auto packageFixture = std::filesystem::temp_directory_path() /
        (L"ccode-authenticated-package-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(packageFixture / L"docs");
    std::filesystem::create_directories(packageFixture / L"notices");
    auto packageDocument = rootManifest;
    for (auto& entry : packageDocument["files"]) {
        entry["size"] = 7;
        entry["sha256"] = "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d";
        std::ofstream file(packageFixture / std::filesystem::u8path(entry["path"].get<std::string>()), std::ios::binary);
        file << "fixture";
        if (entry["path"] == "ccode.exe") packageDocument["executable"] = entry;
        if (entry["path"] == "notices/LICENSE.txt") packageDocument["notices"][0] = entry;
    }
    assert(argc == 3 || (argc == 4 && std::string(argv[3]) == "--enterprise-launcher"));
    const bool exerciseEnterpriseLauncher = argc == 4;
    // Windows build supplies an actual PE and independent build metadata.
    std::ifstream expectedMetadataFile(argv[2], std::ios::binary);
    const std::string expectedMetadataBytes((std::istreambuf_iterator<char>(expectedMetadataFile)), {});
    auto expectedMetadata = ccode::ParseManifestDocument(expectedMetadataBytes);
    packageDocument["packageVersion"] = expectedMetadata["engineVersion"];
    packageDocument["provenance"] = expectedMetadata;
    packageDocument["provenance"].erase("platform");
    packageDocument["provenance"].erase("architecture");
    std::filesystem::copy_file(std::filesystem::path(argv[1]), packageFixture / L"ccode.exe",
        std::filesystem::copy_options::overwrite_existing);
    {
        ccode::LockedCandidateFile executable(packageFixture / L"ccode.exe");
        for (auto& entry : packageDocument["files"]) if (entry["path"] == "ccode.exe") {
            entry["size"] = executable.Size();
            entry["sha256"] = executable.Sha256();
            packageDocument["executable"] = entry;
        }
    }
    const auto packageBytes = packageDocument.dump();
    { std::ofstream file(packageFixture / L"manifest.json", std::ios::binary); file << packageBytes; }
    const auto packageSignature = signDocument(packageBytes);
    {
        ccode::LockedCandidatePackage package(packageFixture, packageSignature, validDer, pin);
        assert(package.Manifest() == packageDocument);
        package.VerifyEmbeddedResources();
        assert(!DeleteFileW((packageFixture / L"manifest.json").c_str()));
        assert(!DeleteFileW((packageFixture / L"notices" / L"LICENSE.txt").c_str()));
        assert(!MoveFileW(packageFixture.c_str(), (packageFixture.wstring() + L"-moved").c_str()));

    }
    auto resourceMismatchDocument = packageDocument;
    resourceMismatchDocument["provenance"]["engineSha256"] = std::string(64, '0');
    const auto resourceMismatchBytes = resourceMismatchDocument.dump();
    { std::ofstream file(packageFixture / L"manifest.json", std::ios::binary); file << resourceMismatchBytes; }
    bool resourceMismatchRejected = false;
    try {
        ccode::LockedCandidatePackage package(packageFixture, signDocument(resourceMismatchBytes), validDer, pin);
    } catch (const std::runtime_error& error) {
        resourceMismatchRejected = std::string(error.what()) == "E_MANIFEST_RESOURCE";
    }
    assert(resourceMismatchRejected);
    { std::ofstream file(packageFixture / L"manifest.json", std::ios::binary); file << packageBytes; }
    auto wrongMachineDocument = packageDocument;
    auto wrongMachineBytes = peFixture;
    wrongMachineBytes[68] = '\x64'; wrongMachineBytes[69] = '\xaa';
    { std::ofstream file(packageFixture / L"ccode.exe", std::ios::binary);
      file.write(wrongMachineBytes.data(), wrongMachineBytes.size()); }
    {
        ccode::LockedCandidateFile executable(packageFixture / L"ccode.exe");
        for (auto& entry : wrongMachineDocument["files"]) if (entry["path"] == "ccode.exe") {
            entry["size"] = executable.Size(); entry["sha256"] = executable.Sha256();
            wrongMachineDocument["executable"] = entry;
        }
    }
    const auto wrongMachineManifestBytes = wrongMachineDocument.dump();
    { std::ofstream file(packageFixture / L"manifest.json", std::ios::binary); file << wrongMachineManifestBytes; }
    bool packageMachineRejected = false;
    try {
        ccode::LockedCandidatePackage package(packageFixture, signDocument(wrongMachineManifestBytes), validDer, pin);
    } catch (const std::runtime_error& error) {
        packageMachineRejected = std::string(error.what()) == "E_MANIFEST_PE";
    }
    assert(packageMachineRejected);
    std::filesystem::copy_file(std::filesystem::path(argv[1]), packageFixture / L"ccode.exe",
        std::filesystem::copy_options::overwrite_existing);
    { std::ofstream file(packageFixture / L"manifest.json", std::ios::binary); file << packageBytes; }
    {
        std::ofstream signatureFile(packageFixture / L"manifest.sig", std::ios::binary);
        signatureFile.write(reinterpret_cast<const char*>(packageSignature.data()), packageSignature.size());
    }
    std::filesystem::create_directory(packageFixture / L"runtime");
    {
        ccode::NativeEnterpriseGate<FixtureSignerPolicy> gate(packageFixture);
        assert(gate.Manifest() == packageDocument);
        assert(!DeleteFileW((packageFixture / L"manifest.sig").c_str()));
    }
    if (exerciseEnterpriseLauncher) {
        // Execute the real enterprise frontend against this signed synthetic
        // package. This catches gate/PE-loader/share-mode integration failures
        // that constructing NativeEnterpriseGate alone cannot demonstrate.
        const auto executable = packageFixture / L"ccode.exe";
        SetEnvironmentVariableW(L"CCODE_DATA_DIR", nullptr);
        assert(GetEnvironmentVariableW(L"CCODE_DATA_DIR", nullptr, 0) == 0); // no ambient override
        auto runEntry = [&](const wchar_t* option) {
            std::wstring command = L"\"" + executable.wstring() + L"\" " + option;
            STARTUPINFOW startup{}; startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            assert(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                FALSE, CREATE_NO_WINDOW, nullptr, packageFixture.c_str(), &startup, &process));
            const auto wait = WaitForSingleObject(process.hProcess, 60000);
            if (wait != WAIT_OBJECT_0) {
                TerminateProcess(process.hProcess, 99);
                WaitForSingleObject(process.hProcess, 5000);
            }
            DWORD exitCode = 99;
            const bool observed = GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
            assert(wait == WAIT_OBJECT_0 && observed);
            return exitCode;
        };
        {
            const auto& entry = packageDocument["executable"];
            ccode::RetainedRuntimePayload retained(executable, entry["size"].get<uint64_t>(),
                                                   entry["sha256"].get<std::string>());
            // Demonstrate actual PE process creation remains compatible with
            // the retained read-only file handle and locked parent chain.
            assert(runEntry(L"--help") == 0);
        }
        for (const auto option : {L"--help", L"--version", L"--package-manifest",
                                 L"--boundary-manifest", L"--ccode-self-test"})
            assert(runEntry(option) == 0);
        assert(!std::filesystem::exists(packageFixture / L"data"));
        assert(std::filesystem::is_empty(packageFixture / L"runtime"));
        // Signed startup alone must not allow persistent state inside the
        // authenticated program tree, including the historical public default.
        assert(runEntry(L"--sessions") == 64);
        for (const auto& forbiddenData : {packageFixture, packageFixture / L"runtime",
                                         packageFixture / L"data", packageFixture.parent_path()}) {
            const auto arguments = L"--sessions --data-dir \"" + forbiddenData.wstring() + L"\"";
            assert(runEntry(arguments.c_str()) == 64);
        }
        assert(!std::filesystem::exists(packageFixture / L"data"));
        const auto externalData = packageFixture.parent_path() /
            (L"ccode-external-data-" + std::to_wstring(GetCurrentProcessId()));
        assert(!std::filesystem::exists(externalData));
        const auto externalArguments = L"--sessions --data-dir \"" + externalData.wstring() + L"\"";
        assert(runEntry(externalArguments.c_str()) == 0);
        assert(std::filesystem::is_directory(externalData));
        std::filesystem::remove_all(externalData);
        assert(!std::filesystem::exists(packageFixture / L"data"));
        assert(std::filesystem::is_empty(packageFixture / L"runtime"));
        {
            std::ofstream signatureFile(packageFixture / L"manifest.sig", std::ios::binary);
            const std::string zeros(64, '\0'); signatureFile.write(zeros.data(), zeros.size());
        }
        for (const auto option : {L"--help", L"--package-manifest", L"--ccode-permission-server", L"--print probe"})
            assert(runEntry(option) == 64);
        {
            std::ofstream signatureFile(packageFixture / L"manifest.sig", std::ios::binary);
            signatureFile.write(reinterpret_cast<const char*>(packageSignature.data()), packageSignature.size());
        }
        { std::ofstream notice(packageFixture / L"notices" / L"LICENSE.txt", std::ios::binary); notice << "tamper!"; }
        assert(runEntry(L"--help") == 64);
        { std::ofstream notice(packageFixture / L"notices" / L"LICENSE.txt", std::ios::binary); notice << "fixture"; }
        { std::ofstream manifestFile(packageFixture / L"manifest.json", std::ios::binary); manifestFile << packageBytes << ' '; }
        assert(runEntry(L"--version") == 64); // Signature covers original raw bytes.
        { std::ofstream manifestFile(packageFixture / L"manifest.json", std::ios::binary); manifestFile << packageBytes; }
        assert(runEntry(L"--version") == 0);
        assert(!std::filesystem::exists(packageFixture / L"data"));
        assert(std::filesystem::is_empty(packageFixture / L"runtime"));
        std::cout << "Signed enterprise launcher entry-point integration passed\n";
    }
    { std::ofstream signatureFile(packageFixture / L"manifest.sig", std::ios::binary);
      signatureFile << std::string(64, '\0'); }
    bool badStartupSignatureRejected = false;
    try { ccode::NativeEnterpriseGate<FixtureSignerPolicy> gate(packageFixture); }
    catch (const std::runtime_error& error) {
        badStartupSignatureRejected = std::string(error.what()) == "E_MANIFEST_SIGNATURE";
    }
    assert(badStartupSignatureRejected);
    std::filesystem::remove(packageFixture / L"manifest.sig");
    bool unsignedStartupRejected = false;
    try { ccode::NativeEnterpriseGate<FixtureSignerPolicy> gate(packageFixture); }
    catch (const std::runtime_error& error) {
        unsignedStartupRejected = std::string(error.what()) == "E_MANIFEST_FILE";
    }
    assert(unsignedStartupRejected);
    bool freshRuntimeRejected = false;
    try { ccode::LockedCandidatePackage package(packageFixture, packageSignature, validDer, pin); }
    catch (const std::runtime_error& error) {
        freshRuntimeRejected = std::string(error.what()) == "E_MANIFEST_INVENTORY";
    }
    assert(freshRuntimeRejected);
    std::filesystem::remove(packageFixture / L"runtime");
    { std::ofstream file(packageFixture / L"extra.txt"); file << "extra"; }
    bool extraFileRejected = false;
    try { ccode::LockedCandidatePackage package(packageFixture, packageSignature, validDer, pin); }
    catch (const std::runtime_error& error) {
        extraFileRejected = std::string(error.what()) == "E_MANIFEST_INVENTORY";
    }
    assert(extraFileRejected);
    std::filesystem::remove(packageFixture / L"extra.txt");
    { std::ofstream file(packageFixture / L"notices" / L"LICENSE.txt", std::ios::binary); file << "tamper!"; }
    bool packageTamperRejected = false;
    try { ccode::LockedCandidatePackage package(packageFixture, packageSignature, validDer, pin); }
    catch (const std::runtime_error& error) {
        packageTamperRejected = std::string(error.what()) == "E_MANIFEST_FILE_MISMATCH";
    }
    assert(packageTamperRejected);
    std::filesystem::remove_all(packageFixture);
    auto directoryFixture = std::filesystem::temp_directory_path() /
        (L"ccode-locked-directory-" + std::to_wstring(GetCurrentProcessId()));
    auto nestedFixture = directoryFixture / L"candidate";
    std::filesystem::create_directories(nestedFixture);
    auto ordinaryFile = nestedFixture / L"not-a-directory";
    { std::ofstream file(ordinaryFile); file << "fixture"; }
    for (const auto& invalid : std::vector<std::filesystem::path>{
            ordinaryFile, nestedFixture / L"..", std::filesystem::path(L"relative")}) {
        bool directoryRejected = false;
        try { ccode::LockedCandidateDirectories directories(invalid); }
        catch (const std::runtime_error& error) {
            directoryRejected = std::string(error.what()) == "E_MANIFEST_DIRECTORY";
        }
        assert(directoryRejected);
    }

    {
        ccode::LockedCandidateDirectories directories(nestedFixture);
        assert(!MoveFileW(directoryFixture.c_str(), (directoryFixture.wstring() + L"-moved").c_str()));
        assert(!MoveFileW(nestedFixture.c_str(), (nestedFixture.wstring() + L"-moved").c_str()));
    }
    assert(MoveFileW(nestedFixture.c_str(), (nestedFixture.wstring() + L"-moved").c_str()));
    std::filesystem::remove_all(directoryFixture);
    auto fixturePath = std::filesystem::temp_directory_path() /
        (L"ccode-locked-file-" + std::to_wstring(GetCurrentProcessId()) + L".txt");
    { std::ofstream fixture(fixturePath, std::ios::binary); fixture << "fixture"; }
    {
        ccode::LockedCandidateFile locked(fixturePath);
        auto lockedEntry = executableEntry;
        lockedEntry["size"] = 7;
        lockedEntry["sha256"] = "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d";
        locked.Verify(lockedEntry);
        lockedEntry["size"] = 8;
        bool mismatchRejected = false;
        try { locked.Verify(lockedEntry); }
        catch (const std::runtime_error& error) {
            mismatchRejected = std::string(error.what()) == "E_MANIFEST_FILE_MISMATCH";
        }
        assert(mismatchRejected);
        assert(locked.ReadRange(1, 3) == "ixt");
        bool rangeRejected = false;
        try { locked.ReadRange(7, 1); }
        catch (const std::runtime_error& error) {
            rangeRejected = std::string(error.what()) == "E_MANIFEST_FILE_LIMIT";
        }
        assert(rangeRejected);
        assert(locked.ReadBounded(7) == "fixture");
        assert(locked.Sha256() == "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d");
        assert(locked.ReadBounded(7) == "fixture");
        bool limitRejected = false;
        try { locked.ReadBounded(6); }
        catch (const std::runtime_error& error) {
            limitRejected = std::string(error.what()) == "E_MANIFEST_FILE_LIMIT";
        }
        assert(limitRejected);
        HANDLE writer = CreateFileW(fixturePath.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        assert(writer == INVALID_HANDLE_VALUE);
        assert(!DeleteFileW(fixturePath.c_str()));
    }
    HANDLE existingWriter = CreateFileW(fixturePath.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(existingWriter != INVALID_HANDLE_VALUE);
    bool writerRejected = false;
    try { ccode::LockedCandidateFile locked(fixturePath); }
    catch (const std::runtime_error& error) {
        writerRejected = std::string(error.what()) == "E_MANIFEST_FILE";
    }
    CloseHandle(existingWriter);
    assert(writerRejected);
    auto hardlinkPath = fixturePath; hardlinkPath += L".link";
    assert(CreateHardLinkW(hardlinkPath.c_str(), fixturePath.c_str(), nullptr));
    bool hardlinkRejected = false;
    try { ccode::LockedCandidateFile locked(fixturePath); }
    catch (const std::runtime_error& error) {
        hardlinkRejected = std::string(error.what()) == "E_MANIFEST_FILE";
    }
    assert(hardlinkRejected);
    assert(DeleteFileW(hardlinkPath.c_str()));
    assert(DeleteFileW(fixturePath.c_str()));
#endif
    auto embeddedProvenance = rootManifest["provenance"];
    embeddedProvenance["platform"] = "windows";
    embeddedProvenance["architecture"] = "x64";
    assert(ccode::ManifestMatchesEmbeddedProvenance(rootManifest, embeddedProvenance));
    embeddedProvenance["engineSha256"] = std::string(64, 'd');
    assert(!ccode::ManifestMatchesEmbeddedProvenance(rootManifest, embeddedProvenance));
    for (int mutation = 0; mutation < 5; ++mutation) {
        auto metadata = rootManifest["provenance"];
        metadata["platform"] = "windows"; metadata["architecture"] = "x64";
        if (mutation == 0) metadata["engineSize"] = 1.0;
        if (mutation == 1) metadata["architecture"] = "arm64";
        if (mutation == 2) metadata.erase("adapterRevision");
        if (mutation == 3) metadata["extra"] = "untrusted";
        if (mutation == 4) metadata["officialManifestSha256"] = std::string(64, 'd');
        assert(!ccode::ManifestMatchesEmbeddedProvenance(rootManifest, metadata));
    }
    size_t chunkOffset = 0;
    const std::string hashFixture = "fixture";
    auto chunkReader = [&](unsigned char* buffer, size_t capacity) {
        const auto count = std::min<size_t>(2, hashFixture.size() - chunkOffset);
        assert(count <= capacity);
        std::copy_n(hashFixture.data() + chunkOffset, count, buffer);
        chunkOffset += count;
        return count;
    };
    assert(ccode::StreamSha256(chunkReader) == "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d");
    assert(ccode::StreamSha256([](unsigned char*, size_t) { return size_t(0); }) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    bool oversizeChunkRejected = false;
    try { ccode::StreamSha256([](unsigned char*, size_t maximum) { return maximum + 1; }); }
    catch (const std::runtime_error& error) {
        oversizeChunkRejected = std::string(error.what()) == "E_MANIFEST_HASH";
    }
    assert(oversizeChunkRejected);
    auto observedEntry = executableEntry;
    observedEntry["size"] = 7;
    observedEntry["sha256"] = "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d";
    assert(ccode::ManifestFileMatchesObservation(observedEntry, 7,
        "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d"));
    assert(!ccode::ManifestFileMatchesObservation(observedEntry, 8,
        "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d"));
    assert(!ccode::ManifestFileMatchesObservation(observedEntry, 7, std::string(64, '0')));
    std::cout << "native signature tests passed\n";
}
