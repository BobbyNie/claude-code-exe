#include "../../scripts/ccode/common.hpp"
#include "../../scripts/ccode/runtime-paths.hpp"
#include <fstream>
#include <chrono>

#include <cassert>
#include <iostream>

int main() {
    using namespace ccode;

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

    namespace fs = std::filesystem;
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

    std::cout << "ccode native isolation tests passed\n";
    return 0;
}
