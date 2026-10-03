#include "../../scripts/ccode/environment.hpp"
#include "../../scripts/ccode/boundary.hpp"
#include <cassert>
#include <iostream>

int main() {
    const auto env = ccode::BuildEnvironment({
        L"Path=C:\\tools", L"A_AUTH_TOKEN=secret", L"A_BASE_URL=http://gateway.test",
        L"ANTHROPIC_AUTH_TOKEN=stale", L"C_SUBAGENT_MODEL=small",
        L"CLAUDE_CONFIG_DIR=C:\\wrong", L"=C:=C:\\project",
        L"C_MAX_RETRIES=10", L"C_RETRY_WATCHDOG=1",
        L"C_DISABLE_NONSTREAMING_FALLBACK=0", L"node_tls_reject_unauthorized=0"
    }, L"D:\\profile");
    auto has = [&](const std::wstring& value) {
        return std::find(env.begin(), env.end(), value) != env.end();
    };
    assert(has(L"ANTHROPIC_AUTH_TOKEN=secret"));
    assert(!has(L"ANTHROPIC_AUTH_TOKEN=stale"));
    assert(has(L"CLAUDE_CODE_SUBAGENT_MODEL=small"));
    assert(!has(L"CLAUDE_CONFIG_DIR=C:\\wrong"));
    assert(has(L"HOME=" + (std::filesystem::path(L"D:\\profile") / L"home").wstring()));
    assert(has(L"Path=C:\\tools"));
    assert(has(L"=C:=C:\\project"));
    assert(has(L"DISABLE_AUTOUPDATER=1"));
    // Caller configuration must not re-enable automatic request replay.
    assert(has(L"CLAUDE_CODE_MAX_RETRIES=0"));
    assert(has(L"CLAUDE_CODE_RETRY_WATCHDOG=0"));
    assert(has(L"CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK=1"));
    assert(has(L"NODE_TLS_REJECT_UNAUTHORIZED=1"));
    assert(!has(L"node_tls_reject_unauthorized=0"));

    // Only the child bridge hop bypasses proxies. Preserve upstream/tool proxy
    // settings and caller exclusions, and never mutate the inherited block.
    const std::vector<std::wstring> inheritedProxy = {
        L"https_proxy=http://proxy.invalid:8080", L"HTTP_PROXY=http://proxy.invalid:8081",
        L"no_proxy=corp.internal .example.test", L"A_BASE_URL=https://upstream.test",
        L"ANTHROPIC_BASE_URL=https://stale.test"};
    const auto bridgeEnv = ccode::BuildEnvironment(inheritedProxy, L"D:\\profile",
        L"http://127.0.0.1:1234/capability");
    const auto contains = [](const auto& block, const std::wstring& value) {
        return std::find(block.begin(), block.end(), value) != block.end();
    };
    assert(contains(bridgeEnv, L"NO_PROXY=corp.internal .example.test,127.0.0.1"));
    assert(contains(bridgeEnv, L"https_proxy=http://proxy.invalid:8080"));
    assert(contains(bridgeEnv, L"HTTP_PROXY=http://proxy.invalid:8081"));
    assert(contains(bridgeEnv, L"A_BASE_URL=http://127.0.0.1:1234/capability"));
    assert(contains(bridgeEnv, L"ANTHROPIC_BASE_URL=http://127.0.0.1:1234/capability"));
    assert(contains(inheritedProxy, L"no_proxy=corp.internal .example.test"));
    assert(!contains(bridgeEnv, L"no_proxy=corp.internal .example.test"));
    for (const auto& inherited : {std::vector<std::wstring>{},
                                 std::vector<std::wstring>{L"NO_PROXY="}}) {
        const auto child = ccode::BuildEnvironment(inherited, L"profile", L"http://127.0.0.1:1234/capability");
        assert(contains(child, L"NO_PROXY=127.0.0.1"));
    }
    const auto noBridge = ccode::BuildEnvironment(inheritedProxy, L"profile");
    assert(contains(noBridge, L"no_proxy=corp.internal .example.test"));
    assert(contains(noBridge, L"ANTHROPIC_BASE_URL=https://upstream.test"));
    assert(contains(ccode::BuildEnvironment({L"NO_PROXY=*"}, L"profile", L"http://127.0.0.1:1234/capability"),
                    L"NO_PROXY=*,127.0.0.1"));

    const auto boundary = ccode::BoundaryManifest();
    assert(boundary.at("schemaVersion") == 1);
    assert(boundary.at("platform") == "windows");
    assert(boundary.at("architecture") == "x64");
    assert(boundary.at("minimumWindowsBuild") == 22000);
    assert(boundary.at("publicEnvironment").at("acceptedExact") ==
        ccode::Json::array({"A_API_KEY", "A_AUTH_TOKEN", "A_BASE_URL", "CCODE_DATA_DIR"}));
    assert(boundary.at("publicEnvironment").at("acceptedPrefixes") ==
        ccode::Json::array({"A_", "C_"}));
    assert(boundary.at("publicEnvironment").at("valuesRecorded") == false);
    assert(boundary.at("childRuntimeEnvironment").at("aliasExpansion") == ccode::Json::array({
        {{"publicPrefix", "A_"}, {"runtimePrefix", "ANTHROPIC_"}},
        {{"publicPrefix", "C_"}, {"runtimePrefix", "CLAUDE_CODE_"}},
    }));
    assert(boundary.at("childRuntimeEnvironment").at("fixedValues").at(
        "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC") == "1");
    assert(boundary.at("childRuntimeEnvironment").at("fixedValues").at(
        "CLAUDE_CODE_MAX_RETRIES") == "0");
    assert(boundary.at("childRuntimeEnvironment").at("fixedValues").at(
        "NODE_TLS_REJECT_UNAUTHORIZED") == "1");
    assert(boundary.at("childRuntimeEnvironment").at("originalRuntimeNamesPresent") == true);
    assert(boundary.at("childRuntimeEnvironment").at("processTreeNameFree") == false);
    assert(boundary.at("binaryMetadata").at("peResources").at(0).at("id") == 101);
    assert(boundary.at("binaryMetadata").at("peResources").at(1).at("id") == 102);
    assert(boundary.at("binaryMetadata").at("publisherSignature") == "not-asserted");
    assert(boundary.at("notices").at("source") == "enterprise-package-manifest");
    assert(boundary.at("sideEffects").at("createsData") == false);
    const auto serialized = boundary.dump();
    assert(serialized.find("secret") == std::string::npos);
    assert(serialized.find("gateway.test") == std::string::npos);
    std::cout << "native child environment passed\n";
}
