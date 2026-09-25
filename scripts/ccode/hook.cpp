#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ws2tcpip.h>

#include <cstring>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

#include "MinHook.h"
#include "common.hpp"

namespace {
using GetEnvironmentStringsAFn = LPCH(WINAPI*)();

decltype(&GetEnvironmentVariableW) OriginalGetEnvironmentVariableW = nullptr;
decltype(&GetEnvironmentVariableA) OriginalGetEnvironmentVariableA = nullptr;
decltype(&GetEnvironmentStringsW) OriginalGetEnvironmentStringsW = nullptr;
GetEnvironmentStringsAFn OriginalGetEnvironmentStringsA = nullptr;
decltype(&FreeEnvironmentStringsW) OriginalFreeEnvironmentStringsW = nullptr;
decltype(&FreeEnvironmentStringsA) OriginalFreeEnvironmentStringsA = nullptr;
decltype(&SetEnvironmentVariableW) OriginalSetEnvironmentVariableW = nullptr;
decltype(&SetEnvironmentVariableA) OriginalSetEnvironmentVariableA = nullptr;
decltype(&getenv) OriginalGetEnv = nullptr;
decltype(&_wgetenv) OriginalGetWEnv = nullptr;
decltype(&GetAddrInfoW) OriginalGetAddrInfoW = nullptr;
decltype(&getaddrinfo) OriginalGetAddrInfoA = nullptr;

SRWLOCK SyntheticEnvironmentLock = SRWLOCK_INIT;
std::unordered_set<void*> SyntheticEnvironmentBlocks;

std::wstring Widen(const char* value) {
    if (!value) return {};
    const int size = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
    std::wstring result(size > 0 ? size : 0, L'\0');
    if (size > 1) MultiByteToWideChar(CP_ACP, 0, value, -1, result.data(), size);
    if (!result.empty()) result.pop_back();
    return result;
}

std::string Narrow(const std::wstring& value) {
    const int size = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size > 0 ? size : 0, '\0');
    if (size > 1) WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    if (!result.empty()) result.pop_back();
    return result;
}

std::wstring ExpandEnvironmentEntry(const std::wstring& entry) {
    const size_t equals = entry.find(L'=');
    if (equals == std::wstring::npos || equals == 0) return entry;
    return ccode::ExpandEnvironmentName(entry.substr(0, equals)) + entry.substr(equals);
}

bool RegisterSyntheticEnvironmentBlock(void* block) {
    try {
        AcquireSRWLockExclusive(&SyntheticEnvironmentLock);
        SyntheticEnvironmentBlocks.insert(block);
        ReleaseSRWLockExclusive(&SyntheticEnvironmentLock);
        return true;
    } catch (...) {
        ReleaseSRWLockExclusive(&SyntheticEnvironmentLock);
        return false;
    }
}

bool ReleaseSyntheticEnvironmentBlock(void* block) {
    AcquireSRWLockExclusive(&SyntheticEnvironmentLock);
    const auto found = SyntheticEnvironmentBlocks.find(block);
    if (found == SyntheticEnvironmentBlocks.end()) {
        ReleaseSRWLockExclusive(&SyntheticEnvironmentLock);
        return false;
    }
    SyntheticEnvironmentBlocks.erase(found);
    ReleaseSRWLockExclusive(&SyntheticEnvironmentLock);
    HeapFree(GetProcessHeap(), 0, block);
    return true;
}

LPWCH WINAPI HookGetEnvironmentStringsW() {
    LPWCH original = OriginalGetEnvironmentStringsW();
    if (!original) return nullptr;

    std::vector<std::wstring> entries;
    size_t characters = 1;
    for (const wchar_t* cursor = original; *cursor; cursor += wcslen(cursor) + 1) {
        entries.push_back(ExpandEnvironmentEntry(cursor));
        characters += entries.back().size() + 1;
    }

    auto* mapped = static_cast<wchar_t*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, characters * sizeof(wchar_t)));
    if (!mapped) return original;

    wchar_t* output = mapped;
    for (const auto& entry : entries) {
        std::memcpy(output, entry.c_str(), entry.size() * sizeof(wchar_t));
        output += entry.size() + 1;
    }
    if (!RegisterSyntheticEnvironmentBlock(mapped)) {
        HeapFree(GetProcessHeap(), 0, mapped);
        return original;
    }
    OriginalFreeEnvironmentStringsW(original);
    return mapped;
}

LPCH WINAPI HookGetEnvironmentStringsA() {
    LPCH original = OriginalGetEnvironmentStringsA();
    if (!original) return nullptr;

    std::vector<std::string> entries;
    size_t characters = 1;
    for (const char* cursor = original; *cursor; cursor += strlen(cursor) + 1) {
        entries.push_back(Narrow(ExpandEnvironmentEntry(Widen(cursor))));
        characters += entries.back().size() + 1;
    }

    auto* mapped = static_cast<char*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, characters));
    if (!mapped) return original;

    char* output = mapped;
    for (const auto& entry : entries) {
        std::memcpy(output, entry.c_str(), entry.size());
        output += entry.size() + 1;
    }
    if (!RegisterSyntheticEnvironmentBlock(mapped)) {
        HeapFree(GetProcessHeap(), 0, mapped);
        return original;
    }
    OriginalFreeEnvironmentStringsA(original);
    return mapped;
}

BOOL WINAPI HookFreeEnvironmentStringsW(LPWCH block) {
    return ReleaseSyntheticEnvironmentBlock(block) ? TRUE : OriginalFreeEnvironmentStringsW(block);
}

