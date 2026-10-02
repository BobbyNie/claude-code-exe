#pragma once
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ccode {
// Termination requests are asynchronous: callers may release workspace state
// only after the job reports no active processes. Failure is never success.
template<class Terminate, class Active, class Clock, class Pause>
void StopProcessTree(Terminate terminate, Active active, Clock clock, Pause pause,
                     unsigned timeoutMilliseconds) {
    if (!terminate()) throw std::runtime_error("E_PROCESS_TREE");
    const auto start = clock();
    while (active() != 0) {
        if (clock() - start >= timeoutMilliseconds)
            throw std::runtime_error("E_PROCESS_TREE");
        pause();
    }
}
#ifdef _WIN32
inline void StopNativeProcessTree(HANDLE job, DWORD exitCode) {
    StopProcessTree([&] { return TerminateJobObject(job, exitCode) != FALSE; },
        [&] {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
            if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                    &info, sizeof(info), nullptr))
                throw std::runtime_error("E_PROCESS_TREE");
            return info.ActiveProcesses;
        }, [] { return GetTickCount64(); }, [] { Sleep(1); }, 10000);
}
#endif
}
