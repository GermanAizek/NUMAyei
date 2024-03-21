
#include "minhook\include\MinHook.h"
#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

#include "framework.h"
#include <thread>

/*
namespace std::thread
{
    typedef unsigned int (*hardware_concurrency)() noexcept;
    hardware_concurrency fp_hardware_concurrency = NULL;
}
*/


/* Hooking and extending CreateThread's capabilities to all logical processors on Windows system using SetThreadAffinity function */
typedef HANDLE (WINAPI *CreateThread)(
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ __drv_aliasesMem LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
);
CreateThread fp_CreateThread = NULL;

// This implementation supports both conventional single-cpu PC configurations
// and multi-cpu system on NUMA (Non-uniform_memory_access) architecture
// Modified by GermanAizek
unsigned int GetThreadCount() noexcept
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

// Many thanks x64dbg project
static DWORD BridgeGetNtBuildNumberWindows7()
{
    auto p_RtlGetVersion = (NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    RTL_OSVERSIONINFOW info = { sizeof(info) };
    if (p_RtlGetVersion && p_RtlGetVersion(&info) == 0)
        return info.dwBuildNumber;
    else
        return 0;
}

unsigned int BridgeGetNtBuildNumber()
{
    // https://www.vergiliusproject.com/kernels/x64/Windows%2010%20%7C%202016/1507%20Threshold%201/_KUSER_SHARED_DATA
    auto NtBuildNumber = *(DWORD*)(0x7FFE0000 + 0x260);
    if (NtBuildNumber == 0)
    {
        // Older versions of Windows
        static DWORD NtBuildNumber7 = BridgeGetNtBuildNumberWindows7();
        NtBuildNumber = NtBuildNumber7;
    }
    return NtBuildNumber;
}

void setThreadAffinityAllGroupCores(HANDLE handle)
{
    const auto threads = GetThreadCount();
    if (threads >= 64 && BridgeGetNtBuildNumber() <= 19045 /* Windows 10 22H2 */)
    {
        SetThreadAffinityMask(handle, (1 << threads) - 1);
    }
}

HANDLE WINAPI DetourCreateThread(
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ __drv_aliasesMem LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
)
{
    // setThreadAffinityAllGroupCores
    return fp_CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

BOOL InitCreateEnableHooks(LPCSTR nameFunction, LPVOID detour, LPVOID original)
{
    /* WINAPI hook
    if (MH_CreateHookApiEx(L"user32", "GetSystemMetrics", &DetourGetSystemMetrics, &fpGetSystemMetrics) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create GetSystemMetrics hook", L"NUMAYei", MB_OK);
        return TRUE;
    }
    */
    
    // hook which overrides std::thread::hardware::concurrency
    if (MH_CreateHook(&Function, &DetourFunction,
        reinterpret_cast<LPVOID*>(&fpFunction)) != MH_OK)
    {
        return 1;
    }

    if (MH_EnableHook(&GetSystemMetrics) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook GetSystemMetrics", L"NUMAYei", MB_OK);
        return TRUE;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    if (!IsNUMA())
    {
        MessageBoxW(NULL, L"Failed to DLL injection: your system configuration is not multiprocessor NUMA (Hint: maybe you forgot turn on NUMA in BIOS/UEFI)", L"NUMAYei", MB_OK);
        return FALSE;
    }

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        if (MH_Initialize() != MH_OK) {
            MessageBoxW(NULL, L"Failed to initialize MinHook", L"HideTS", MB_OK);
            return TRUE;
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        if (MH_Uninitialize() != MH_OK)
        {
            return TRUE;
        }
        break;
    }
    return TRUE;
}

