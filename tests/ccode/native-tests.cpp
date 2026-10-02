#include "../../scripts/ccode/common.hpp"
#include "../../scripts/ccode/runtime-paths.hpp"
#include "../../scripts/ccode/enterprise-data.hpp"
#include "../../scripts/ccode/retained-runtime.hpp"
#include "../../scripts/ccode/runtime-staging.hpp"
#include "../../scripts/ccode/extraction-errors.hpp"
#include "../../scripts/ccode/concurrency.hpp"
#include "../../scripts/ccode/workspace-boundary.hpp"
#include <fstream>
#include <chrono>

#include <cassert>
#include <iostream>

int RunNativeTests() {
    using namespace ccode;
    assert(std::string(RuntimePayloadOpenError(2)) == "E_RUNTIME_MISSING");
    assert(std::string(RuntimePayloadOpenError(3)) == "E_RUNTIME_PATH");
    assert(std::string(RuntimePayloadOpenError(5)) == "E_RUNTIME_ACCESS");
    assert(std::string(RuntimePayloadOpenError(32)) == "E_RUNTIME_SHARING");
    assert(std::string(RuntimePayloadOpenError(999999)) == "E_RUNTIME_FILE");
    RequireRuntimeStagingObservation(true, false, false, 1);
    for (const auto& invalid : std::vector<std::array<unsigned, 4>>{
            {0, 0, 0, 1}, {1, 1, 0, 1}, {1, 0, 1, 1}, {1, 0, 0, 2}}) {
        bool stagingRejected = false;
        try { RequireRuntimeStagingObservation(invalid[0], invalid[1], invalid[2], invalid[3]); }
        catch (const std::runtime_error& error) { stagingRejected = std::string(error.what()) == "E_RUNTIME_PATH"; }
        assert(stagingRejected);
    }
    const std::string fixtureRuntimeHash = "f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d";
    RequireRuntimePayloadObservation(7, fixtureRuntimeHash, 7, fixtureRuntimeHash);
    for (const auto& observation : std::vector<std::pair<uint64_t, std::string>>{
            {8, fixtureRuntimeHash}, {7, std::string(64, '0')}}) {
        bool runtimeRejected = false;
        try { RequireRuntimePayloadObservation(7, fixtureRuntimeHash, observation.first, observation.second); }
        catch (const std::runtime_error& error) { runtimeRejected = std::string(error.what()) == "E_RUNTIME_INTEGRITY"; }
        assert(runtimeRejected);
    }
    const auto programPath = std::filesystem::absolute("enterprise-program");
    for (const auto& dataPath : {programPath, programPath / "data", programPath.parent_path()}) {
        bool rejected = false;
        try { RequireDisjointEnterpriseData(programPath, dataPath); }
        catch (const std::runtime_error& error) { rejected = std::string(error.what()) == "E_ENTERPRISE_DATA"; }
        assert(rejected);
    }
    RequireDisjointEnterpriseData(programPath, programPath.parent_path() / "enterprise-program-data");
    RequireDisjointEnterpriseData(programPath, programPath.parent_path() / "external-data");
    bool caseAliasRejected = false;
    try { RequireDisjointEnterpriseData(programPath, programPath.parent_path() / "ENTERPRISE-PROGRAM"); }
    catch (const std::runtime_error& error) { caseAliasRejected = std::string(error.what()) == "E_ENTERPRISE_DATA"; }
    assert(caseAliasRejected);
#ifdef _WIN32
    const auto enterpriseRoot = std::filesystem::temp_directory_path() /
        (L"ccode-data-boundary-" + std::to_wstring(GetCurrentProcessId()));
    const auto enterpriseProgram = enterpriseRoot / L"program";
    const auto enterpriseData = enterpriseRoot / L"external" / L"nested";
    std::filesystem::create_directories(enterpriseProgram);
    {
        LockedEnterpriseData data(enterpriseProgram, enterpriseData);
        assert(std::filesystem::is_directory(enterpriseData));
        assert(!MoveFileW(enterpriseData.c_str(), (enterpriseData.wstring() + L"-moved").c_str()));
        assert(!MoveFileW(enterpriseData.parent_path().c_str(),
                         (enterpriseData.parent_path().wstring() + L"-moved").c_str()));
    }
    assert(MoveFileW(enterpriseData.c_str(), (enterpriseData.wstring() + L"-moved").c_str()));
    std::filesystem::remove_all(enterpriseRoot);
    const auto runtimeRoot = std::filesystem::temp_directory_path() /
        (L"ccode-runtime-retention-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(runtimeRoot / L"runtime");
    const auto runtimeEngine = runtimeRoot / L"runtime" / L"engine.exe";
    { std::ofstream engine(runtimeEngine, std::ios::binary); engine << "fixture"; }
    bool retainedMismatchRejected = false;
    try { RetainedRuntimePayload engine(runtimeEngine, 7, std::string(64, '0')); }
    catch (const std::runtime_error& error) {
        retainedMismatchRejected = std::string(error.what()) == "E_RUNTIME_INTEGRITY";
    }
    assert(retainedMismatchRejected);
    {
        RetainedRuntimePayload engine(runtimeEngine, 7, fixtureRuntimeHash);
        assert(engine.Path() == runtimeEngine);
        assert(!DeleteFileW(runtimeEngine.c_str()));
        assert(!MoveFileW(runtimeRoot.c_str(), (runtimeRoot.wstring() + L"-moved").c_str()));
        HANDLE writer = CreateFileW(runtimeEngine.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        assert(writer == INVALID_HANDLE_VALUE);
    }
    assert(DeleteFileW(runtimeEngine.c_str()));
    // MSVC release CRT assertion failures can omit their expression from CI.
    // Emit only fixed fixture checkpoint IDs, never candidate paths or bytes.
    auto stagingCheck = [](bool condition, const char* checkpoint) {
        if (!condition) { std::cerr << checkpoint << std::endl; std::exit(1); }
    };
    std::cerr << "E_TEST_STAGING_BEGIN" << std::endl;
    const auto stagingPath = runtimeEngine.parent_path() / L"engine.new";
    { std::ofstream stale(stagingPath, std::ios::binary); stale << "stale-long-tail"; }
    {
        RuntimeStagingFile staging(runtimeEngine.parent_path());
        staging.Write(reinterpret_cast<const unsigned char*>("fixture"), 7);
        stagingCheck(!DeleteFileW(stagingPath.c_str()), "E_TEST_STAGING_1");
        std::cerr << "E_TEST_STAGING_FIRST_ACTIVATE" << std::endl;
        staging.Activate();
        stagingCheck(!DeleteFileW(runtimeEngine.c_str()), "E_TEST_STAGING_2");
        std::cerr << "E_TEST_STAGING_FIRST_DONE" << std::endl;
    }
    { RetainedRuntimePayload engine(runtimeEngine, 7, fixtureRuntimeHash); }
    stagingCheck(DeleteFileW(runtimeEngine.c_str()), "E_TEST_STAGING_3");
    const auto sentinel = runtimeRoot / L"sentinel";
    { std::ofstream original(sentinel, std::ios::binary); original << "sentinel"; }
    stagingCheck(CreateHardLinkW(stagingPath.c_str(), sentinel.c_str(), nullptr), "E_TEST_STAGING_4");
    bool hardlinkRejected = false;
    try { RuntimeStagingFile staging(runtimeEngine.parent_path()); }
    catch (const std::runtime_error& error) { hardlinkRejected = std::string(error.what()) == "E_RUNTIME_PATH"; }
    stagingCheck(hardlinkRejected, "E_TEST_STAGING_5");
    { std::ifstream original(sentinel, std::ios::binary); std::string value;
      original >> value; stagingCheck(value == "sentinel", "E_TEST_STAGING_6"); }
    stagingCheck(DeleteFileW(stagingPath.c_str()), "E_TEST_STAGING_7");
    { std::ofstream original(runtimeEngine, std::ios::binary); original << "fixture"; }
    {
        RetainedRuntimePayload original(runtimeEngine, 7, fixtureRuntimeHash);
        RuntimeStagingFile staging(runtimeEngine.parent_path());
        staging.Write(reinterpret_cast<const unsigned char*>("replacement"), 11);
        bool blocked = false;
        try { staging.Activate(); } catch (const std::runtime_error& error) {
            blocked = std::string(error.what()) == "E_EXTRACT_SHARING";
        }
        stagingCheck(blocked, "E_TEST_STAGING_8");
    }
    { RetainedRuntimePayload original(runtimeEngine, 7, fixtureRuntimeHash); }
    std::cerr << "E_TEST_STAGING_RETRY_BEGIN" << std::endl;
    {
        RuntimeStagingFile staging(runtimeEngine.parent_path());
        staging.Write(reinterpret_cast<const unsigned char*>("fixture"), 7);
        staging.Activate();
    }
    { RetainedRuntimePayload engine(runtimeEngine, 7, fixtureRuntimeHash); }
    std::cerr << "E_TEST_STAGING_LONG_BEGIN" << std::endl;
    const auto namedRuntime = runtimeRoot / (L"runtime 中文 with spaces " + std::wstring(80, L'x'));
    std::filesystem::create_directory(namedRuntime);
    const std::string largePayload(131073, 'q');
    {
        RuntimeStagingFile staging(namedRuntime);
        std::cerr << "E_TEST_STAGING_LONG_WRITE" << std::endl;
        staging.Write(reinterpret_cast<const unsigned char*>(largePayload.data()), largePayload.size());
        std::cerr << "E_TEST_STAGING_LONG_ACTIVATE" << std::endl;
        staging.Activate();
    }
    {
        LockedCandidateFile activated(namedRuntime / L"engine.exe", LockedFilePurpose::RuntimePayload);
        stagingCheck(activated.Size() == largePayload.size(), "E_TEST_STAGING_LONG_SIZE");
        stagingCheck(activated.ReadBounded(largePayload.size()) == largePayload,
                     "E_TEST_STAGING_LONG_BYTES");
    }
    std::filesystem::remove_all(runtimeRoot);

#endif


    assert(std::string(ExtractionWriteError(112)) == "E_EXTRACT_DISK_FULL");
    assert(std::string(ExtractionWriteError(39)) == "E_EXTRACT_DISK_FULL");
    assert(std::string(ExtractionWriteError(5)) == "E_EXTRACT_ACCESS");
    assert(std::string(ExtractionWriteError(32)) == "E_EXTRACT_SHARING");
    assert(std::string(ExtractionWriteError(33)) == "E_EXTRACT_LOCKED");
    assert(std::string(ExtractionWriteError(0)) == "E_EXTRACT_WRITE");
    assert(std::string(ExtractionWriteError(999999)) == "E_EXTRACT_WRITE");
    assert(std::string(ExtractionActivationError(5)) == "E_EXTRACT_ACCESS");
    assert(std::string(ExtractionActivationError(32)) == "E_EXTRACT_SHARING");
    assert(std::string(ExtractionActivationError(33)) == "E_EXTRACT_LOCKED");
    assert(std::string(ExtractionActivationError(39)) == "E_EXTRACT_DISK_FULL");
    assert(std::string(ExtractionActivationError(112)) == "E_EXTRACT_DISK_FULL");
    assert(std::string(ExtractionActivationError(999999)) == "E_EXTRACT_ACTIVATE");

    assert(MapEnvironmentName(L"ANTHROPIC_DEFAULT_HAIKU_MODEL") == L"A_DEFAULT_HAIKU_MODEL");
    assert(MapEnvironmentName(L"anthropic_api_key") == L"A_api_key");
    assert(MapEnvironmentName(L"ANTHROPIC_AUTH_TOKEN") == L"A_AUTH_TOKEN");
    assert(MapEnvironmentName(L"CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC") ==
           L"C_DISABLE_NONESSENTIAL_TRAFFIC");
    assert(MapEnvironmentName(L"PATH") == L"PATH");
    assert(ExpandEnvironmentName(L"A_BASE_URL") == L"ANTHROPIC_BASE_URL");
    assert(ExpandEnvironmentName(L"A_AUTH_TOKEN") == L"ANTHROPIC_AUTH_TOKEN");
    assert(ExpandEnvironmentName(L"C_SUBAGENT_MODEL") == L"CLAUDE_CODE_SUBAGENT_MODEL");
    assert(ExpandEnvironmentName(L"PATH") == L"PATH");
    assert(HasApiCredential(L"", L"vendor-token"));
    assert(HasApiCredential(L"compatibility-key", L""));
    assert(!HasApiCredential(L"", L""));


    assert(IsValidGatewayUrl(L"https://gateway.example.test"));
    assert(IsValidGatewayUrl(L"https://api.deepseek.com/anthropic"));
    assert(IsValidGatewayUrl(L"https://api.anthropic.example"));
    assert(IsValidGatewayUrl(L"http://gateway.example.test"));
    assert(!IsValidGatewayUrl(L""));
    assert(GatewayHost(L"https://gateway.example.test/v1") == L"gateway.example.test");
    assert(GatewayHost(L"http://gateway.example.test/v1") == L"gateway.example.test");
    assert(IsAllowedNetworkHost(L"gateway.example.test", L"https://gateway.example.test/v1"));
    assert(!IsAllowedNetworkHost(L"example.org", L"https://gateway.example.test/v1"));

    const std::filesystem::path maximumWorkspace(std::wstring(258, L'a'));
    ValidateWorkspaceBoundary(maximumWorkspace);
    bool longWorkspaceRejected = false;
    try { ValidateWorkspaceBoundary(std::filesystem::path(std::wstring(259, L'a'))); }
    catch (const std::runtime_error& error) {
        longWorkspaceRejected = std::string(error.what()) == "E_WORKSPACE_PATH_TOO_LONG";
    }
    assert(longWorkspaceRejected);
    for (const auto& unc : {std::filesystem::path(L"\\\\server\\share\\workspace"),
                            std::filesystem::path(L"\\\\?\\UNC\\server\\share\\workspace"),
                            std::filesystem::path(L"\\\\?\\unc\\server\\share\\workspace")}) {
        bool uncRejected = false;
        try { ValidateWorkspaceBoundary(unc); }
        catch (const std::runtime_error& error) {
            uncRejected = std::string(error.what()) == "E_WORKSPACE_UNSUPPORTED";
        }
        assert(uncRejected);
    }
    for (const auto& device : {std::filesystem::path(L"\\\\?\\C:\\workspace"),
                               std::filesystem::path(L"\\\\.\\C:\\workspace")}) {
        bool deviceRejected = false;
        try { ValidateWorkspaceBoundary(device); }
        catch (const std::runtime_error& error) {
            deviceRejected = std::string(error.what()) == "E_WORKSPACE_PATH";
        }
        assert(deviceRejected);
    }

    namespace fs = std::filesystem;
    const auto workspaceSelectionRoot = fs::temp_directory_path() / ("ccode-workspace-selection-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(workspaceSelectionRoot / "selected");
    assert(ResolveWorkspaceSelection(workspaceSelectionRoot, {}) == workspaceSelectionRoot.lexically_normal());
    assert(ResolveWorkspaceSelection(workspaceSelectionRoot, "selected") ==
           (workspaceSelectionRoot / "selected").lexically_normal());
    fs::remove_all(workspaceSelectionRoot);

    const auto root = fs::temp_directory_path() / ("ccode-runtime-paths-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::string hash(64, 'a');
    fs::create_directories(root / "runtime" / hash);
    const auto runtime = root / "runtime" / hash;
    ValidateRuntimePaths(root, hash);
    const auto outside = root / "outside.txt";
    { std::ofstream output(outside); output << "preserve-outside"; }
    std::error_code linkError;
    fs::create_symlink(outside, runtime / "engine.new", linkError);
    if (!linkError) {
        bool rejected = false;
        try { ValidateRuntimePaths(root, hash); }
        catch (const std::runtime_error& error) { rejected = std::string(error.what()) == "E_RUNTIME_PATH"; }
        assert(rejected);
        std::ifstream input(outside);
        std::string contents; std::getline(input, contents);
        assert(contents == "preserve-outside");
        input.close();
        fs::remove(runtime / "engine.new");
    } else {
        std::cout << "SKIP: runtime symlink test requires link creation privilege\n";
    }
    fs::create_hard_link(outside, runtime / "engine.new");
    bool hardLinkRejected = false;
    try { ValidateRuntimePaths(root, hash); }
    catch (const std::runtime_error& error) { hardLinkRejected = std::string(error.what()) == "E_RUNTIME_PATH"; }
    assert(hardLinkRejected);
    { std::ifstream input(outside); std::string contents; std::getline(input, contents);
      assert(contents == "preserve-outside"); }
    fs::remove(runtime / "engine.new");
    fs::create_directory(runtime / "engine.exe");
    bool invalidType = false;
    try { ValidateRuntimePaths(root, hash); }
    catch (const std::runtime_error& error) { invalidType = std::string(error.what()) == "E_RUNTIME_PATH"; }
    assert(invalidType);
    fs::remove(runtime / "engine.exe");
    for (const auto& invalidHash : {std::string("../outside"), std::string(64, 'g')}) {
        bool rejected = false;
        try { ValidateRuntimePaths(root, invalidHash); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    fs::remove_all(root);

    const auto concurrencyRoot = fs::temp_directory_path() / ("ccode-concurrency-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto data = concurrencyRoot / "data";
    const auto profile = data / "profile";
    fs::create_directories(profile);
    const std::string session = "a2345678-1234-4234-8234-123456789abc";
    const auto resumedTurn = PlanSessionTurn(session);
    assert(resumedTurn.id == session && resumedTurn.resume);
    const auto newTurn = PlanSessionTurn("");
    assert(ValidSessionId(newTurn.id) && !newTurn.resume && newTurn.id != session);

    const auto sessionLock = PrepareSessionLockPath(data, profile, session);
    assert(sessionLock == data / "session-locks" / "profile" / (session + ".lock"));
    assert(fs::is_directory(sessionLock.parent_path()));

    const auto candidateRoot = data / "candidates" / "b2345678-1234-4234-8234-123456789abc";
    const auto candidateProfile = candidateRoot / "profile";
    fs::create_directories(candidateProfile);
    const auto selectedCandidateLock = PrepareSessionLockPath(data, candidateProfile, session);
    const auto directCandidateLock = PrepareSessionLockPath(candidateRoot, candidateProfile, session);
    assert(selectedCandidateLock == directCandidateLock);
    assert(selectedCandidateLock == candidateRoot / "session-locks" / "profile" / (session + ".lock"));

    fs::remove_all(data / "session-locks");
    const auto outsideLocks = concurrencyRoot / "outside-locks";
    fs::create_directories(outsideLocks);
    std::error_code lockLinkError;
    fs::create_directory_symlink(outsideLocks, data / "session-locks", lockLinkError);
    if (!lockLinkError) {
        bool rejected = false;
        try { PrepareSessionLockPath(data, profile, session); }
        catch (const std::runtime_error& error) {
            rejected = std::string(error.what()) == "E_SESSION_LOCK_PATH";
        }
        assert(rejected);
        assert(fs::is_empty(outsideLocks));
        fs::remove(data / "session-locks");
        fs::create_directory(data / "session-locks");
        fs::create_directory_symlink(outsideLocks, data / "session-locks" / "profile");
        rejected = false;
        try { PrepareSessionLockPath(data, profile, session); }
        catch (const std::runtime_error& error) {
            rejected = std::string(error.what()) == "E_SESSION_LOCK_PATH";
        }
        assert(rejected);
        assert(fs::is_empty(outsideLocks));
    } else {
        std::cout << "SKIP: session lock symlink test requires link creation privilege\n";
    }

    fs::remove_all(data / "session-locks");
    const auto hardLinkedLock = PrepareSessionLockPath(data, profile, session);
    const auto lockSentinel = concurrencyRoot / "lock-sentinel.txt";
    { std::ofstream output(lockSentinel); output << "preserve-lock-sentinel"; }
    fs::create_hard_link(lockSentinel, hardLinkedLock);
    bool linkedLockRejected = false;
    try { PrepareSessionLockPath(data, profile, session); }
    catch (const std::runtime_error& error) {
        linkedLockRejected = std::string(error.what()) == "E_SESSION_LOCK_PATH";
    }
    assert(linkedLockRejected);
    { std::ifstream input(lockSentinel); std::string contents; std::getline(input, contents);
      assert(contents == "preserve-lock-sentinel"); }

    fs::remove_all(data / "session-locks");
    fs::remove_all(profile);
    const auto outsideProfile = concurrencyRoot / "outside-profile";
    fs::create_directories(outsideProfile);
    std::error_code profileLinkError;
    fs::create_directory_symlink(outsideProfile, profile, profileLinkError);
    if (!profileLinkError) {
        bool profileLinkRejected = false;
        try { PrepareSessionLockPath(data, profile, session); }
        catch (const std::runtime_error& error) {
            profileLinkRejected = std::string(error.what()) == "E_SESSION_LOCK_PATH";
        }
        assert(profileLinkRejected);
    } else {
        std::cout << "SKIP: profile symlink test requires link creation privilege\n";
    }
    fs::remove_all(concurrencyRoot);

    std::cout << "ccode native isolation tests passed\n";
    return 0;
}

int main() {
    try { return RunNativeTests(); }
    catch (const std::exception& error) {
        const std::string code = error.what();
        // Only fixed neutral library codes may escape; filesystem exceptions
        // and other raw diagnostics can contain private fixture paths.
        if (code.size() <= 80 && code.rfind("E_", 0) == 0 &&
            code.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") == std::string::npos)
            std::cerr << code << std::endl;
        else std::cerr << "E_TEST_NATIVE_EXCEPTION" << std::endl;
        return 1;
    }
    catch (...) { std::cerr << "E_TEST_NATIVE_UNKNOWN" << std::endl; return 1; }
}
