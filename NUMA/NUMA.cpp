
#include <Windows.h>
#include <shlobj_core.h>
#include <iostream>
#include <tchar.h>
#include <TlHelp32.h>
#include <iomanip>
#include <Shlwapi.h>
#include <psapi.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Psapi")

#define print(format, ...) fprintf (stderr, format, __VA_ARGS__)

int MessageBoxPrintfW(const TCHAR* msgBoxTitle, const TCHAR* msgBoxFormat, ...)
{
    va_list args;
    va_start(args, msgBoxFormat);

    int len = _vsctprintf(msgBoxFormat, args) + 1; // add terminating '\0'
    TCHAR* buf = new TCHAR[len];
    _vstprintf_s(buf, len, msgBoxFormat, args);

    int result = MessageBox(NULL, buf, msgBoxTitle, MB_OK);

    delete[]buf;

    return result;
}

DWORD GetProcId(std::wstring pn, unsigned short int fi = 0b1101)
{
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pE;
        pE.dwSize = sizeof(pE);

        if (Process32First(hSnap, &pE))
        {
            if (!pE.th32ProcessID)
                Process32Next(hSnap, &pE);
            do
            {
                if (fi == 0b10100111001)
                    std::cout << pE.szExeFile << u8"\x9\x9\x9" << pE.th32ProcessID << std::endl;
                if (!_wcsicmp(pE.szExeFile, pn.c_str()))
                {
                    procId = pE.th32ProcessID;
                    print("Process : 0x%lX\n", pE);
                    CloseHandle(hSnap);
                    return procId;
                    //break;
                }
            } while (Process32Next(hSnap, &pE));
        }
    }
    CloseHandle(hSnap);
    return procId;
}


BOOL InjectDLL(DWORD procID, const wchar_t* dllPath)
{
    BOOL WPM = 0;

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS | PROCESS_VM_WRITE, 0, procID);
    if (hProc == INVALID_HANDLE_VALUE)
    {
        print("OpenProcess failed process is INVALID_HANDLE_VALUE!");
        return -1;
    }
    void* loc = VirtualAllocEx(hProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WPM = WriteProcessMemory(hProc, loc, dllPath, wcslen(dllPath) + 1, 0);
    if (!WPM)
    {
        print("WriteProcessMemory failed! GetLastError = %d", GetLastError());
        CloseHandle(hProc);
        return -1;
    }
    print("DLL Injected Succesfully 0x%lX\n", WPM);
    HANDLE hThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, loc, 0, 0);
    if (!hThread)
    {
        VirtualFree(loc, wcslen(dllPath) + 1, MEM_RELEASE);
        CloseHandle(hProc);
        return -1;
    }
    print("Thread Created Succesfully 0x%lX\n", hThread);
    CloseHandle(hProc);
    VirtualFree(loc, wcslen(dllPath) + 1, MEM_RELEASE);
    CloseHandle(hThread);
    return 0;
}

