#include "../../scripts/ccode/process-tree.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#ifdef _WIN32
#include <string>
static void NativeDescendantTest() {
    wchar_t exe[32768]; assert(GetModuleFileNameW(nullptr, exe, 32768));
    wchar_t temporary[MAX_PATH], copied[MAX_PATH];
    assert(GetTempPathW(MAX_PATH, temporary));
    assert(GetTempFileNameW(temporary, L"cpt", 0, copied));
    assert(CopyFileW(exe, copied, FALSE));
    HANDLE job = CreateJobObjectW(nullptr, nullptr); assert(job);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    assert(SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)));
    std::wstring command = L"\"" + std::wstring(copied) + L"\" --spawn-child";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION parent{};
    assert(CreateProcessW(copied, command.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED, nullptr, nullptr, &startup, &parent));
    assert(AssignProcessToJobObject(job, parent.hProcess));
    assert(ResumeThread(parent.hThread) != (DWORD)-1);
    assert(WaitForSingleObject(parent.hProcess, 10000) == WAIT_OBJECT_0);
    DWORD code = 1; assert(GetExitCodeProcess(parent.hProcess, &code) && code == 0);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    assert(QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr));
    // A signalled root handle can precede the job's accounting update. Wait
    // for that update, not for the descendant to exit; zero remains a failure.
    const auto accountingDeadline = GetTickCount64() + 5000;
    while (info.ActiveProcesses > 1 && GetTickCount64() < accountingDeadline) {
        Sleep(10);
        assert(QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr));
    }
    assert(info.ActiveProcesses == 1); // orphaned descendant survives root exit
    assert(!DeleteFileW(copied)); // Descendant still maps its executable image.
    ccode::StopNativeProcessTree(job, 0);
    assert(DeleteFileW(copied)); // No retry/ignored cleanup after return.
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
    // Job accounting reaching zero does not substitute for process-object
    // waits. All captured members must signal, within one aggregate deadline.
    unsigned ticks = 0;
    std::vector<int> waited;
    ccode::WaitProcessTreeHandles(std::vector<int>{11, 22},
        [&](int handle, unsigned remaining) {
            waited.push_back(handle); assert(remaining == 10 - ticks); ticks += 2; return true;
        }, [&] { return ticks; }, 10);
    assert((waited == std::vector<int>{11, 22}));
    bool waitRejected = false;
    try { ccode::WaitProcessTreeHandles(std::vector<int>{11},
        [](int, unsigned) { return false; }, [] { return 0u; }, 10); }
    catch (const std::runtime_error& error) { waitRejected = std::string(error.what()) == "E_PROCESS_TREE"; }
    assert(waitRejected);
    ticks = 0; waitRejected = false;
    try { ccode::WaitProcessTreeHandles(std::vector<int>{11, 22},
        [&](int, unsigned) { ticks += 10; return true; }, [&] { return ticks; }, 10); }
    catch (const std::runtime_error& error) { waitRejected = std::string(error.what()) == "E_PROCESS_TREE"; }
    assert(waitRejected);
    ticks = 0; waitRejected = false;
    try { ccode::StopProcessTree([&] { ticks = 10; return true; },
        [] { return 1u; }, [&] { return ticks; }, [&] { ++ticks; }, 10); }
    catch (const std::runtime_error&) { waitRejected = true; }
    assert(waitRejected && ticks == 10); // Termination wait shares the deadline.
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
