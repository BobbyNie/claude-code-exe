#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>

#include "common.hpp"

namespace fs = std::filesystem;

namespace {
constexpr int kPayloadResource = 101;
constexpr int kHookResource = 102;

std::wstring Quote(const std::wstring& value) {
    std::wstring result = L"\"";
    unsigned backslashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(ch);
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(ch);
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

bool ExtractResource(int id, const fs::path& destination) {
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!resource) return false;
    HGLOBAL loaded = LoadResource(nullptr, resource);
    const void* bytes = loaded ? LockResource(loaded) : nullptr;
    const DWORD size = SizeofResource(nullptr, resource);
    if (!bytes || size == 0) return false;

    fs::create_directories(destination.parent_path());
    const fs::path temporary = destination.wstring() + L".new";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, bytes, size, &written, nullptr);
    CloseHandle(file);
    if (!ok || written != size) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    return true;
}

std::wstring ReadRequired(const wchar_t* name) {
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (length == 0) return {};
    std::wstring value(length, L'\0');
    GetEnvironmentVariableW(name, value.data(), length);
    value.resize(length - 1);
    return value;
}

void RemoveOriginalEnvironmentPrefixes() {
    LPWCH block = GetEnvironmentStringsW();
    if (!block) return;
    for (const wchar_t* cursor = block; *cursor; cursor += wcslen(cursor) + 1) {
        std::wstring entry(cursor);
        const size_t equals = entry.find(L'=');
        if (equals == std::wstring::npos || equals == 0) continue;
        const std::wstring name = entry.substr(0, equals);
        if (ccode::StartsWithInsensitive(name, L"ANTHROPIC_") ||
            ccode::StartsWithInsensitive(name, L"CLAUDE_CODE_")) {
            SetEnvironmentVariableW(name.c_str(), nullptr);
        }
    }
    FreeEnvironmentStringsW(block);
}

bool InjectDll(HANDLE process, const fs::path& dllPath) {
    const std::wstring path = dllPath.wstring();
    const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) return false;
    if (!WriteProcessMemory(process, remote, path.c_str(), bytes, nullptr)) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }
    FARPROC procedure = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    LPTHREAD_START_ROUTINE loadLibrary = nullptr;
    static_assert(sizeof(procedure) == sizeof(loadLibrary));
    std::memcpy(&loadLibrary, &procedure, sizeof(loadLibrary));
    if (!loadLibrary) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr);
    if (!thread) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }
    WaitForSingleObject(thread, INFINITE);
    DWORD result = 0;
    GetExitCodeThread(thread, &result);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return result != 0;
}

bool IsBlockedCommand(int argc, wchar_t** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = ccode::Lower(argv[i]);
        if (arg == L"login" || arg == L"logout" || arg == L"setup-token" ||
            ccode::ContainsForbiddenText(arg)) return true;
    }
    return false;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    wchar_t module[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, module, MAX_PATH)) return 70;
    const fs::path exeDir = fs::path(module).parent_path();
    const fs::path root = exeDir / L"data" / L"cc";
    const fs::path runtime = root / L"runtime";
    const fs::path payload = runtime / L"aa-runtime.bin";
    const fs::path hook = runtime / L"cc-runtime.dll";

    if (argc == 2 && std::wstring(argv[1]) == L"--ccode-self-test") {
        std::wcout << L"ccode self-test ok\n";
        return ccode::ContainsForbiddenText(root.wstring()) ? 1 : 0;
    }

    if (ccode::ContainsForbiddenText(exeDir.wstring())) {
        std::wcerr << L"The executable directory contains a reserved product name.\n";
        return 65;
    }

    const std::wstring apiKey = ReadRequired(L"A_API_KEY");
    const std::wstring baseUrl = ReadRequired(L"A_BASE_URL");
    if (apiKey.empty()) {
        std::wcerr << L"A_API_KEY is required.\n";
        return 64;
    }
    if (!ccode::IsValidGatewayUrl(baseUrl)) {
        std::wcerr << L"A_BASE_URL is required.\n";
        return 64;
    }
    if (IsBlockedCommand(argc, argv)) {
        std::wcerr << L"Interactive account commands are disabled; use A_API_KEY.\n";
        return 64;
    }

    wchar_t current[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, current);
    if (ccode::ContainsForbiddenText(current)) {
        std::wcerr << L"The working directory contains a reserved product name.\n";
        return 65;
    }

    if (!ExtractResource(kPayloadResource, payload) || !ExtractResource(kHookResource, hook)) {
        std::wcerr << L"Unable to prepare the isolated runtime.\n";
        return 74;
    }

    const fs::path profile = root / L"profile";
    fs::create_directories(profile / L"home");
    fs::create_directories(profile / L"roaming");
    fs::create_directories(profile / L"local");
    fs::create_directories(profile / L"temp");
    SetEnvironmentVariableW(L"HOME", (profile / L"home").c_str());
    SetEnvironmentVariableW(L"USERPROFILE", (profile / L"home").c_str());
    SetEnvironmentVariableW(L"APPDATA", (profile / L"roaming").c_str());
    SetEnvironmentVariableW(L"LOCALAPPDATA", (profile / L"local").c_str());
    SetEnvironmentVariableW(L"TEMP", (profile / L"temp").c_str());
    SetEnvironmentVariableW(L"TMP", (profile / L"temp").c_str());
    SetEnvironmentVariableW(L"C_DISABLE_NONESSENTIAL_TRAFFIC", L"1");
    SetEnvironmentVariableW(L"C_DISABLE_AUTOUPDATER", L"1");
    SetEnvironmentVariableW(L"DISABLE_AUTOUPDATER", L"1");
    RemoveOriginalEnvironmentPrefixes();

    std::wstring command = Quote(payload.wstring());
    for (int i = 1; i < argc; ++i) command += L" " + Quote(argv[i]);
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(payload.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, nullptr, current,
                        &startup, &child)) {
        std::wcerr << L"Unable to start the isolated runtime.\n";
        return 71;
    }
    if (!InjectDll(child.hProcess, hook)) {
        TerminateProcess(child.hProcess, 72);
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        std::wcerr << L"Unable to initialize the isolation layer.\n";
        return 72;
    }
    ResumeThread(child.hThread);
    WaitForSingleObject(child.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(child.hProcess, &exitCode);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    return static_cast<int>(exitCode);
}
