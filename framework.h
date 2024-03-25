#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <winbase.h>
#include <memory>
#include <thread>
#include <algorithm>
#include <vector>

static constexpr unsigned MaxLogicalProcessorsPerGroup =
std::numeric_limits<KAFFINITY>::digits;

bool IsNUMA() noexcept
{
    ULONG HighestNodeNumber;
    return !(!GetNumaHighestNodeNumber(&HighestNodeNumber) || HighestNodeNumber == 0);
}

int __g_ProcGroupCount = 0;
int __g_ProcLogicalThreadCount = 0;

int calcLogicalGroups()
{
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &length);

    std::unique_ptr<void, void (*)(void*)> buffer(std::malloc(length), std::free);

    auto* mem = reinterpret_cast<unsigned char*>(buffer.get());
    GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem), &length);

    DWORD i = 0;
    while (i < length) {
        const auto* proc = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem + i);
        return proc->Processor.GroupCount;
    }
}

// This implementation supports both conventional single-cpu PC configurations
// and multi-cpu system on NUMA (Non-uniform_memory_access) architecture
// Modified by GermanAizek
unsigned int GetLogicalThreadCount() noexcept
{
    DWORD length = 0;
    const auto single_cpu_concurrency = []() noexcept -> unsigned int {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return info.dwNumberOfProcessors;
        };

    if (GetLogicalProcessorInformationEx(RelationAll, nullptr, &length) != FALSE) {
        return single_cpu_concurrency();
    }

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return single_cpu_concurrency();
    }

    std::unique_ptr<void, void (*)(void*)> buffer(std::malloc(length), std::free);
    if (!buffer) {
        return single_cpu_concurrency();
    }

    auto* mem = reinterpret_cast<unsigned char*>(buffer.get());
    if (GetLogicalProcessorInformationEx(
        RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem), &length) == false) {
        return single_cpu_concurrency();
    }

    DWORD i = 0;
    unsigned int concurrency = 0;
    while (i < length) {
        const auto* proc = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem + i);
        if (proc->Relationship == RelationProcessorCore) {
            for (WORD group = 0; group < proc->Processor.GroupCount; ++group) {
                for (KAFFINITY mask = proc->Processor.GroupMask[group].Mask; mask != 0; mask >>= 1) {
                    concurrency += mask & 1;
                }
            }
        }
        i += proc->Size;
    }

    return concurrency;
}

void setThreadAffinityAllGroupCores(HANDLE handle)
{
    /* hard way */
    
    if (*(DWORD*)(0x7FFE0000 + 0x260) > 19045) /* Windows 10 22H2 */
    {
        std::vector<PROCESSOR_NUMBER> procs;
        procs.reserve(__g_ProcGroupCount);
        //MessageBoxA(0, std::to_string(__g_ProcLogicalThreadCount).c_str(), "Some Title", MB_ICONERROR | MB_OK);
        //MessageBoxA(0, std::to_string(__g_ProcGroupCount).c_str(), "Some Title", MB_ICONERROR | MB_OK);
        for (int i = 0; i <= __g_ProcGroupCount; i++)
        {
            PROCESSOR_NUMBER n = {};
            n.Group = i;
            n.Number = 63; //% MaxLogicalProcessorsPerGroup; //(__g_ProcLogicalThreadCount / __g_ProcGroupCount + 1); // % MaxLogicalProcessorsPerGroup;
            procs.push_back(n);
        }

        auto first = procs.front();
        auto last = procs.back();

        //if (first.Group != last.Group) {
            // do nothing; if we're running on Windows 11 or Server 2022, we're
            // already a multi-group process, so leave it at that
        //    return;
        //}

        GROUP_AFFINITY groupAffinity = { .Mask = KAFFINITY(-1), .Group = first.Group };
        if (last.Number != MaxLogicalProcessorsPerGroup) {
            groupAffinity.Mask = KAFFINITY(1) << (last.Number + 1 - first.Number);
            groupAffinity.Mask -= 1;
        }
        groupAffinity.Mask <<= first.Number;

        if (SetThreadGroupAffinity(handle, &groupAffinity, nullptr) == 0) {
            //win32_perror("SetThreadGroupAffinity");
            return;
        }

        if (SetThreadGroupAffinity(GetCurrentThread(), &groupAffinity, nullptr) == 0) {
            //win32_perror("SetThreadGroupAffinity");
            return;
        }
    }
    else
    {
        SetThreadIdealProcessor(handle, MAXIMUM_PROCESSORS);
        //SetThreadAffinityMask(handle, maskAllCores);

        // HACK: set thread affinity for main thread
        SetThreadIdealProcessor(GetCurrentThread(), MAXIMUM_PROCESSORS);
        //SetThreadAffinityMask(GetCurrentThread(), maskAllCores);
        //SetProcessAffinityMask(GetCurrentProcess(), maskAllCores);
    }
}