void RegSet(HKEY hkeyHive, const wchar_t* pszVar, const char* pszValue)
{

    HKEY hkey;

    char szValueCurrent[1000];
    DWORD dwType;
    DWORD dwSize = sizeof(szValueCurrent);

    int iRC = RegGetValue(hkeyHive, pszVar, NULL, RRF_RT_ANY, &dwType, szValueCurrent, &dwSize);

    bool bDidntExist = iRC == ERROR_FILE_NOT_FOUND;

    if (iRC != ERROR_SUCCESS && !bDidntExist)
        MessageBoxPrintfW(L"NUMAYei", L"RegGetValue( %s ): %s", pszVar, strerror(iRC));

    if (!bDidntExist) {
        if (dwType != REG_SZ)
            MessageBoxPrintfW(L"NUMAYei", L"RegGetValue( %s ) found type unhandled %d", pszVar, dwType);

        if (strcmp(szValueCurrent, pszValue) == 0) {
            MessageBoxPrintfW(L"NUMAYei", L"RegSet( \"%s\" \"%s\" ): already correct", pszVar, pszValue);
            return;
        }
    }

    DWORD dwDisposition;
    iRC = RegCreateKeyEx(hkeyHive, pszVar, 0, 0, 0, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition);
    if (iRC != ERROR_SUCCESS)
        MessageBoxPrintfW(L"NUMAYei", L"RegCreateKeyEx( %s ): %s", pszVar, strerror(iRC));

    iRC = RegSetValueEx(hkey, L"", 0, REG_SZ, (BYTE*)pszValue, strlen(pszValue) + 1);
    if (iRC != ERROR_SUCCESS)
        MessageBoxPrintfW(L"NUMAYei", L"RegSetValueEx( %s ): %s", pszVar, strerror(iRC));

    if (bDidntExist)
        MessageBoxPrintfW(L"NUMAYei", L"RegSet( %s ): set to \"%s\"", pszVar, pszValue);
    else
        MessageBoxPrintfW(L"NUMAYei", L"RegSet( %s ): changed \"%s\" to \"%s\"", pszVar, szValueCurrent, pszValue);

    RegCloseKey(hkey);
}

int SetUpRegistry()
{

    // App doesn't have permission for this when run as normal user, but may for Admin? Anyway, not needed.
    // RegSet( HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\MoselleIDE.exe", "C:\\Moselle\\bin\\MoselleIDE.exe" );

    RegSet(HKEY_CURRENT_USER, L"Software\\Classes\\.moselle", "Moselle.MoselleIDE.1");

    // Not needed.
    RegSet(HKEY_CURRENT_USER, L"Software\\Classes\\.moselle\\Content Type", "text/plain");
    RegSet(HKEY_CURRENT_USER, L"Software\\Classes\\.moselle\\PerceivedType", "text");

    // Not needed, but may be be a way to have wordpad show up on the default list.
    // RegSet( HKEY_CURRENT_USER, "Software\\Classes\\.moselle\\OpenWithProgIds\\Moselle.MoselleIDE.1", "" );

    RegSet(HKEY_CURRENT_USER, L"Software\\Classes\\Moselle.MoselleIDE.1", "Moselle IDE");

    RegSet(HKEY_CURRENT_USER, L"Software\\Classes\\Moselle.MoselleIDE.1\\Shell\\Open\\Command", "C:\\Moselle\\bin\\MoselleIDE.exe %1");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

DWORD startup(std::wstring path)
{
    // additional information
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    // set the size of the structures
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // start the program up
    CreateProcessW
    (
        NULL,   // the path
        const_cast<wchar_t*>(path.c_str()),                // Command line
        NULL,                   // Process handle not inheritable
        NULL,                   // Thread handle not inheritable
        FALSE,                  // Set handle inheritance to FALSE
        CREATE_NEW_CONSOLE,     // Opens file in a separate console
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi           // Pointer to PROCESS_INFORMATION structure
    );

    Sleep(100);

    DWORD pid = pi.dwProcessId;

    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return pid;
}

int wmain(int argc, wchar_t** argv)
{
    std::wstring pname = argv[1];
    auto dllpath = L"NUMADLL.dll";

    if (PathFileExists(dllpath) == FALSE)
    {
        print("DLL File does NOT exist!");
        return EXIT_FAILURE;
    }

    DWORD procId = startup(pname); // begin start process and inject
    
    //procId = GetProcId(pname.c_str()); // inject to exist process

    print("Process ID [ %d ] attempt injected ...", procId);
    
    if (procId == NULL)
    {
        print("Process Not found (0x%lX)\n", GetLastError());
        print("Here is a list of available process \n", GetLastError());
        Sleep(3500);
        system("cls");
        GetProcId(L"skinjbir", 0b10100111001);
    }
    else
        InjectDLL(procId, dllpath);
}
