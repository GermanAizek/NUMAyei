
#include "minhook\include\MinHook.h"
#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

#include "framework.h"

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

typedef DWORD (WINAPI* HGetEnvironmentVariableA)(
    _In_opt_ LPCSTR lpName,
    _Out_writes_to_opt_(nSize, return + 1) LPSTR lpBuffer,
    _In_ DWORD nSize
);
HGetEnvironmentVariableA fp_GetEnvironmentVariableA = NULL;

typedef DWORD (WINAPI* HGetEnvironmentVariableW)(
    _In_opt_ LPCWSTR lpName,
    _Out_writes_to_opt_(nSize, return + 1) LPWSTR lpBuffer,
    _In_ DWORD nSize
    );
HGetEnvironmentVariableW fp_GetEnvironmentVariableW = NULL;

typedef LPVOID (WINAPI* HVirtualAllocEx)(
    _In_ HANDLE hProcess,
    _In_opt_ LPVOID lpAddress,
    _In_ SIZE_T dwSize,
    _In_ DWORD flAllocationType,
    _In_ DWORD flProtect
);
HVirtualAllocEx fp_VirtualAllocEx = NULL;

typedef BOOL (WINAPI* HVirtualFreeEx)(
    _In_ HANDLE hProcess,
    _Pre_notnull_ _When_(dwFreeType == MEM_DECOMMIT, _Post_invalid_) _When_(dwFreeType == MEM_RELEASE, _Post_ptr_invalid_) LPVOID lpAddress,
    _In_ SIZE_T dwSize,
    _In_ DWORD dwFreeType
);
HVirtualFreeEx fp_VirtualFreeEx = NULL;

typedef unsigned int (*Hhardware_concurrency)();
Hhardware_concurrency fp_hardware_concurrency = NULL;

/* Detours functions */
HANDLE WINAPI DetourCreateThread(
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ __drv_aliasesMem LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
)
{
    auto thread = fp_CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags /* | INHERIT_PARENT_AFFINITY */, lpThreadId);
    setThreadAffinityAllGroupCores(thread);
    setThreadParallelAllNUMAGroups(thread);

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
    setThreadParallelAllNUMAGroups(thread);

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
    //auto* pAttribs = lpAttributeList;
    //DWORD node = __g_ProcSelectedForThread;
    //UpdateProcThreadAttribute(pAttribs, 0, PROC_THREAD_ATTRIBUTE_PREFERRED_NODE, &node, sizeof(DWORD), NULL, NULL);
    auto thread = fp_CreateRemoteThreadEx(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags /* | INHERIT_PARENT_AFFINITY */, lpAttributeList, lpThreadId);
    setThreadAffinityAllGroupCores(thread);
    setThreadParallelAllNUMAGroups(thread);

    return thread;
}

VOID WINAPI DetourGetSystemInfo(
    _Out_ LPSYSTEM_INFO lpSystemInfo
)
{
    // MessageBoxA(0, "DetourGetSystemInfo", "Some Title", MB_ICONERROR | MB_OK);
    lpSystemInfo->dwNumberOfProcessors = __g_ProcLogicalThreadCount;
    return fp_GetSystemInfo(lpSystemInfo);
}

DWORD WINAPI DetourGetActiveProcessorCount(
    _In_ WORD GroupNumber
)
{
    GroupNumber = ALL_PROCESSOR_GROUPS;
    return fp_GetActiveProcessorCount(GroupNumber);
}

DWORD WINAPI DetourGetEnvironmentVariableA(
    _In_opt_ LPCSTR lpName,
    _Out_writes_to_opt_(nSize, return +1) LPSTR lpBuffer,
    _In_ DWORD nSize
)
{
    if (lpName == "NUMBER_OF_PROCESSORS")
        _itoa(__g_ProcLogicalThreadCount, lpBuffer, 10);
    return fp_GetEnvironmentVariableA(lpName, lpBuffer, nSize);
}

DWORD WINAPI DetourGetEnvironmentVariableW(
    _In_opt_ LPCWSTR lpName,
    _Out_writes_to_opt_(nSize, return +1) LPWSTR lpBuffer,
    _In_ DWORD nSize
)
{
    if (lpName == L"NUMBER_OF_PROCESSORS")
        _itow(__g_ProcLogicalThreadCount, lpBuffer, 10);
    return fp_GetEnvironmentVariableW(lpName, lpBuffer, nSize);
}

LPVOID WINAPI DetourVirtualAllocEx(
    _In_ HANDLE hProcess,
    _In_opt_ LPVOID lpAddress,
    _In_ SIZE_T dwSize,
    _In_ DWORD flAllocationType,
    _In_ DWORD flProtect
)
{
    return virtualAllocNUMA(hProcess, lpAddress, dwSize);
}

BOOL WINAPI DetourVirtualFreeEx(
    _In_ HANDLE hProcess,
    _Pre_notnull_ _When_(dwFreeType == MEM_DECOMMIT, _Post_invalid_) _When_(dwFreeType == MEM_RELEASE, _Post_ptr_invalid_) LPVOID lpAddress,
    _In_ SIZE_T dwSize,
    _In_ DWORD dwFreeType
)
{
    return virtualFreeNUMA();
}

unsigned int DetourHardware_concurrency()
{
    // MessageBoxA(0, "DetourHardware_concurrency", "Some Title", MB_ICONERROR | MB_OK);
    //return fp_hardware_concurrency();
    return __g_ProcLogicalThreadCount;
}

BOOL InitCreateEnableHooks()
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
    /* dont uncomment - this broken ideal sheduler
    if (MH_CreateHookApiEx(L"kernel32", "CreateRemoteThreadEx", &DetourCreateRemoteThreadEx, reinterpret_cast<LPVOID*>(&fp_CreateRemoteThreadEx), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create CreateRemoteThreadEx hook", L"NUMAYei", MB_OK);
        return TRUE;
    }
    */
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

    if (MH_CreateHookApiEx(L"kernel32", "GetEnvironmentVariableA", &DetourGetEnvironmentVariableA, reinterpret_cast<LPVOID*>(&fp_GetEnvironmentVariableA), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create GetEnvironmentVariableA hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"kernel32", "GetEnvironmentVariableW", &DetourGetEnvironmentVariableW, reinterpret_cast<LPVOID*>(&fp_GetEnvironmentVariableW), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create GetEnvironmentVariableW hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"kernel32", "VirtualAllocEx", &DetourVirtualAllocEx, reinterpret_cast<LPVOID*>(&fp_VirtualAllocEx), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create VirtualAllocEx hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"kernel32", "VirtualFreeEx", &DetourVirtualFreeEx, reinterpret_cast<LPVOID*>(&fp_VirtualFreeEx), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create VirtualFreeEx hook", L"NUMAYei", MB_OK);
        return TRUE;
    }
    
    // hook which overrides std::thread::hardware::concurrency
    if (MH_CreateHook(&std::thread::hardware_concurrency, &DetourHardware_concurrency,
        reinterpret_cast<LPVOID*>(&fp_hardware_concurrency)) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create std::thread::hardware_concurrency hook", L"NUMAYei", MB_OK);
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
    /*
    if (MH_EnableHook(&CreateRemoteThreadEx) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook CreateRemoteThreadEx", L"NUMAYei", MB_OK);
        return TRUE;
    }
    */
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
        __g_ProcGroupCount = calcLogicalGroups() + 1;
        __g_ProcLogicalThreadCount = GetLogicalThreadCount();
        InitCreateEnableHooks();
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