BOOL WINAPI HookFreeEnvironmentStringsA(LPCH block) {
    return ReleaseSyntheticEnvironmentBlock(block) ? TRUE : OriginalFreeEnvironmentStringsA(block);
}

DWORD WINAPI HookGetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD size) {
    const std::wstring mapped = ccode::MapEnvironmentName(name ? name : L"");
    return OriginalGetEnvironmentVariableW(mapped.c_str(), buffer, size);
}

DWORD WINAPI HookGetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size) {
    const std::string mapped = Narrow(ccode::MapEnvironmentName(Widen(name)));
    return OriginalGetEnvironmentVariableA(mapped.c_str(), buffer, size);
}

BOOL WINAPI HookSetEnvironmentVariableW(LPCWSTR name, LPCWSTR value) {
    const std::wstring mapped = ccode::MapEnvironmentName(name ? name : L"");
    return OriginalSetEnvironmentVariableW(mapped.c_str(), value);
}

BOOL WINAPI HookSetEnvironmentVariableA(LPCSTR name, LPCSTR value) {
    const std::string mapped = Narrow(ccode::MapEnvironmentName(Widen(name)));
    return OriginalSetEnvironmentVariableA(mapped.c_str(), value);
}

char* __cdecl HookGetEnv(const char* name) {
    const std::string mapped = Narrow(ccode::MapEnvironmentName(Widen(name)));
    return OriginalGetEnv(mapped.c_str());
}

wchar_t* __cdecl HookGetWEnv(const wchar_t* name) {
    const std::wstring mapped = ccode::MapEnvironmentName(name ? name : L"");
    return OriginalGetWEnv(mapped.c_str());
}

INT WSAAPI HookGetAddrInfoW(PCWSTR node, PCWSTR service, const ADDRINFOW* hints, PADDRINFOW* result) {
    wchar_t gateway[2048];
    const DWORD length = OriginalGetEnvironmentVariableW(L"A_BASE_URL", gateway, 2048);
    if (node && (length == 0 || length >= 2048 || !ccode::IsAllowedNetworkHost(node, gateway))) return EAI_NONAME;
    return OriginalGetAddrInfoW(node, service, hints, result);
}

INT WSAAPI HookGetAddrInfoA(PCSTR node, PCSTR service, const addrinfo* hints, addrinfo** result) {
    wchar_t gateway[2048];
    const DWORD length = OriginalGetEnvironmentVariableW(L"A_BASE_URL", gateway, 2048);
    if (node && (length == 0 || length >= 2048 || !ccode::IsAllowedNetworkHost(Widen(node), gateway))) return EAI_NONAME;
    return OriginalGetAddrInfoA(node, service, hints, result);
}

template <typename T>
bool Install(LPCWSTR module, const char* name, T hook, T* original) {
    HMODULE handle = GetModuleHandleW(module);
    if (!handle) return false;
    LPVOID target = reinterpret_cast<LPVOID>(GetProcAddress(handle, name));
    LPVOID hookAddress = nullptr;
    LPVOID originalAddress = nullptr;
    static_assert(sizeof(hook) == sizeof(hookAddress));
    std::memcpy(&hookAddress, &hook, sizeof(hookAddress));
    if (!target || MH_CreateHook(target, hookAddress, &originalAddress) != MH_OK) return false;
    std::memcpy(original, &originalAddress, sizeof(originalAddress));
    return true;
}

bool InstallHooks() {
    if (MH_Initialize() != MH_OK) return false;
    bool ok = true;
    ok &= Install(L"kernel32.dll", "GetEnvironmentVariableW", HookGetEnvironmentVariableW, &OriginalGetEnvironmentVariableW);
    ok &= Install(L"kernel32.dll", "GetEnvironmentVariableA", HookGetEnvironmentVariableA, &OriginalGetEnvironmentVariableA);
    ok &= Install(L"kernel32.dll", "GetEnvironmentStringsW", HookGetEnvironmentStringsW, &OriginalGetEnvironmentStringsW);
    ok &= Install(L"kernel32.dll", "GetEnvironmentStringsA", HookGetEnvironmentStringsA, &OriginalGetEnvironmentStringsA);
    ok &= Install(L"kernel32.dll", "FreeEnvironmentStringsW", HookFreeEnvironmentStringsW, &OriginalFreeEnvironmentStringsW);
    ok &= Install(L"kernel32.dll", "FreeEnvironmentStringsA", HookFreeEnvironmentStringsA, &OriginalFreeEnvironmentStringsA);
    ok &= Install(L"kernel32.dll", "SetEnvironmentVariableW", HookSetEnvironmentVariableW, &OriginalSetEnvironmentVariableW);
    ok &= Install(L"kernel32.dll", "SetEnvironmentVariableA", HookSetEnvironmentVariableA, &OriginalSetEnvironmentVariableA);
    ok &= Install(L"ucrtbase.dll", "getenv", HookGetEnv, &OriginalGetEnv);
    ok &= Install(L"ucrtbase.dll", "_wgetenv", HookGetWEnv, &OriginalGetWEnv);
    // Preserve filesystem paths across Win32, NT APIs, and unhooked children.
    ok &= Install(L"ws2_32.dll", "GetAddrInfoW", HookGetAddrInfoW, &OriginalGetAddrInfoW);
    ok &= Install(L"ws2_32.dll", "getaddrinfo", HookGetAddrInfoA, &OriginalGetAddrInfoA);
    return ok && MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}
}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (!InstallHooks()) return FALSE;
    }
    return TRUE;
}
