
#include "minhook\include\MinHook.h"
#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

#include "framework.h"
#include <thread>

namespace std::thread
{
    typedef unsigned int (*hardware_concurrency)() noexcept;
    hardware_concurrency fp_hardware_concurrency = NULL;
}

bool IsNUMA() noexcept
{
    ULONG HighestNodeNumber;
    return !(!GetNumaHighestNodeNumber(&HighestNodeNumber) || HighestNodeNumber == 0);
}

BOOL InitCreateEnableHooks(LPCSTR nameFunction, LPVOID detour, LPVOID original)
{
    if (MH_CreateHookApiEx(L"user32", "GetSystemMetrics", &DetourGetSystemMetrics, &fpGetSystemMetrics) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to create GetSystemMetrics hook", L"NUMAYei", MB_OK);
        return TRUE;
    }

    if (MH_EnableHook(&GetSystemMetrics) != MH_OK)
    {
        MessageBoxW(NULL, L"Failed to enable hook GetSystemMetrics", L"NUMAYei", MB_OK);
        return TRUE;
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    if (!IsNUMA())
    {
        MessageBoxW(NULL, L"Failed to DLL injection: your system configuration is not multiprocessor NUMA (Hint: maybe you forgot turn on NUMA in BIOS/UEFI)", L"NUMAYei", MB_OK);
        return FALSE;
    }

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

