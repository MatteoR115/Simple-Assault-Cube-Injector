#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <commdlg.h>
#include <ShObjIdl.h>

// Function to open file dialog and get the DLL path
bool OpenFileDialog(wchar_t* path, size_t pathSize) {
    IFileDialog* pfd;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog, reinterpret_cast<void**>(&pfd)))) {
        pfd->SetTitle(L"Select the DLL");
        DWORD options;
        pfd->GetOptions(&options);
        pfd->SetOptions(options | FOS_FORCEFILESYSTEM);
        if (SUCCEEDED(pfd->Show(NULL))) {
            IShellItem* psi;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR pszPath;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    wcsncpy_s(path, pathSize, pszPath, pathSize - 1);
                    CoTaskMemFree(pszPath);
                    psi->Release();
                    pfd->Release();
                    return true;
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    return false;
}

DWORD GetProcId(const wchar_t* procName) {
    DWORD pId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &procEntry)) {
            do {
                if (!_wcsicmp(procEntry.szExeFile, procName)) {
                    pId = procEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &procEntry));
        }
        CloseHandle(hSnap);
    }
    return pId;
}

int main() {
    std::wcout << L"Welcome to the Assault Cube Injector!\n\n";
    Sleep(500);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"Failed to initialize COM library. Error: " << std::hex << hr << L"\n";
        return 1;
    }

    wchar_t dllPath[MAX_PATH] = { 0 };
    if (!OpenFileDialog(dllPath, MAX_PATH)) {
        std::wcout << L"Failed to get DLL path. Exiting program...\n";
        CoUninitialize();
        return 1;
    }

    const wchar_t* procName = L"ac_client.exe";
    DWORD pId = GetProcId(procName);
    if (pId == 0) {
        std::wcout << L"No process found. Exiting program...\n";
        CoUninitialize();
        return 1;
    }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pId);
    if (!hProc || hProc == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open target process. Exiting program...\n";
        CoUninitialize();
        return 1;
    }

    void* loc = VirtualAllocEx(hProc, NULL, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!loc) {
        std::wcerr << L"Memory allocation failed in target process. Exiting program...\n";
        CloseHandle(hProc);
        CoUninitialize();
        return 1;
    }

    size_t size = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    if (!WriteProcessMemory(hProc, loc, dllPath, size, NULL)) {
        std::wcerr << L"Failed to write to process memory. Exiting program...\n";
        VirtualFreeEx(hProc, loc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        CoUninitialize();
        return 1;
    }

    if (!CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, loc, 0, NULL)) {
        std::wcerr << L"Injection failed. Exiting program...\n";
        VirtualFreeEx(hProc, loc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        CoUninitialize();
        return 1;
    }
    std::wcout << L"Successfully injected! Exiting program...\n\n";
    Sleep(1500);
    std::wcout;
}
