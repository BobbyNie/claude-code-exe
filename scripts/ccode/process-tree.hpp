#pragma once
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ccode {
template<class Handles, class Wait, class Clock>
void WaitProcessTreeHandles(const Handles& handles, Wait wait, Clock clock, unsigned timeoutMilliseconds) {
    const auto start = clock();
    for (const auto handle : handles) {
        const auto elapsed = clock() - start;
        if (elapsed >= timeoutMilliseconds || !wait(handle, timeoutMilliseconds - static_cast<unsigned>(elapsed)))
            throw std::runtime_error("E_PROCESS_TREE");
    }
}
// Termination requests are asynchronous: callers may release workspace state
// only after the job reports no active processes. Failure is never success.
template<class Terminate, class Active, class Clock, class Pause>
void StopProcessTree(Terminate terminate, Active active, Clock clock, Pause pause,
                     unsigned timeoutMilliseconds) {
    const auto start = clock();
    if (!terminate()) throw std::runtime_error("E_PROCESS_TREE");
    while (active() != 0) {
        if (clock() - start >= timeoutMilliseconds)
            throw std::runtime_error("E_PROCESS_TREE");
        pause();
    }
}
#ifdef _WIN32
inline void StopNativeProcessTree(HANDLE job, DWORD exitCode) {
    // Retain identity-checked member handles BEFORE requesting asynchronous
    // termination. Job accounting alone is not a process-object wait.
    struct Members {
        std::vector<HANDLE> values;
        ~Members() { for (auto handle : values) CloseHandle(handle); }
    } members;
    for (size_t capacity = 16;; capacity *= 2) {
        if (capacity > 65536) throw std::runtime_error("E_PROCESS_TREE");
        std::vector<ULONG_PTR> storage(capacity + 2);
        auto ids = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(storage.data());
        const bool queried = QueryInformationJobObject(job, JobObjectBasicProcessIdList,
            ids, static_cast<DWORD>(storage.size() * sizeof(ULONG_PTR)), nullptr) != FALSE;
        if (!queried) {
            if (GetLastError() == ERROR_MORE_DATA) continue;
            throw std::runtime_error("E_PROCESS_TREE");
        }
        if (ids->NumberOfProcessIdsInList != ids->NumberOfAssignedProcesses ||
            ids->NumberOfProcessIdsInList > capacity) continue;
        members.values.reserve(ids->NumberOfProcessIdsInList);
        for (DWORD index = 0; index < ids->NumberOfProcessIdsInList; ++index) {
            HANDLE handle = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                static_cast<DWORD>(ids->ProcessIdList[index]));
            if (!handle) {
                if (GetLastError() == ERROR_INVALID_PARAMETER) continue; // Object already gone.
                throw std::runtime_error("E_PROCESS_TREE");
            }
            BOOL member = FALSE;
            if (!IsProcessInJob(handle, job, &member)) {
                CloseHandle(handle); throw std::runtime_error("E_PROCESS_TREE");
            }
            if (!member) { CloseHandle(handle); continue; } // PID reused outside this job.
            members.values.push_back(handle);
        }
        break;
    }
    StopProcessTree([&] {
            if (!TerminateJobObject(job, exitCode)) return false;
            WaitProcessTreeHandles(members.values, [](HANDLE handle, unsigned remaining) {
                return WaitForSingleObject(handle, remaining) == WAIT_OBJECT_0;
            }, [] { return GetTickCount64(); }, 10000);
            return true;
        },
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
