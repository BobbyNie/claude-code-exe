#include "../../scripts/ccode/common.hpp"
#include "../../scripts/ccode/runtime-paths.hpp"
#include "../../scripts/ccode/extraction-errors.hpp"
#include "../../scripts/ccode/concurrency.hpp"
#include "../../scripts/ccode/workspace-boundary.hpp"
#include <fstream>
#include <chrono>

#include <cassert>
#include <iostream>

int main() {
    using namespace ccode;
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
