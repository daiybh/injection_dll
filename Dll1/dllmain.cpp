#include "pch.h"
#include <tchar.h>
#include <Windows.h>

#include <iostream>
//Handle of current process
HANDLE g_hProc;

//Backup of orignal code of target api
BYTE g_aBackup[6];
BYTE g_aOpcode[6];

//Critical section, prevent concurrency of calling the monitor
CRITICAL_SECTION g_cs;

//Base address of target API in DWORD
DWORD g_dwApiFunc = (DWORD)GetTickCount;

//Hook the target API
__inline BOOL MonitorBase(void)
{
    std::cout << " MonitorBase: in "<< std::endl;
    // Modify the heading 6 bytes opcode in target API to jmp instruction,
    // the jmp instruction will lead the EIP to our fake function
    ReadProcessMemory(g_hProc, LPVOID(g_dwApiFunc), LPVOID(g_aBackup),sizeof(g_aBackup) / sizeof(g_aBackup[0]), NULL); 
    printf("ReadProcessMemory g_aBackup %x %x %x %x %x %x "
        , g_aBackup[0]
        , g_aBackup[1]
        , g_aBackup[2]
        , g_aBackup[3]
        , g_aBackup[4]
        , g_aBackup[5]);
    printf("ReadProcessMemory g_aOpcode %x %x %x %x %x %x "
        , g_aOpcode[0]
        , g_aOpcode[1]
        , g_aOpcode[2]
        , g_aOpcode[3]
        , g_aOpcode[4]
        , g_aOpcode[5]);
    BOOL b= WriteProcessMemory(g_hProc, LPVOID(g_dwApiFunc),
        LPVOID(g_aOpcode), sizeof(g_aOpcode) / sizeof(g_aOpcode[0]), NULL);

    std::cout << " MonitorBase: out "<<b << std::endl;
    return b;
}

//Unhook the target API
__inline BOOL ReleaseBase(void)
{
    // Restore the heading 6 bytes opcode of target API.
    return WriteProcessMemory(g_hProc, LPVOID(g_dwApiFunc),
        LPVOID(g_aBackup), sizeof(g_aOpcode) / sizeof(g_aOpcode[0]), NULL);
}

//Pre-declare
BOOL UninstallMonitor(void);

//Monitor Function
DWORD WINAPI MonFunc()
{
    //Thread safety
    EnterCriticalSection(&g_cs);

    std::cout << "MonFunc in" << std::endl;
    //Restore the original API before calling it
    ReleaseBase();
    DWORD dw = GetTickCount();
    MonitorBase();

    //You can do anything here, and you can call the UninstallMonitor
    //when you want to leave.

    //Thread safety
    LeaveCriticalSection(&g_cs);

    std::cout << "MonFunc out" << std::endl;
    return dw;
}
//Install Monitor
BOOL InstallMonitor(void)
{
    //Get handle of current process
    g_hProc = GetCurrentProcess();
    std::cout << "GetCurrentProcess:" << g_hProc << std::endl;

    g_aOpcode[0] = 0xE9; //JMP Procudure
    *(DWORD*)(&g_aOpcode[1]) = (DWORD)MonFunc - g_dwApiFunc - 5;

    InitializeCriticalSection(&g_cs);

    //Start monitor
    bool b=  MonitorBase();

    std::cout << "InstallMonitor MonitorBase:" << b << std::endl;
    return b;
}

BOOL UninstallMonitor(void)
{
    //Release monitor
    if (!ReleaseBase())
        return FALSE;

    DeleteCriticalSection(&g_cs);

    CloseHandle(g_hProc);

    //Synchronize to main application, release semaphore to free injector
    HANDLE hSema = OpenSemaphore(EVENT_ALL_ACCESS, FALSE, _T("Global\\InjHack"));
    if (hSema == NULL)
        return FALSE;
    return ReleaseSemaphore(hSema, 1, (LPLONG)g_hProc);
}

DWORD WINAPI MonFunc1(void)
{
    MessageBox(NULL, _T("注入代码"), _T("示例"), 0);
    return 999999;
}
void InstallMonitor1(void)
{
    HANDLE hProc = GetCurrentProcess();
    BYTE aOpcode[5] = { 0xE9 }; //JMP Procudure
    *(DWORD*)(&aOpcode[1]) = DWORD(MonFunc1) - DWORD(GetTickCount) - 5;
    WriteProcessMemory(hProc, LPVOID(GetTickCount), LPVOID(aOpcode), 5, NULL);
    CloseHandle(hProc);
}

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);

        InstallMonitor1();
        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}