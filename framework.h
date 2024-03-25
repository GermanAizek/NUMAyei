#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <winbase.h>
#include <memory>
#include <thread>
#include <algorithm>

bool IsNUMA() noexcept
{
    ULONG HighestNodeNumber;
    return !(!GetNumaHighestNodeNumber(&HighestNodeNumber) || HighestNodeNumber == 0);
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
    // old variant
    const auto threads = GetLogicalThreadCount();
    KAFFINITY maskAllCores = (1 << threads) - 1;

    auto node = static_cast<PGROUP_AFFINITY>(malloc(sizeof(PGROUP_AFFINITY)));
    //auto aff = node;
    //memset(&node, 0, sizeof(node));
    //node->Group = group;
    node->Mask = maskAllCores;
    SetThreadGroupAffinity(handle, node, NULL);

    // HACK: set thread affinity for main thread
    SetThreadGroupAffinity(GetCurrentThread(), node, NULL);

    /* hard way, behavior is same*/
    /*
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &length);

    std::unique_ptr<void, void (*)(void*)> buffer(std::malloc(length), std::free);

    auto* mem = reinterpret_cast<unsigned char*>(buffer.get());
    GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem), &length);
    
    DWORD i = 0;
    while (i < length) {
        const auto* proc = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem + i);
        if (proc->Relationship == RelationProcessorCore) {
            for (WORD group = 0; group < proc->Processor.GroupCount; ++group) {
                auto node = static_cast<PGROUP_AFFINITY>(malloc(sizeof(PGROUP_AFFINITY)));
                //auto aff = node;
                //memset(&node, 0, sizeof(node));
                node->Group = group;
                node->Mask = maskAllCores;
                SetThreadGroupAffinity(handle, node, NULL);

                // HACK: set thread affinity for main thread
                SetThreadGroupAffinity(GetCurrentThread(), node, NULL);

                //GetNumaNodeProcessorMaskEx(group, node);
                //maskAllCores += node->Mask;
                //break;
            }
        }
    }
    */
    /*
    SetThreadAffinityMask(handle, maskAllCores);

    // HACK: set thread affinity for main thread
    SetThreadAffinityMask(GetCurrentThread(), maskAllCores);
    SetProcessAffinityMask(GetCurrentProcess(), maskAllCores);
    */
}