#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {
bool ContainsEntry(LPWCH block, const std::wstring& expected) {
    for (const wchar_t* cursor = block; cursor && *cursor; cursor += wcslen(cursor) + 1) {
        if (expected == cursor) return true;
    }
    return false;
}

int Fail(const char* message) {
    std::cerr << message << " (Windows error " << GetLastError() << ")\n";
    return 1;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return Fail("Expected the isolation DLL path");

    _wputenv_s(L"A_AUTH_TOKEN", L"vendor-token");
    _wputenv_s(L"A_BASE_URL", L"https://api.deepseek.com/anthropic");
    _wputenv_s(L"C_SUBAGENT_MODEL", L"vendor-model");

    LPWCH stored = GetEnvironmentStringsW();
    if (!ContainsEntry(stored, L"A_AUTH_TOKEN=vendor-token") ||
        ContainsEntry(stored, L"ANTHROPIC_AUTH_TOKEN=vendor-token")) {
        FreeEnvironmentStringsW(stored);
        return Fail("Stored environment was not alias-only before injection");
    }
    FreeEnvironmentStringsW(stored);

    if (!LoadLibraryW(L"ws2_32.dll")) return Fail("Failed to load Winsock");
    if (!LoadLibraryW(argv[1])) return Fail("Failed to load the isolation DLL");

    wchar_t value[128]{};
    if (GetEnvironmentVariableW(L"ANTHROPIC_AUTH_TOKEN", value, 128) == 0 ||
        std::wstring(value) != L"vendor-token") {
        return Fail("Mapped environment lookup failed");
    }

    const char* crtValue = std::getenv("ANTHROPIC_AUTH_TOKEN");
    if (!crtValue || std::strcmp(crtValue, "vendor-token") != 0) {
        return Fail("Mapped CRT environment lookup failed");
    }

    LPWCH expanded = GetEnvironmentStringsW();
    const bool hasToken = ContainsEntry(expanded, L"ANTHROPIC_AUTH_TOKEN=vendor-token");
    const bool hasBase = ContainsEntry(
        expanded, L"ANTHROPIC_BASE_URL=https://api.deepseek.com/anthropic");
    const bool hasModel = ContainsEntry(expanded, L"CLAUDE_CODE_SUBAGENT_MODEL=vendor-model");
    const bool leakedAlias = ContainsEntry(expanded, L"A_AUTH_TOKEN=vendor-token");
    FreeEnvironmentStringsW(expanded);

    if (!hasToken || !hasBase || !hasModel || leakedAlias) {
        return Fail("Expanded environment enumeration failed");
    }

    std::cout << "ccode hook integration tests passed\n";
    return 0;
}
