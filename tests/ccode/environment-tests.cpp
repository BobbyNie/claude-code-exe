#include "../../scripts/ccode/environment.hpp"
#include <cassert>
#include <iostream>

int main() {
    const auto env = ccode::BuildEnvironment({
        L"Path=C:\\tools", L"A_AUTH_TOKEN=secret", L"A_BASE_URL=http://gateway.test",
        L"ANTHROPIC_AUTH_TOKEN=stale", L"C_SUBAGENT_MODEL=small",
        L"CLAUDE_CONFIG_DIR=C:\\wrong", L"=C:=C:\\project",
        L"C_MAX_RETRIES=10", L"C_RETRY_WATCHDOG=1",
        L"C_DISABLE_NONSTREAMING_FALLBACK=0"
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
    std::cout << "native child environment passed\n";
}
