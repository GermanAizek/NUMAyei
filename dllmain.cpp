
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
    auto thread = fp_CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags | INHERIT_PARENT_AFFINITY, lpThreadId);
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
    auto thread = fp_CreateRemoteThread(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags | INHERIT_PARENT_AFFINITY, lpThreadId);
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
    auto* pAttribs = lpAttributeList;
    GROUP_AFFINITY GrpAffinity = { 0 };
    GrpAffinity.Mask = 1;
    UpdateProcThreadAttribute(pAttribs, 0, PROC_THREAD_ATTRIBUTE_GROUP_AFFINITY, &GrpAffinity, sizeof(GrpAffinity), NULL, NULL);
    auto thread = fp_CreateRemoteThreadEx(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags | INHERIT_PARENT_AFFINITY, pAttribs, lpThreadId);
    setThreadAffinityAllGroupCores(thread);
    return thread;
}

BOOL InitCreateEnableHooks(LPCSTR nameFunction, LPVOID detour, LPVOID original)
{
    // WINAPI hook
    if (MH_CreateHookApiEx(L"user32", "CreateThread", &DetourCreateThread, reinterpret_cast<LPVOID*>(&fp_CreateThread), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create CreateThread hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"user32", "CreateRemoteThread", &DetourCreateRemoteThread, reinterpret_cast<LPVOID*>(&fp_CreateRemoteThread), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create CreateRemoteThread hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_CreateHookApiEx(L"user32", "CreateRemoteThreadEx", &DetourCreateRemoteThreadEx, reinterpret_cast<LPVOID*>(&fp_CreateRemoteThreadEx), NULL) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create CreateRemoteThreadEx hook", L"NUMAYei", MB_OK);
        return TRUE;
    }
    
    // hook which overrides std::thread::hardware::concurrency
    //if (MH_CreateHook(&Function, &DetourFunction,
    //    reinterpret_cast<LPVOID*>(&fpFunction)) != MH_OK)
    //{
    //    return 1;
    //}

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

