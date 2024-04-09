#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>
#include <thread>
#include <algorithm>
#include <vector>

//#include "uv.h"

static constexpr unsigned MaxLogicalProcessorsPerGroup =
std::numeric_limits<KAFFINITY>::digits;

bool IsNUMA() noexcept
{
    ULONG HighestNodeNumber;
    return !(!GetNumaHighestNodeNumber(&HighestNodeNumber) || HighestNodeNumber == 0);
}

int __g_ProcGroupCount = 0;
int __g_ProcLogicalThreadCount = 0;
int __g_ProcSelectedForThread = 0;
PVOID* __g_VirtAllocBuffers = NULL;

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

void setThreadAffinityAllGroupCores(HANDLE handle) noexcept
{
    /* After Windows 10 22H2, include Windows 11 not needed set thread affinity */
    if (*(DWORD*)(0x7FFE0000 + 0x260) <= 19045) /* Windows 10 22H2 */
    {
        //SetThreadIdealProcessor(handle, MAXIMUM_PROCESSORS);
        SetThreadAffinityMask(handle, (1 << __g_ProcLogicalThreadCount) - 1);

        // HACK: set thread affinity for main thread
        //SetThreadIdealProcessor(GetCurrentThread(), MAXIMUM_PROCESSORS);
        SetThreadAffinityMask(GetCurrentThread(), (1 << __g_ProcLogicalThreadCount) - 1);
    }
    //MessageBoxW(NULL, L"Crash", L"NUMAYei", MB_OK);
}

void setThreadParallelAllNUMAGroups(HANDLE handle) noexcept
{
    auto nodes = std::make_unique<GROUP_AFFINITY[]>(__g_ProcGroupCount);
    for (int i = 0; i < __g_ProcGroupCount; ++i)
    {
        nodes[i].Group = i;
        nodes[i].Mask = (ULONG_PTR(1) << (__g_ProcLogicalThreadCount / __g_ProcGroupCount)) - 1;
    }
    SetThreadSelectedCpuSetMasks(handle, nodes.get(), __g_ProcGroupCount);

    // parallel threads using global var scheduler
    /* not effective
    PROCESSOR_NUMBER proc;
    proc.Group = __g_ProcSelectedForThread;
    proc.Number = __g_ProcLogicalThreadCount / __g_ProcGroupCount;
    //MessageBoxW(NULL, std::to_wstring(__g_ProcLogicalThreadCount).c_str(), L"NUMAYei", MB_OK);
    SetThreadIdealProcessorEx(handle, &proc, NULL);
    */

    if (__g_ProcSelectedForThread < __g_ProcGroupCount)
        ++__g_ProcSelectedForThread;
    else
        __g_ProcSelectedForThread = 0;
}

LPVOID virtualAllocNUMA(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize)
{
    //
    // Allocate array of pointers to memory blocks.
    //

    __g_VirtAllocBuffers = (PVOID*)malloc(sizeof(PVOID) * __g_ProcLogicalThreadCount);

    if (__g_VirtAllocBuffers == NULL)
    {
        //_putts(_T("Allocating array of buffers failed"));
        //goto Exit;
    }

    ZeroMemory(__g_VirtAllocBuffers, sizeof(PVOID) * __g_ProcLogicalThreadCount);

    //
    // For each processor, get its associated NUMA node and allocate some memory from it.
    //

    for (UINT i = 0; i < __g_ProcLogicalThreadCount; ++i)
    {
        UCHAR NodeNumber;

        if (!GetNumaProcessorNode(i, &NodeNumber))
        {
            //assert(false);
        }

        auto data = VirtualAllocExNuma(hProcess, lpAddress, dwSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NodeNumber);

        __g_VirtAllocBuffers[i] = data;

        // full memory call below will touch every page in the buffer, faulting them into our working set.
        FillMemory(data, dwSize, 'x');
    }

    return __g_VirtAllocBuffers[__g_ProcLogicalThreadCount]; // TODO: it will be necessary to test which block allocated memory is better to return back
}

BOOL virtualFreeNUMA()
{
    if (__g_VirtAllocBuffers != NULL)
    {
        for (UINT i = 0; i < __g_ProcLogicalThreadCount; ++i)
        {
            if (__g_VirtAllocBuffers[i] != NULL)
            {
                VirtualFree(__g_VirtAllocBuffers[i], 0, MEM_RELEASE);
            }
        }

        free(__g_VirtAllocBuffers);
        return TRUE;
    }

    return FALSE;
}