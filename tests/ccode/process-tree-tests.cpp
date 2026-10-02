#include "../../scripts/ccode/process-tree.hpp"
#include <cassert>
#include <iostream>
#ifdef _WIN32
#include <string>
static void NativeDescendantTest() {
    wchar_t exe[32768]; assert(GetModuleFileNameW(nullptr, exe, 32768));
    HANDLE job = CreateJobObjectW(nullptr, nullptr); assert(job);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    assert(SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)));
    std::wstring command = L"\"" + std::wstring(exe) + L"\" --spawn-child";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION parent{};
    assert(CreateProcessW(exe, command.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED, nullptr, nullptr, &startup, &parent));
    assert(AssignProcessToJobObject(job, parent.hProcess));
    assert(ResumeThread(parent.hThread) != (DWORD)-1);
    assert(WaitForSingleObject(parent.hProcess, 10000) == WAIT_OBJECT_0);
    DWORD code = 1; assert(GetExitCodeProcess(parent.hProcess, &code) && code == 0);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    assert(QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr));
    assert(info.ActiveProcesses == 1); // orphaned descendant survives root exit
    ccode::StopNativeProcessTree(job, 0);
    assert(QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr));
    assert(info.ActiveProcesses == 0);
    CloseHandle(parent.hThread); CloseHandle(parent.hProcess); CloseHandle(job);
}
#endif
int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc == 2 && std::string(argv[1]) == "--hold") { Sleep(60000); return 0; }
    if (argc == 2 && std::string(argv[1]) == "--spawn-child") {
        wchar_t exe[32768]; if (!GetModuleFileNameW(nullptr, exe, 32768)) return 2;
        std::wstring command = L"\"" + std::wstring(exe) + L"\" --hold";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION child{};
        if (!CreateProcessW(exe, command.data(), nullptr, nullptr, FALSE, 0,
                nullptr, nullptr, &startup, &child)) return 3;
        CloseHandle(child.hThread); CloseHandle(child.hProcess); return 0;
    }
    NativeDescendantTest();
#else
    (void)argc; (void)argv;
#endif
    unsigned active = 2, sleeps = 0; bool terminated = false;
    ccode::StopProcessTree([&] { terminated = true; return true; },
        [&] { assert(terminated); return active; }, [&] { return sleeps; },
        [&] { ++sleeps; --active; }, 10);
    assert(terminated && active == 0 && sleeps == 2);
    bool rejected = false;
    try { ccode::StopProcessTree([] { return false; }, [] { return 0u; },
        [] { return 0u; }, [] {}, 10); }
    catch (const std::runtime_error& e) { rejected = std::string(e.what()) == "E_PROCESS_TREE"; }
    assert(rejected);
    unsigned clock = 0; rejected = false;
    try { ccode::StopProcessTree([] { return true; }, [] { return 1u; },
        [&] { return clock; }, [&] { ++clock; }, 3); }
    catch (const std::runtime_error& e) { rejected = std::string(e.what()) == "E_PROCESS_TREE"; }
    assert(rejected && clock == 3);
    std::cout << "ccode process tree shutdown tests passed\n";
}
