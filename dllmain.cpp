
#include "minhook\include\MinHook.h"
#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

#include "framework.h"

/*
namespace std::thread
{
    typedef unsigned int (*hardware_concurrency)() noexcept;
    hardware_concurrency fp_hardware_concurrency = NULL;
}
*/


/* Hooking and extending CreateThread's capabilities to all logical processors on Windows system using SetThreadAffinity function */
typedef HANDLE (WINAPI *HCreateThread)(
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ __drv_aliasesMem LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
);
HCreateThread fp_CreateThread = NULL;

typedef HANDLE(WINAPI* HCreateRemoteThread)(
    _In_ HANDLE hProcess,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
    );
HCreateRemoteThread fp_CreateRemoteThread = NULL;

typedef HANDLE (WINAPI* HCreateRemoteThreadEx)(
    _In_ HANDLE hProcess,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
    _Out_opt_ LPDWORD lpThreadId
);
HCreateRemoteThreadEx fp_CreateRemoteThreadEx = NULL;

/* Hooking and extending GetSystemInfo */
typedef VOID (WINAPI* HGetSystemInfo)(
    _Out_ LPSYSTEM_INFO lpSystemInfo
);
HGetSystemInfo fp_GetSystemInfo = NULL;

typedef DWORD (WINAPI* HGetActiveProcessorCount)(
    _In_ WORD GroupNumber
);
HGetActiveProcessorCount fp_GetActiveProcessorCount = NULL;

typedef unsigned int (*Hhardware_concurrency)();
Hhardware_concurrency fp_hardware_concurrency = NULL;

HANDLE WINAPI DetourCreateThread(
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ __drv_aliasesMem LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
)
{
    // set Thread Affinity All Group Logical Cores its effi
    auto thread = fp_CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags /* | INHERIT_PARENT_AFFINITY */, lpThreadId);
    setThreadAffinityAllGroupCores(thread);
    return thread;
}

HANDLE WINAPI DetourCreateRemoteThread(
    _In_ HANDLE hProcess,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
)
{
    auto thread = fp_CreateRemoteThread(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags /* | INHERIT_PARENT_AFFINITY */, lpThreadId);
    setThreadAffinityAllGroupCores(thread);
    return thread;
}

HANDLE WINAPI DetourCreateRemoteThreadEx(
    _In_ HANDLE hProcess,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
    _Out_opt_ LPDWORD lpThreadId
)
{
    const auto threads = GetLogicalThreadCount();
    KAFFINITY maskAllCores = (1 << threads) - 1;

    auto* pAttribs = lpAttributeList;
    GROUP_AFFINITY GrpAffinity = { 0 };
    GrpAffinity.Mask = maskAllCores;
    UpdateProcThreadAttribute(pAttribs, 0, PROC_THREAD_ATTRIBUTE_GROUP_AFFINITY, &GrpAffinity, sizeof(GrpAffinity), NULL, NULL);
    auto thread = fp_CreateRemoteThreadEx(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags /* | INHERIT_PARENT_AFFINITY */, pAttribs, lpThreadId);
    //setThreadAffinityAllGroupCores(thread);
    return thread;
}

VOID WINAPI DetourGetSystemInfo(
    _Out_ LPSYSTEM_INFO lpSystemInfo
)
{
    lpSystemInfo->dwNumberOfProcessors = GetLogicalThreadCount();
    fp_GetSystemInfo(lpSystemInfo);
}

DWORD WINAPI DetourGetActiveProcessorCount(
    _In_ WORD GroupNumber
)
{
    //GroupNumber = ALL_PROCESSOR_GROUPS;
    //return fp_GetActiveProcessorCount(GroupNumber);
    return GetLogicalThreadCount();
}

unsigned int DetourHardware_concurrency()
{
    //return fp_hardware_concurrency();
    return GetLogicalThreadCount();
}

BOOL InitCreateEnableHooks(LPCSTR nameFunction, LPVOID detour, LPVOID original)
{
    // WINAPI hook
    if (MH_CreateHookApiEx(L"kernel32", "CreateThread", &DetourCreateThread, reinterpret_cast<LPVOID*>(&fp_CreateThread), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create CreateThread hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"kernel32", "CreateRemoteThread", &DetourCreateRemoteThread, reinterpret_cast<LPVOID*>(&fp_CreateRemoteThread), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create CreateRemoteThread hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"kernel32", "CreateRemoteThreadEx", &DetourCreateRemoteThreadEx, reinterpret_cast<LPVOID*>(&fp_CreateRemoteThreadEx), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create CreateRemoteThreadEx hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"kernel32", "GetSystemInfo", &DetourGetSystemInfo, reinterpret_cast<LPVOID*>(&fp_GetSystemInfo), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create GetSystemInfo hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"kernel32", "GetActiveProcessorCount", &DetourGetActiveProcessorCount, reinterpret_cast<LPVOID*>(&fp_GetActiveProcessorCount), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create GetActiveProcessorCount hook", L"NUMAYei", MB_OK);
        return TRUE;
    }
    
    // hook which overrides std::thread::hardware::concurrency
    if (MH_CreateHook(&std::thread::hardware_concurrency, &DetourHardware_concurrency,
        reinterpret_cast<LPVOID*>(&fp_hardware_concurrency)) != MH_OK)
    {
        return TRUE;
    }

    if (MH_EnableHook(&CreateThread) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook CreateThread", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_EnableHook(&CreateRemoteThread) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook CreateRemoteThread", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_EnableHook(&CreateRemoteThreadEx) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook CreateRemoteThreadEx", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_EnableHook(&GetSystemInfo) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook GetSystemInfo", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_EnableHook(&GetActiveProcessorCount) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook GetActiveProcessorCount", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_EnableHook(&std::thread::hardware_concurrency) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook std::thread::hardware_concurrency", L"NUMAYei", MB_OK);
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

